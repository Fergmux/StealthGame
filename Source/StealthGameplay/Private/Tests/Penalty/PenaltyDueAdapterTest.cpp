// Story nkp-011 (production/epics/npc-kill-penalty-informants/story-011-t-due-owner-carrier-adapter.md), the Logic half
// (ADR-0019 section 6 option (a); GDD PEN-R15). The carrier adapter FPenaltyDueAdapter is bound to the core's
// OnDueTimeChanged and writes a recording Roster feed (SetOwnerRespawnDue(P, T_due, ExpectedEpoch)).
//
//   StealthGame.Penalty.DueAdapter         AC1: a schedule (115.0) and a readmission each make exactly one call with the
//                                          double T_due and the current epoch; embodiment, disconnect-cancellation and
//                                          reset each write 0; at round end OnDueTimeChanged(P, unset) still fires and no
//                                          call is made (latched). Edge: Picking (a retry) writes nothing.
//   StealthGame.Penalty.DueAdapterLatched  AC2: while IsLatched() a value and an unset make no call; across three ordinary
//                                          strikes no SetOwnerTally and no SetOwnerRespawnDue is made. Edge: the latch
//                                          flipping mid-drain (the write before it lands, the one after does not).
//
// AC3 (the owner-only wire capture) is the Roster half StealthGame.Roster.OwnerRespawnDueOwnerOnly plus the PIE run.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/StealthPenaltyDueAdapter.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyDueAdapterTestData
{
	/** A core rig whose OnDueTimeChanged drives an adapter over a recording feed; bLatched is the latch the adapter reads. */
	struct FAdapterRig
	{
		StealthPenaltyTest::FRig Rig;
		StealthPenaltyTest::FRecordingOwnerView Feed;
		bool bLatched = false;
		FPenaltyDueAdapter Adapter;

		explicit FAdapterRig(TFunction<void(StealthPenaltyTest::FFakeTuning&)> Configure = nullptr)
			: Rig(MoveTemp(Configure), false, /*bWithRespawnGateway*/ true)
			, Adapter(&Feed, [this]() { return bLatched; }, [this]() { return Rig.Roster.Epoch; })
		{
			Rig.DueTimes.Forward = [this](FParticipantId Participant, TOptional<double> Due) { Adapter.OnDueTimeChanged(Participant, Due); };
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyDueAdapterTest, "StealthGame.Penalty.DueAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyDueAdapterTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyDueAdapterTestData;

	FAdapterRig A;
	FRig& Rig = A.Rig;
	Rig.Roster.States.Add(P, EParticipantState::Informant);
	Rig.SetTime(100.0);

	// A schedule: one call with the double T_due and the epoch.
	Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, 100.0);
	TestTrue(TEXT("schedule: exactly one call, 115.0, the current epoch"), A.Feed.DueCalls.Num() == 1 && A.Feed.DueCalls[0].Participant == P
		&& A.Feed.DueCalls[0].DueServerTime == 115.0 && A.Feed.DueCalls[0].Epoch == FixtureEpoch);

	// Picking writes nothing: the item fires and the pick answers None (a regular retry).
	Rig.Gateway.Picks.Add(FPenaltyRespawnPick());
	Rig.Fire(P);
	TestEqual(TEXT("Picking (a retry): no write"), A.Feed.DueCalls.Num(), 1);

	// Embodiment: 0.
	Rig.Fire(P);
	TestTrue(TEXT("embodiment writes 0"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied && A.Feed.DueCalls.Num() == 2 && A.Feed.DueCalls[1].DueServerTime == 0.0);

	// An informant kill schedules again (one call), a disconnect cancels it (0), a readmission re-arms it (one call).
	Rig.KillInformant(P, Q, 901, Rig.Clock.GetServerTimeSeconds() + 0.4);
	const double Due = Rig.Core->GetDueServerTime(P).Get(0.0);
	TestTrue(TEXT("informant kill: one call with its T_due"), A.Feed.DueCalls.Num() == 3 && A.Feed.DueCalls[2].DueServerTime == Due && Due > 0.0);
	Rig.SetTime(Rig.Clock.GetServerTimeSeconds() + 1.0);
	Rig.Roster.Disconnected.Add(P);
	Rig.Core->OnConnectionChanged(P, false);
	TestTrue(TEXT("disconnect-cancellation writes 0"), A.Feed.DueCalls.Num() == 4 && A.Feed.DueCalls[3].DueServerTime == 0.0);
	Rig.SetTime(Rig.Clock.GetServerTimeSeconds() + 1.0);
	Rig.Roster.Disconnected.Remove(P);
	Rig.Core->OnInformantReadmitted(P);
	TestTrue(TEXT("readmission before T_due: exactly one call with the same T_due"), A.Feed.DueCalls.Num() == 5 && A.Feed.DueCalls[4].DueServerTime == Due);

	// Reset (Round clears the latch first): 0 for the pending request.
	Rig.Core->OnRosterReset(FixtureEpoch + 1);
	TestTrue(TEXT("reset writes 0"), A.Feed.DueCalls.Num() == 6 && A.Feed.DueCalls[5].DueServerTime == 0.0);

	// Round end: the delegate still fires unset, but the round is latched: no call.
	Rig.Roster.Epoch = FixtureEpoch + 1;
	Rig.SetTime(Rig.Clock.GetServerTimeSeconds() + 1.0);
	Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::Assassinated, Rig.Clock.GetServerTimeSeconds());
	const int32 BeforeEnd = A.Feed.DueCalls.Num();
	const int32 RaisedBeforeEnd = Rig.DueTimes.Calls.Num();
	A.bLatched = true;
	Rig.Core->OnRosterRoundEnded();
	TestTrue(TEXT("round end: OnDueTimeChanged(P, unset) still fires"), Rig.DueTimes.Calls.Num() == RaisedBeforeEnd + 1 && !Rig.DueTimes.Calls.Last().DueServerTime.IsSet());
	TestEqual(TEXT("round end: no SetOwnerRespawnDue (latched)"), A.Feed.DueCalls.Num(), BeforeEnd);
	TestEqual(TEXT("one write per change, never per frame: the adapter's count equals the feed's"), A.Adapter.GetWriteCount(), A.Feed.DueCalls.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyDueAdapterLatchedTest, "StealthGame.Penalty.DueAdapterLatched",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyDueAdapterLatchedTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyDueAdapterTestData;

	// IsLatched(): a value and an unset make no call.
	{
		FAdapterRig A;
		A.bLatched = true;
		A.Adapter.OnDueTimeChanged(P, 115.0);
		A.Adapter.OnDueTimeChanged(P, TOptional<double>());
		TestEqual(TEXT("latched: no SetOwnerRespawnDue for a value or an unset"), A.Feed.DueCalls.Num(), 0);
	}

	// Three ordinary strikes (L raised so none exiles): no owner-part write of the tally or of T_due.
	{
		FAdapterRig A([](FFakeTuning& Tuning) { Tuning.Live.NpcKillStrikeLimit = 5; });
		for (uint64 Npc = 900; Npc < 903; ++Npc)
		{
			A.Rig.NpcKill(P, Npc);
		}
		TestEqual(TEXT("three strikes: k = 3"), static_cast<int32>(A.Rig.Core->GetStrikeCount(P)), 3);
		TestEqual(TEXT("three strikes: no SetOwnerTally"), A.Rig.OwnerView.Calls.Num() + A.Feed.Calls.Num(), 0);
		TestEqual(TEXT("three strikes: no SetOwnerRespawnDue"), A.Feed.DueCalls.Num(), 0);
	}

	// The latch flipping mid-drain: the write before it lands, the one after it does not.
	{
		FAdapterRig A;
		A.Adapter.OnDueTimeChanged(P, 115.0);
		A.bLatched = true;
		A.Adapter.OnDueTimeChanged(P, TOptional<double>());
		TestTrue(TEXT("latch flip: one write (before the latch)"), A.Feed.DueCalls.Num() == 1 && A.Feed.DueCalls[0].DueServerTime == 115.0);
	}
	return true;
}

#endif
