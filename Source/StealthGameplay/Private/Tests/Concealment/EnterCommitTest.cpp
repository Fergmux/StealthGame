// Story con-004 (production/epics/concealment-bushes/story-004-hide-enter-execution-and-commit.md; GDD CON-R5, CON-R6a,
// AC-CON-29; ADR-0017 section 3):
//   - StealthGame.Concealment.EnterExecution (AC1, AC2): a legal Hide(Enter) at T writes Entering, a new generation and
//     FActionState {BushEnter, StartServerTime = T} identically for an NPC and a player, and schedules exactly two items:
//     Commit at T + BushEnterSwapSec and End at T + BushEnterDurSec, on whole server ticks. At the commit: Concealed, one
//     occupancy entry with its CommitSeq, one Commit log row, SetCullState(true) once in the commit's slot, the owner and
//     team setters once (player only); at End the pose returns to None and the state stays Concealed.
//   - StealthGame.Concealment.GuardFaultPath (AC3, AC-CON-29): a forced guard failure at the swap commits nothing, records
//     exactly one ConcealGuard fault and runs the ordinary exit on the entry side (Exiting, BushExit, Cross and End
//     items, never a PawnCommand); after the latch the same internal transition still runs.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"

using namespace StealthConcealmentTest;

namespace ConcealmentEnterCommitTestData
{
	/** The observable sequence of one enter: times of the commit and the end, and the writes made. */
	struct FTrace
	{
		double Start = -1.0;
		double CommitAt = -1.0;
		double EndAt = -1.0;
		int32 Scheduled = 0;
		EActionPose PoseAtStart = EActionPose::None;
		double PoseStart = -1.0;
		EActionPose PoseAfterEnd = EActionPose::Strike;
		EConcealState StateAfterEnd = EConcealState::Outside;
		int32 CullTrue = 0;
		FQueueKey CullKey;
		int32 CommitRows = 0;
		bool bOwnerFlag = false;
		bool bTeamFlag = false;
		int32 FlagWrites = 0;
		int32 Occupants = 0;
	};

	FTrace RunEnter(bool bPlayer, FAutomationTestBase& Test)
	{
		FTrace Out;
		FRig Rig(bPlayer ? TEXT("ConcealmentEnterPlayer") : TEXT("ConcealmentEnterNpc"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(40.0);
		if (bPlayer)
		{
			Rig.Relations.Bind(Pawn, 7, 0);
		}
		Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
		Rig.Tick();
		Out.Start = Rig.Core->FindRecord(Pawn) != nullptr ? Rig.Core->FindRecord(Pawn)->TransitionStartServerTime : -1.0;
		Out.Scheduled = Rig.Core->GetScheduledItemCount();
		Out.PoseAtStart = Rig.Pawns.Of(Pawn).Action.Pose;
		Out.PoseStart = Rig.Pawns.Of(Pawn).Action.StartServerTime;
		for (int32 Frame = 0; Frame < 90; ++Frame)
		{
			const EConcealState Before = Rig.StateOf(Pawn);
			const EActionPose PoseBefore = Rig.Pawns.Of(Pawn).Action.Pose;
			Rig.Tick();
			if (Before == EConcealState::Entering && Rig.StateOf(Pawn) == EConcealState::Concealed)
			{
				Out.CommitAt = Rig.Now();
			}
			if (PoseBefore == EActionPose::BushEnter && Rig.Pawns.Of(Pawn).Action.Pose == EActionPose::None)
			{
				Out.EndAt = Rig.Now();
			}
		}
		Out.PoseAfterEnd = Rig.Pawns.Of(Pawn).Action.Pose;
		Out.StateAfterEnd = Rig.StateOf(Pawn);
		Out.CullTrue = Rig.Cull.CountFor(Pawn, true);
		Out.CullKey = Rig.Cull.Calls.Num() > 0 ? Rig.Cull.Calls[0].Key : FQueueKey();
		Out.CommitRows = Rig.Count(EConcealmentLogEvent::Commit);
		Out.bOwnerFlag = Rig.Flags.OwnerOf(7);
		Out.bTeamFlag = Rig.Flags.TeamOf(7);
		Out.FlagWrites = Rig.Flags.Writes;
		TArray<APawn*> Record;
		Rig.Core->GetOccupancy(1, Record);
		Out.Occupants = Record.Num();
		Test.TestTrue(TEXT("the commit ran inside a queue slot (its key is set)"), Out.CullKey.ServerTickIndex != 0 || Out.CullKey.Seq != 0);
		Test.TestEqual(TEXT("no unexpected fault"), Rig.Q.Faults.GetRecords().Num(), 0);
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentEnterExecutionTest, "StealthGame.Concealment.EnterExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentEnterExecutionTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentEnterCommitTestData;
	const FConcealmentParams Params;
	const FTrace Npc = RunEnter(false, *this);
	const FTrace Player = RunEnter(true, *this);
	for (const FTrace* T : { &Npc, &Player })
	{
		const TCHAR* Who = T == &Npc ? TEXT("NPC") : TEXT("player");
		TestEqual(FString::Printf(TEXT("%s: exactly two items scheduled (Commit, End)"), Who), T->Scheduled, 2);
		TestEqual(FString::Printf(TEXT("%s: FActionState pose BushEnter"), Who), T->PoseAtStart, EActionPose::BushEnter);
		TestEqual(FString::Printf(TEXT("%s: StartServerTime = execution time"), Who), T->PoseStart, T->Start, 1e-9);
		TestEqual(FString::Printf(TEXT("%s: Commit at T + BushEnterSwapSec (whole ticks)"), Who), T->CommitAt - T->Start, static_cast<double>(Params.Public.BushEnterSwapSec), FrameSec + 1e-6);
		TestEqual(FString::Printf(TEXT("%s: End at T + BushEnterDurSec (whole ticks)"), Who), T->EndAt - T->Start, static_cast<double>(Params.Public.BushEnterDurSec), FrameSec + 1e-6);
		TestEqual(FString::Printf(TEXT("%s: at End the pose returns to None"), Who), T->PoseAfterEnd, EActionPose::None);
		TestEqual(FString::Printf(TEXT("%s: the state stays Concealed"), Who), T->StateAfterEnd, EConcealState::Concealed);
		TestEqual(FString::Printf(TEXT("%s: SetCullState(true) once"), Who), T->CullTrue, 1);
		TestEqual(FString::Printf(TEXT("%s: one Commit log row"), Who), T->CommitRows, 1);
		TestEqual(FString::Printf(TEXT("%s: one occupant with its CommitSeq"), Who), T->Occupants, 1);
	}
	TestEqual(TEXT("NPC and player: identical commit offset"), Npc.CommitAt - Npc.Start, Player.CommitAt - Player.Start, 1e-9);
	TestEqual(TEXT("NPC and player: identical end offset"), Npc.EndAt - Npc.Start, Player.EndAt - Player.Start, 1e-9);
	TestTrue(TEXT("player: owner and team flags set once each"), Player.bOwnerFlag && Player.bTeamFlag && Player.FlagWrites == 2);
	TestEqual(TEXT("NPC: no flag setter is called (an unbound pawn has no participant)"), Npc.FlagWrites, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentGuardFaultPathTest, "StealthGame.Concealment.GuardFaultPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentGuardFaultPathTest::RunTest(const FString& Parameters)
{
	for (const bool bLatched : { false, true })
	{
		const TCHAR* Phase = bLatched ? TEXT("after the latch") : TEXT("during Active");
		FRig Rig(bLatched ? TEXT("ConcealmentGuardLatched") : TEXT("ConcealmentGuardActive"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(40.0);
		Rig.Pawns.Of(Pawn).bWarpLands = false; // the forced guard failure: the capsule never gets inside the hide volume
		Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
		Rig.Tick();
		const uint64 CommandsBefore = Rig.Q.Queue->PeekNextSeq();
		if (bLatched)
		{
			Rig.Q.Queue->Latch(Rig.Q.Queue->GetLastDrainRecords().Last().Key);
		}
		Rig.Run(0.5);
		TestEqual(FString::Printf(TEXT("%s: no commit, the ordinary exit runs"), Phase), Rig.StateOf(Pawn), EConcealState::Exiting);
		TestEqual(FString::Printf(TEXT("%s: the exit pose is BushExit"), Phase), Rig.Pawns.Of(Pawn).Action.Pose, EActionPose::BushExit);
		TestEqual(FString::Printf(TEXT("%s: exactly one ConcealGuard fault"), Phase),
			Rig.Q.Faults.GetRecords().FilterByPredicate([](const FStealthFaultRecord& R) { return R.Fault == EStealthFault::ConcealGuard; }).Num(), 1);
		TestEqual(FString::Printf(TEXT("%s: nothing culled"), Phase), Rig.Cull.Calls.Num(), 0);
		TestEqual(FString::Printf(TEXT("%s: no Commit row"), Phase), Rig.Count(EConcealmentLogEvent::Commit), 0);
		TestEqual(FString::Printf(TEXT("%s: the exit uses the entry side (+X)"), Phase), Rig.Core->FindRecord(Pawn)->Side, static_cast<uint8>(0));
		TestEqual(FString::Printf(TEXT("%s: 4 items in all (Commit, End, then the exit's Cross and End)"), Phase), Rig.Core->GetScheduledItemCount(), 4);
		// No PawnCommand was submitted for the exit: every item since the command is a timer.
		bool bOnlyTimers = true;
		for (const FQueueSlotRecord& Slot : Rig.Q.Queue->GetLastDrainRecords())
		{
			bOnlyTimers &= Slot.Type == EQueueItemType::TimerFired;
		}
		TestTrue(FString::Printf(TEXT("%s: the exit is an internal transition (no PawnCommand)"), Phase), bOnlyTimers && Rig.Q.Queue->PeekNextSeq() >= CommandsBefore);
		Rig.Run(1.0);
		TestEqual(FString::Printf(TEXT("%s: the exit completes Outside"), Phase), Rig.StateOf(Pawn), EConcealState::Outside);
	}
	return true;
}

#endif
