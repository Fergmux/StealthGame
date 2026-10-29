// Story con-006 (production/epics/concealment-bushes/story-006-round-phase-lifecycle-and-no-dwell.md; GDD AC-CON-17,
// AC-CON-12 simulation part, Edge Cases; ADR-0017 section 3 Phases; ADR-0002 section 4 post-latch exemption):
//   - StealthGame.Concealment.RoundEndAndReset: transitions in flight at the latch complete (the ConcealmentTransition
//     exemption: a Commit after the latch still conceals and culls; an exit completes), later Hide commands are
//     Rejected_Latched, concealed pawns stay culled; at the reset every record is empty and every queued item drops.
//   - StealthGame.Concealment.NoDwellTimer: 30 min concealed in Active with no input: still concealed, no event, reply,
//     log warning or state change except the Commit row.
//   - StealthGame.Concealment.StaleItemsVoided: a Commit, Cross or End firing after its pawn was killed or reset resolves
//     Voided with no state, cull, flag or log write.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"

using namespace StealthConcealmentTest;

namespace ConcealmentRoundPhaseTestData
{
	int32 CountOutcome(const FRig& Rig, EQueueSlotOutcome Outcome)
	{
		return Rig.Q.Queue->GetLastDrainRecords().FilterByPredicate([Outcome](const FQueueSlotRecord& R) { return R.Outcome == Outcome; }).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentRoundEndAndResetTest, "StealthGame.Concealment.RoundEndAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentRoundEndAndResetTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentRoundPhaseTestData;
	FRig Rig(TEXT("ConcealmentRoundEnd"));
	AStealthPawn* Entrant = Rig.SpawnAtReach(30.0);
	AStealthPawn* Hider = Rig.SpawnAtReach(30.0, 40.0);
	AStealthPawn* Exiter = Rig.SpawnAtReach(30.0, -40.0);
	Rig.Relations.Bind(Hider, 2, 0);
	Rig.EnterAndSettle(Hider);
	Rig.EnterAndSettle(Exiter);
	Rig.SubmitHide(Exiter, ECommandKind::HideExit, 0);
	Rig.SubmitHide(Entrant, ECommandKind::HideEnter, 1);
	Rig.Tick();
	TestEqual(TEXT("in flight: one Entering"), Rig.StateOf(Entrant), EConcealState::Entering);
	TestEqual(TEXT("in flight: one Exiting"), Rig.StateOf(Exiter), EConcealState::Exiting);

	// The latch (round Ended): the in-flight transitions complete through the exemption.
	Rig.Q.Queue->Latch(Rig.Q.Queue->GetLastDrainRecords().Last().Key);
	const int32 UnculledBefore = Rig.Cull.CountFor(Hider, false);
	Rig.Run(1.0);
	TestEqual(TEXT("after the latch the Commit still conceals the entrant"), Rig.StateOf(Entrant), EConcealState::Concealed);
	TestEqual(TEXT("... and culls it (concealed pawns stay culled in Ended)"), Rig.Cull.CountFor(Entrant, true), 1);
	TestEqual(TEXT("the exit in flight completes Outside"), Rig.StateOf(Exiter), EConcealState::Outside);

	// Later Hide commands are rejected by the latch (the owner hears NotNow through the IIF's latched path).
	Rig.SubmitHide(Hider, ECommandKind::HideExit, 0);
	Rig.Tick();
	TestEqual(TEXT("a Hide after the latch is Rejected_Latched"), CountOutcome(Rig, EQueueSlotOutcome::Rejected_Latched), 1);
	TestEqual(TEXT("the concealed pawn stays Concealed"), Rig.StateOf(Hider), EConcealState::Concealed);
	TestEqual(TEXT("... and culled (no un-cull after the latch)"), Rig.Cull.CountFor(Hider, false), UnculledBefore);

	// Reset: OnRosterReset clears every record and flag; queued items of the old epoch drop.
	Rig.SubmitHide(Exiter, ECommandKind::HideEnter, 1);
	Rig.Q.Queue->ResetForNewRound();
	Rig.Core->ResetAll();
	Rig.Q.Epoch.Current.RoundEpoch += 1;
	Rig.Run(1.0);
	TestFalse(TEXT("reset: the hider's record is gone"), Rig.Core->FindRecord(Hider) != nullptr);
	TestFalse(TEXT("reset: the entrant's record is gone"), Rig.Core->FindRecord(Entrant) != nullptr);
	TArray<APawn*> Record;
	Rig.Core->GetOccupancy(1, Record);
	TestEqual(TEXT("reset: the occupancy record is empty"), Record.Num(), 0);
	TestFalse(TEXT("reset: the owner flag is cleared"), Rig.Flags.OwnerOf(2));
	TestEqual(TEXT("reset: the old epoch's Hide never executed"), Rig.StateOf(Exiter), EConcealState::Outside);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentNoDwellTimerTest, "StealthGame.Concealment.NoDwellTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentNoDwellTimerTest::RunTest(const FString& Parameters)
{
	FRig Rig(TEXT("ConcealmentNoDwell"));
	AStealthPawn* Player = Rig.SpawnAtReach(30.0);
	Rig.Relations.Bind(Player, 4, 1);
	Rig.EnterAndSettle(Player);
	const int32 Items = Rig.Core->GetScheduledItemCount();
	const int32 Rows = Rig.Q.Log.Entries.Num();
	const int32 FlagWrites = Rig.Flags.Writes;
	const int32 CullCalls = Rig.Cull.Calls.Num();
	const int32 PoseWrites = Rig.Pawns.PoseWrites;
	const uint32 Generation = Rig.Core->FindRecord(Player)->TransitionGeneration;
	// 30 minutes of server time, one drain per simulated minute and one per frame for the first minute.
	Rig.Run(60.0);
	for (int32 Minute = 1; Minute < 30; ++Minute)
	{
		Rig.Q.Clock.Advance(60.0, 3600);
		Rig.Q.Queue->Drain();
	}
	TestEqual(TEXT("30 min later: still Concealed"), Rig.StateOf(Player), EConcealState::Concealed);
	TestEqual(TEXT("no item was ever scheduled after the enter's two"), Rig.Core->GetScheduledItemCount(), Items);
	TestEqual(TEXT("no log row after the Commit row"), Rig.Q.Log.Entries.Num(), Rows);
	TestEqual(TEXT("exactly one Commit row"), Rig.Count(EConcealmentLogEvent::Commit), 1);
	TestEqual(TEXT("no flag write"), Rig.Flags.Writes, FlagWrites);
	TestEqual(TEXT("no cull change"), Rig.Cull.Calls.Num(), CullCalls);
	TestEqual(TEXT("no pose write"), Rig.Pawns.PoseWrites, PoseWrites);
	TestEqual(TEXT("no reply"), Rig.RigQueue.Replies.Num(), 0);
	TestEqual(TEXT("no state change (same generation)"), Rig.Core->FindRecord(Player)->TransitionGeneration, Generation);
	TestEqual(TEXT("no fault"), Rig.Q.Faults.GetRecords().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentStaleItemsVoidedTest, "StealthGame.Concealment.StaleItemsVoided",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentStaleItemsVoidedTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentRoundPhaseTestData;
	// Killed while Entering: the Commit and End drop by generation.
	{
		FRig Rig(TEXT("ConcealmentStaleKilledEntering"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
		Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
		Rig.Tick();
		Rig.Core->OnPawnKilled(*Pawn, FQueueKey());
		Rig.Pawns.Of(Pawn).bLive = false;
		const int32 PoseWrites = Rig.Pawns.PoseWrites;
		int32 Voided = 0;
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			Rig.Tick();
			Voided += CountOutcome(Rig, EQueueSlotOutcome::Voided);
		}
		TestEqual(TEXT("killed while Entering: Commit and End resolve Voided"), Voided, 2);
		TestEqual(TEXT("... no cull"), Rig.Cull.Calls.Num(), 0);
		TestEqual(TEXT("... no log row"), Rig.Count(EConcealmentLogEvent::Commit), 0);
		TestEqual(TEXT("... no pose write"), Rig.Pawns.PoseWrites, PoseWrites);
	}
	// Killed while Exiting before the crossing: the Cross and End drop; the flags stay for the body's removal.
	{
		FRig Rig(TEXT("ConcealmentStaleKilledExiting"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
		Rig.Relations.Bind(Pawn, 9, 0);
		Rig.EnterAndSettle(Pawn);
		Rig.SubmitHide(Pawn, ECommandKind::HideExit, 0);
		Rig.Tick();
		Rig.Core->OnPawnKilled(*Pawn, FQueueKey());
		Rig.Pawns.Of(Pawn).bLive = false;
		const int32 FlagWrites = Rig.Flags.Writes;
		int32 Voided = 0;
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			Rig.Tick();
			Voided += CountOutcome(Rig, EQueueSlotOutcome::Voided);
		}
		TestEqual(TEXT("killed while Exiting: Cross and End resolve Voided"), Voided, 2);
		TestEqual(TEXT("... no flag write"), Rig.Flags.Writes, FlagWrites);
		TestEqual(TEXT("... no ExitCross row"), Rig.Count(EConcealmentLogEvent::ExitCross), 0);
	}
	// Reset with items pending (same epoch, records cleared): they find no record and resolve Voided.
	{
		FRig Rig(TEXT("ConcealmentStaleReset"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
		Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
		Rig.Tick();
		Rig.Core->ResetAll();
		int32 Voided = 0;
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			Rig.Tick();
			Voided += CountOutcome(Rig, EQueueSlotOutcome::Voided);
		}
		TestEqual(TEXT("reset: Commit and End resolve Voided"), Voided, 2);
		TestEqual(TEXT("... the pawn is Outside"), Rig.StateOf(Pawn), EConcealState::Outside);
		TestEqual(TEXT("... no cull"), Rig.Cull.Calls.Num(), 0);
	}
	return true;
}

#endif
