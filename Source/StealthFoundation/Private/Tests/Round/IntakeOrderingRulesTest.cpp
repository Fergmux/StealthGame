// Story rmf-007 (production/epics/round-match-framework/story-007-intake-ordering-rules.md): GDD AC 45.
// Per-source FIFO; the DueEpsilonMs = 1 due rule (ruling D22) with its group order; a 3.0 s hold begun on tick 0
// at 60 Hz stamped on tick 180, never 181, over 10,000 accumulated-time fixtures; equal DueTime ordered by that
// tick's TieRank; a mailbox survives a controller replacement and an old ConnectionGeneration is
// Dropped(StaleConnection); a 33rd item in one drain is dropped and counted.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Random/StealthSecretStream.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "View/Random/StealthRangedDraw.h"

namespace IntakeOrderingRulesTestData
{
	constexpr double FrameSec = 1.0 / 60.0;
	constexpr uint32 IntakeTag = 1000;
	constexpr uint32 TagAlreadyDue = 1;
	constexpr uint32 TagHalfMs = 2;
	constexpr uint32 TagOneMs = 3;
	constexpr uint32 TagTwoMs = 4;
	constexpr int32 TieRankTicks = 200;
	constexpr uint32 OldGeneration = 1;
	constexpr uint32 NewGeneration = 2;

	int32 IndexOfTag(const FStealthStubResolver& Stub, uint32 Tag)
	{
		return Stub.Resolved.IndexOfByPredicate([Tag](const FStealthStubResolution& R) { return R.Arg == Tag; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundIntakeOrderingRulesTest, "StealthGame.Round.IntakeOrderingRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundIntakeOrderingRulesTest::RunTest(const FString& Parameters)
{
	using namespace IntakeOrderingRulesTestData;
	using namespace RoundTestFixtures;
	const FMailboxId A = FMailboxId::ForParticipant(FParticipantId(FixtureParticipantIds[0]));
	const FMailboxId B = FMailboxId::ForParticipant(FParticipantId(FixtureParticipantIds[1]));

	// 1. Per-source order: A1 then A2 in one frame (with B competing) over 10,000 ticks.
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Stub;
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Stub);
		int32 Violations = 0;
		for (int32 Tick = 0; Tick < FairnessTicks; ++Tick)
		{
			Stub.Resolved.Reset();
			Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, 1, A));
			Rig.Queue->Submit(B, Rig.MakeItem(EQueueItemType::PawnCommand, 9, B));
			Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, 2, A));
			Rig.Tick(FrameSec);
			const int32 A1 = IndexOfTag(Stub, 1);
			const int32 A2 = IndexOfTag(Stub, 2);
			Violations += (A1 == INDEX_NONE || A2 == INDEX_NONE || A1 > A2) ? 1 : 0;
		}
		TestEqual(TEXT("A1 precedes A2 in every one of 10,000 ticks"), Violations, 0);
	}

	// 2. Due rule: DueTime <= DrainStart + 1 ms is due; already-due items are stamped before intake.
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Commands;
		FStealthStubResolver Timers;
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Commands);
		Rig.Queue->RegisterTimerResolver(ETimerKind::DefenceWindowEnd, &Timers);
		Rig.Clock.Advance(10.0, 10);
		const double DrainStart = Rig.Clock.GetServerTimeSeconds() + FrameSec;  // the next Tick's drain start
		Rig.Queue->ScheduleAt(DrainStart - 0.1, Rig.MakeTimer(ETimerKind::DefenceWindowEnd, TagAlreadyDue));
		Rig.Queue->ScheduleAt(DrainStart + 0.0005, Rig.MakeTimer(ETimerKind::DefenceWindowEnd, TagHalfMs));
		Rig.Queue->ScheduleAt(DrainStart + FResolutionQueueCore::DueEpsilonSec, Rig.MakeTimer(ETimerKind::DefenceWindowEnd, TagOneMs));
		Rig.Queue->ScheduleAt(DrainStart + 0.002, Rig.MakeTimer(ETimerKind::DefenceWindowEnd, TagTwoMs));
		Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, IntakeTag, A));
		Rig.Tick(FrameSec);
		TestTrue(TEXT("the drain started where the fixture expected"), Rig.Clock.GetServerTimeSeconds() == DrainStart);
		TestTrue(TEXT("already-due item fired in this drain"), IndexOfTag(Timers, TagAlreadyDue) != INDEX_NONE);
		TestTrue(TEXT("item due +0.5 ms fired in this drain"), IndexOfTag(Timers, TagHalfMs) != INDEX_NONE);
		TestTrue(TEXT("item due exactly +1 ms fired in this drain (inclusive)"), IndexOfTag(Timers, TagOneMs) != INDEX_NONE);
		TestTrue(TEXT("item due +2 ms did not fire in this drain"), IndexOfTag(Timers, TagTwoMs) == INDEX_NONE);
		if (TestEqual(TEXT("the intake item resolved"), Commands.Resolved.Num(), 1) && Timers.Resolved.Num() == 3)
		{
			for (const FStealthStubResolution& Timer : Timers.Resolved)
			{
				TestTrue(TEXT("every due item is stamped before this frame's intake"), Timer.Key < Commands.Resolved[0].Key);
			}
		}
		Rig.Tick(FrameSec);
		TestTrue(TEXT("item due +2 ms fired in the next drain"), IndexOfTag(Timers, TagTwoMs) != INDEX_NONE);

		// Group 3: an item scheduled inline during the drain, due now, is stamped after intake.
		Commands.OnResolve = [&Rig](const FQueueItem& Item, FQueueKey Key)
		{
			Rig.Queue->ScheduleAt(Rig.Clock.GetServerTimeSeconds(), Rig.MakeTimer(ETimerKind::DefenceWindowEnd, 77));
			return EResolveResult::Executed;
		};
		const FMailboxId C = FMailboxId::ForParticipant(FParticipantId(FixtureParticipantIds[2]));
		Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, IntakeTag + 1, A));
		Rig.Queue->Submit(C, Rig.MakeItem(EQueueItemType::PawnCommand, IntakeTag + 2, C));
		Timers.Resolved.Reset();
		Commands.Resolved.Reset();
		Rig.Tick(FrameSec);
		if (TestEqual(TEXT("two inline-scheduled items fired in the same drain"), Timers.Resolved.Num(), 2) && Commands.Resolved.Num() == 2)
		{
			TestTrue(TEXT("inline-due items are stamped after intake"), Commands.Resolved[1].Key < Timers.Resolved[0].Key);
		}
	}

	// 3. A 3.0 s hold begun on tick 0 at 60 Hz: stamped on tick 180, never 181, over 10,000 accumulated-time
	//    fixtures. Frame deltas carry a fixed-seed jitter of up to 2 us (counter stream, not a secret).
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Holds;
		Rig.Queue->RegisterResolver(EQueueItemType::HoldTimer, &Holds);
		FSecretStream Jitter(AccumulatedTimeFixtureSeed, StealthRandom::Tag("RMF7JITR"));
		int32 OnTick180 = 0;
		int32 Wrong = 0;
		for (int32 Fixture = 0; Fixture < AccumulatedTimeFixtureCount; ++Fixture)
		{
			Rig.Queue->ResetForNewRound();
			Holds.Resolved.Reset();
			// Tick 0 of the fixture: the hold begins in this drain.
			Rig.Tick(FrameSec);
			const uint64 BeginTick = Rig.Clock.GetServerTickIndex();
			Rig.Queue->ScheduleAt(Rig.Clock.GetServerTimeSeconds() + HoldDurationSec, Rig.MakeItem(EQueueItemType::HoldTimer, static_cast<uint32>(Fixture)));
			for (uint64 Frame = 1; Frame <= HoldDueTick + 1 && Holds.Resolved.Num() == 0; ++Frame)
			{
				const int32 JitterUs = StealthRandom::UniformInRange(Jitter, -FrameJitterMicroseconds, FrameJitterMicroseconds);
				Rig.Tick(1.0 / HoldFrameRateHz + JitterUs * 1e-6);
			}
			if (Holds.Resolved.Num() == 1 && Holds.Resolved[0].Key.ServerTickIndex - BeginTick == HoldDueTick)
			{
				++OnTick180;
			}
			else
			{
				++Wrong;
			}
		}
		TestEqual(TEXT("the 3.0 s hold is stamped on tick 180 in every fixture"), OnTick180, AccumulatedTimeFixtureCount);
		TestEqual(TEXT("never on tick 181 (or any other tick)"), Wrong, 0);
	}

	// 4. Equal DueTime: ordered by that tick's TieRank (a per-tick secret shuffle, not the id, not fixed per round).
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Grace;
		Rig.Queue->RegisterTimerResolver(ETimerKind::GraceExpiry, &Grace);
		const uint32 LowId = FixtureParticipantIds[0];
		const uint32 HighId = FixtureParticipantIds[1];
		int32 Mismatches = 0;
		int32 HighFirst = 0;
		for (int32 Tick = 0; Tick < TieRankTicks; ++Tick)
		{
			Grace.Resolved.Reset();
			const double Due = Rig.Clock.GetServerTimeSeconds() + FrameSec * 0.5;
			// Scheduled high id first, so neither schedule order nor id order can explain the result.
			Rig.Queue->ScheduleAt(Due, Rig.MakeTimer(ETimerKind::GraceExpiry, HighId, FParticipantId(HighId)));
			Rig.Queue->ScheduleAt(Due, Rig.MakeTimer(ETimerKind::GraceExpiry, LowId, FParticipantId(LowId)));
			Rig.Tick(FrameSec);
			// Expected: the TieRank shuffle of the sorted participant ids for this tick.
			uint32 Expected[2] = { LowId, HighId };
			FSecretStream Stream(Rig.Seeds.GetServerSubSeed(ESeedDomain::TieRank, static_cast<uint32>(Rig.Clock.GetServerTickIndex())), FResolutionQueueCore::TieRankStreamTag);
			StealthRandom::FisherYates(Stream, TArrayView<uint32>(Expected, 2));
			if (Grace.Resolved.Num() != 2 || Grace.Resolved[0].Arg != Expected[0] || Grace.Resolved[1].Arg != Expected[1])
			{
				++Mismatches;
			}
			HighFirst += (Grace.Resolved.Num() == 2 && Grace.Resolved[0].Arg == HighId) ? 1 : 0;
		}
		TestEqual(TEXT("equal-DueTime items follow that tick's TieRank in every tick"), Mismatches, 0);
		TestTrue(FString::Printf(TEXT("TieRank varies per tick (high id first in %d of %d ticks)"), HighFirst, TieRankTicks), HighFirst > 0 && HighFirst < TieRankTicks);
	}

	// 5. Controller replacement keeps the mailbox; an old ConnectionGeneration is Dropped(StaleConnection).
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Stub;
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Stub);
		Rig.Generations.Generations.Add(A.Value, OldGeneration);
		// Seamless travel replaces the controller but not the participant or its generation: same mailbox, FIFO.
		Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, 1, A), OldGeneration);
		Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, 2, A), OldGeneration);  // after replacement
		Rig.Tick(FrameSec);
		TestTrue(TEXT("the mailbox survives a controller replacement (both resolved, in order)"),
			Stub.Resolved.Num() == 2 && Stub.Resolved[0].Arg == 1 && Stub.Resolved[1].Arg == 2);

		// A supersede bumps the generation: an item still carrying the old one is dropped and logged.
		Stub.Resolved.Reset();
		Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, 3, A), OldGeneration);
		Rig.Generations.Generations.Add(A.Value, NewGeneration);
		Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, 4, A), NewGeneration);
		Rig.Tick(FrameSec);
		TestTrue(TEXT("only the current-generation item resolved"), Stub.Resolved.Num() == 1 && Stub.Resolved[0].Arg == 4);
		const TArray<FQueueSlotRecord>& Records = Rig.Queue->GetLastDrainRecords();
		TestTrue(TEXT("the old-generation item is Dropped(StaleConnection)"), Records.Num() == 2 && Records[0].Outcome == EQueueSlotOutcome::Dropped_StaleConnection);
		TestEqual(TEXT("one Dropped(StaleConnection) entry logged"), Rig.Log.CountOfType(ERoundLogType::Dropped_StaleConnection), 1);
		TestEqual(TEXT("stale-connection drops counted"), Rig.Queue->GetTotalStaleConnectionDropCount(), static_cast<uint64>(1));
	}

	// 5b. Equal DueTime across kinds (code review): holds first, by Begin key; then the rest by TieRank.
	{
		FStealthQueueTestRig Rig;
		TArray<uint32> Order;
		FStealthStubResolver Holds;
		FStealthStubResolver Grace;
		Holds.OnResolve = [&Order](const FQueueItem& Item, FQueueKey) { Order.Add(FStealthStubResolver::ArgOf(Item)); return EResolveResult::Executed; };
		Grace.OnResolve = Holds.OnResolve;
		Rig.Queue->RegisterResolver(EQueueItemType::HoldTimer, &Holds);
		Rig.Queue->RegisterTimerResolver(ETimerKind::GraceExpiry, &Grace);
		const double Due = Rig.Clock.GetServerTimeSeconds() + FrameSec * 0.5;
		Rig.Queue->ScheduleAt(Due, Rig.MakeTimer(ETimerKind::GraceExpiry, 50, FParticipantId(FixtureParticipantIds[0])));
		FHoldTimerPayload Late;
		Late.ActionEpoch = 20;
		Late.BeginKey = FQueueKey(1, 9);
		FHoldTimerPayload Early;
		Early.ActionEpoch = 10;
		Early.BeginKey = FQueueKey(1, 3);
		Rig.Queue->ScheduleAt(Due, FQueueItem::Make(EQueueItemType::HoldTimer, Rig.Epoch.Current, FMailboxId::ServerLocal(), Late));
		Rig.Queue->ScheduleAt(Due, FQueueItem::Make(EQueueItemType::HoldTimer, Rig.Epoch.Current, FMailboxId::ServerLocal(), Early));
		Rig.Tick(FrameSec);
		TestTrue(TEXT("equal DueTime: holds by Begin key, then the timer"), Order == TArray<uint32>{ 10, 20, 50 });
	}

	// 5c. A resolver that keeps scheduling items due now cannot hang the drain (code review): bounded and reported.
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Looper;
		Looper.OnResolve = [&Rig](const FQueueItem&, FQueueKey)
		{
			Rig.Queue->ScheduleAt(Rig.Clock.GetServerTimeSeconds(), Rig.MakeTimer(ETimerKind::CrowdSchedule, 1));
			return EResolveResult::Executed;
		};
		Rig.Queue->RegisterTimerResolver(ETimerKind::CrowdSchedule, &Looper);
		Rig.Queue->ScheduleAt(Rig.Clock.GetServerTimeSeconds(), Rig.MakeTimer(ETimerKind::CrowdSchedule, 1));
		Rig.Tick(FrameSec);
		TestEqual(TEXT("the drain stopped after the bounded number of inline-due passes"), Looper.Resolved.Num(), FResolutionQueueCore::MaxInlineDuePasses + 1);
		TestEqual(TEXT("the remaining item waits for the next drain"), Rig.Queue->GetPendingTimerCount(), 1);
		const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({ { EStealthFault::QueueMisuse } });
		TestTrue(Teardown.Describe(), Teardown.IsClean());
	}

	// 6. The 33rd item from one mailbox in one drain is dropped and counted.
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Stub;
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Stub);
		for (int32 Index = 0; Index < FResolutionQueueCore::MailboxMaxItemsPerDrain + 1; ++Index)
		{
			Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, static_cast<uint32>(Index), A));
		}
		Rig.Queue->Submit(B, Rig.MakeItem(EQueueItemType::PawnCommand, 500, B));
		Rig.Tick(FrameSec);
		TestEqual(TEXT("32 items from A plus B's one resolved"), Stub.Resolved.Num(), FResolutionQueueCore::MailboxMaxItemsPerDrain + 1);
		TestTrue(TEXT("the 33rd item (the last submitted) was the one dropped"), IndexOfTag(Stub, static_cast<uint32>(FResolutionQueueCore::MailboxMaxItemsPerDrain)) == INDEX_NONE);
		TestEqual(TEXT("one item dropped and counted in this drain"), Rig.Queue->GetLastDrainOverflowCount(), 1);
		const TArray<FRoundLogEntry> Overflow = Rig.Log.Entries.FilterByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::MailboxOverflow; });
		TestTrue(TEXT("one MailboxOverflow entry for A carrying the count"), Overflow.Num() == 1 && Overflow[0].SourceMailbox == A && Overflow[0].Data[0] == 1);
		TestEqual(TEXT("nothing carried over to the next drain"), Rig.Queue->GetPendingMailboxItemCount(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
