// Story rmf-017 (production/epics/round-match-framework/story-017-latched-final-complete-logged.md): GDD AC 63.
// GIVEN a final HoldTimer(Complete) that latches the round and a second final Complete in the same drain, THEN the
// second returns Rejected_Latched and the Event Log holds TendCompleted(ParticipantId, court, NoEffect) with
// Latched = true for it, so the objective-win share counts both attempts. The first (latching) completion is
// logged with Latched = false. In Exit A the latch comes from a stub resolver (the real TryTerminate is rmf-052);
// the owner writes the latched entry through IQueueResolver::OnRejectedLatched, inside the rejected item's slot.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthQueueTestRig.h"
#include "Server/Testing/StealthRoundLogTestSupport.h"

namespace LatchedFinalCompleteTestData
{
	/** The stub's "final objective" action type (the IIF owns real values). */
	constexpr uint8 FinalActionType = 9;

	FRoundLogEntry MakeTend(FQueueKey Key, double Time, uint32 Participant, ERoundTendResult Result, bool bLatched)
	{
		FRoundLogEntry Entry = RoundLogFixtures::MakeEntry(ERoundLogType::TendCompleted, Key, Time);
		Entry.Participants[0] = Participant;
		Entry.Code = static_cast<uint16>(Result);
		Entry.Data[0] = RoundTestFixtures::FinalCourtSocketId;
		Entry.Flags = bLatched ? RoundLogFlags::Latched : 0;
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundLatchedFinalCompleteLoggedTest, "StealthGame.Round.LatchedFinalCompleteLogged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundLatchedFinalCompleteLoggedTest::RunTest(const FString& Parameters)
{
	using namespace LatchedFinalCompleteTestData;
	using namespace RoundTestFixtures;

	// Arrange: the queue writes into the real Event Log.
	FStealthQueueTestRig Rig;
	FStealthRoundLogRig LogRig;
	FResolutionQueueDeps Deps;
	Deps.Clock = &Rig.Clock;
	Deps.SubSeeds = &Rig.Seeds;
	Deps.Epoch = &Rig.Epoch;
	Deps.Generations = &Rig.Generations;
	Deps.Log = &LogRig.Log;
	Deps.FaultSink = &Rig.Faults;
	FResolutionQueueCore Queue(Deps);

	// The stub owner of HoldTimer: a final Complete logs TendCompleted and, as the stand-in for Round's TryTerminate,
	// latches the round and writes WinLatched. A rejected final Complete logs TendCompleted(NoEffect, Latched).
	FStealthStubResolver Holds;
	Holds.OnResolve = [&](const FQueueItem& Item, FQueueKey Key)
	{
		const FHoldTimerPayload& Hold = Item.Payload.Get<FHoldTimerPayload>();
		const uint32 Completer = Hold.ActionEpoch;
		LogRig.Log.Append(MakeTend(Key, Rig.Clock.GetServerTimeSeconds(), Completer, ERoundTendResult::Counted, false));
		if (Hold.ActionType == FinalActionType && Queue.Latch(Key))
		{
			LogRig.Log.Append(RoundLogFixtures::MakeWinLatched(ERoundEndReason::ObjectiveWin, 0, Key, Rig.Clock.GetServerTimeSeconds()));
		}
		return EResolveResult::Executed;
	};
	Holds.OnLatched = [&](const FQueueItem& Item, FQueueKey Key)
	{
		const FHoldTimerPayload& Hold = Item.Payload.Get<FHoldTimerPayload>();
		if (Hold.Kind == EHoldTimerKind::Complete && Hold.ActionType == FinalActionType)
		{
			LogRig.Log.Append(MakeTend(Key, Rig.Clock.GetServerTimeSeconds(), Hold.ActionEpoch, ERoundTendResult::NoEffect, true));
		}
	};
	Queue.RegisterResolver(EQueueItemType::HoldTimer, &Holds);

	// Two final completions due in the same drain (equal DueTime: ordered by their Begin keys, A's first).
	const double Due = Rig.Clock.GetServerTimeSeconds() + 0.01;
	for (const TPair<uint32, uint64> Completer : { TPair<uint32, uint64>(FinalCompleterA, 1), TPair<uint32, uint64>(FinalCompleterB, 2) })
	{
		FHoldTimerPayload Payload;
		Payload.Kind = EHoldTimerKind::Complete;
		Payload.ActionType = FinalActionType;
		Payload.ActionEpoch = Completer.Key;  // the stub's completer id
		Payload.BeginKey = FQueueKey(1, Completer.Value);
		Queue.ScheduleAt(Due, FQueueItem::Make(EQueueItemType::HoldTimer, Rig.Epoch.Current, FMailboxId::ServerLocal(), Payload));
	}

	// Act.
	Rig.Clock.Advance(1.0 / 60.0, 1);
	Queue.Drain();

	// Assert: the second completion is Rejected_Latched and keeps its key (never re-stamped).
	const TArray<FQueueSlotRecord>& Records = Queue.GetLastDrainRecords();
	if (TestEqual(TEXT("two slots in the drain"), Records.Num(), 2))
	{
		TestTrue(TEXT("the first final Complete executed (and latched)"), Records[0].Outcome == EQueueSlotOutcome::Executed);
		TestTrue(TEXT("the second final Complete is Rejected_Latched"), Records[1].Outcome == EQueueSlotOutcome::Rejected_Latched);
		TestTrue(TEXT("the latch holds the first completion's key"), Queue.IsLatched() && Queue.GetTerminalKey() == Records[0].Key);
	}

	LogRig.Gate.bReleasable = true;  // read at Ended
	TArray<FRoundLogEntry> Entries;
	TestTrue(TEXT("log readable at Ended"), LogRig.Log.ReadEntries(Entries) == ERoundLogReadResult::Ok);
	const TArray<FRoundLogEntry> Tends = Entries.FilterByPredicate([](const FRoundLogEntry& E)
	{
		return E.Type == ERoundLogType::TendCompleted && E.Data[0] == FinalCourtSocketId;
	});
	if (TestEqual(TEXT("both attempts are logged as TendCompleted for the final court"), Tends.Num(), 2))
	{
		TestTrue(TEXT("first: completer A, Counted, Latched = false"),
			Tends[0].Participants[0] == FinalCompleterA && Tends[0].Code == static_cast<uint16>(ERoundTendResult::Counted) && (Tends[0].Flags & RoundLogFlags::Latched) == 0);
		TestTrue(TEXT("second: completer B, NoEffect, Latched = true"),
			Tends[1].Participants[0] == FinalCompleterB && Tends[1].Code == static_cast<uint16>(ERoundTendResult::NoEffect) && (Tends[1].Flags & RoundLogFlags::Latched) != 0);
		TestTrue(TEXT("the latched entry carries the rejected item's own key"), Records.Num() == 2 && Tends[1].Key == Records[1].Key);
	}
	TestEqual(TEXT("the queue also logged one Rejected_Latched entry"), Entries.FilterByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::Rejected_Latched; }).Num(), 1);
	TestEqual(TEXT("one WinLatched"), Entries.FilterByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::WinLatched; }).Num(), 1);
	// Objectives F5's objective-win share counts every completion attempt on the final: both.
	TestEqual(TEXT("the objective-win share counts both attempts"), Tends.Num(), 2);

	const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({});
	TestTrue(Teardown.Describe(), Teardown.IsClean());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
