// Story con-032 AC2 and the server half of AC1 (production/epics/concealment-bushes/story-032-ownership-change-while-
// concealed.md; GDD CON-R11; ADR-0017 section 3): StealthGame.Concealment.TransitionSurvivesHandover.
//   - a transition in flight when control passes to the crowd (Roster ReleaseToCrowd: the pawn becomes an NPC) completes:
//     Entering -> Concealed (the commit lands, no flags for the NPC), Exiting -> Outside; nothing is cancelled or snapped;
//   - a Concealed pawn released stays Concealed (no instant un-hide): its former participant's owner and team flags clear,
//     and the cull is pushed again (CullPending from the release) so the former allies are closed at the next boundary;
//     the later exit is the ordinary Hide(Exit) of any NPC (Exiting, Cross, End; one ExitCross row as an NPC);
//   - a rejoiner bound again to its still-concealed pawn learns its state from the owner flag (and its team from the team
//     flag), pose None; a rebind to a pawn that is not concealed sets nothing.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "Server/ConcealmentLog.h"

using namespace StealthConcealmentTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentTransitionSurvivesHandoverTest, "StealthGame.Concealment.TransitionSurvivesHandover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentTransitionSurvivesHandoverTest::RunTest(const FString& Parameters)
{
	// Entering when control passes: the commit still lands.
	{
		FRig Rig(TEXT("ConcealHandoverEntering"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
		Rig.Relations.Bind(Pawn, 31, 0);
		Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
		Rig.Run(0.2);
		TestTrue(TEXT("Entering when control passes"), Rig.StateOf(Pawn) == EConcealState::Entering);
		Rig.Relations.Participants.Remove(Pawn); // ReleaseToCrowd: an NPC now
		Rig.Core->OnPawnReleased(*Pawn, FParticipantId(31));
		Rig.Run(Rig.Params.Public.BushEnterDurSec + 2.0 * FrameSec);
		TestTrue(TEXT("the enter completes: Concealed"), Rig.StateOf(Pawn) == EConcealState::Concealed);
		TestEqual(TEXT("the commit pushed the cull once"), Rig.Cull.CountFor(Pawn, true), 1);
		TestTrue(TEXT("no flag for the NPC (its former participant is never set)"), !Rig.Flags.OwnerOf(31) && !Rig.Flags.TeamOf(31));
	}
	// Exiting when control passes: the exit completes Outside.
	{
		FRig Rig(TEXT("ConcealHandoverExiting"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
		Rig.Relations.Bind(Pawn, 32, 0);
		Rig.EnterAndSettle(Pawn);
		Rig.SubmitHide(Pawn, ECommandKind::HideExit, 0);
		Rig.Run(0.2);
		TestTrue(TEXT("Exiting when control passes"), Rig.StateOf(Pawn) == EConcealState::Exiting);
		Rig.Relations.Participants.Remove(Pawn);
		Rig.Core->OnPawnReleased(*Pawn, FParticipantId(32));
		Rig.Run(Rig.Params.Public.BushExitDurSec + 2.0 * FrameSec);
		TestTrue(TEXT("the exit completes: Outside"), Rig.StateOf(Pawn) == EConcealState::Outside);
		TestEqual(TEXT("no second cull push for a pawn already un-culled"), Rig.Cull.CountFor(Pawn, true), 1);
	}
	// Concealed when released: stays Concealed, flags clear, the cull is pushed again; the later exit is an ordinary NPC exit.
	{
		FRig Rig(TEXT("ConcealHandoverConcealed"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
		Rig.Relations.Bind(Pawn, 33, 1);
		Rig.EnterAndSettle(Pawn);
		TestTrue(TEXT("the player's flags are up"), Rig.Flags.OwnerOf(33) && Rig.Flags.TeamOf(33));
		const int32 CullBefore = Rig.Cull.CountFor(Pawn, true);
		Rig.Relations.Participants.Remove(Pawn);
		Rig.Core->OnPawnReleased(*Pawn, FParticipantId(33));
		TestTrue(TEXT("released: still Concealed (never an instant un-hide)"), Rig.StateOf(Pawn) == EConcealState::Concealed);
		TestEqual(TEXT("no un-cull at the release"), Rig.Cull.CountFor(Pawn, false), 0);
		TestEqual(TEXT("the cull is pushed again (the former allies close at the next boundary)"), Rig.Cull.CountFor(Pawn, true), CullBefore + 1);
		TestTrue(TEXT("the former participant's owner and team flags clear"), !Rig.Flags.OwnerOf(33) && !Rig.Flags.TeamOf(33));
		Rig.Run(20.0); // the crowd's dwell: nothing happens on its own
		TestTrue(TEXT("20 s later: still Concealed (no timer)"), Rig.StateOf(Pawn) == EConcealState::Concealed);
		Rig.SubmitHide(Pawn, ECommandKind::HideExit, 0);
		Rig.Tick();
		TestTrue(TEXT("the crowd's exit is the ordinary Hide(Exit): Exiting"), Rig.StateOf(Pawn) == EConcealState::Exiting);
		Rig.Run(Rig.Params.Public.BushExitDurSec + 2.0 * FrameSec);
		TestTrue(TEXT("... then Outside"), Rig.StateOf(Pawn) == EConcealState::Outside);
		const TArray<FConcealmentLogRow> Rows = ConcealmentLog::DecodeAll(Rig.Q.Log.Entries);
		const FConcealmentLogRow* Cross = Rows.FindByPredicate([](const FConcealmentLogRow& R) { return R.Event == EConcealmentLogEvent::ExitCross; });
		TestTrue(TEXT("one ExitCross row, logged as an NPC"), Cross != nullptr && !Cross->bPlayer);
	}
	// A rejoiner resumes its concealed pawn: the owner flag tells it (pose None); a rebind to an Outside pawn sets nothing.
	{
		FRig Rig(TEXT("ConcealHandoverRejoin"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
		AStealthPawn* Other = Rig.SpawnAtReach(30.0, 300.0);
		Rig.Relations.Bind(Pawn, 34, 0);
		Rig.EnterAndSettle(Pawn);
		Rig.Flags.Owner.Add(34, false); // the leaver's owner view was torn down with its controller
		Rig.Flags.Team.Add(34, false);
		Rig.Core->OnParticipantRebound(FParticipantId(34), Pawn);
		TestTrue(TEXT("the rejoiner's owner flag says Concealed again"), Rig.Flags.OwnerOf(34) && Rig.Flags.TeamOf(34));
		TestTrue(TEXT("its pose is None (the enter's End passed)"), Rig.Pawns.GetActionState(*Pawn).Pose == EActionPose::None);
		Rig.Flags.Owner.Add(35, false);
		Rig.Core->OnParticipantRebound(FParticipantId(35), Other);
		TestFalse(TEXT("a rebind to a pawn outside any bush sets no flag"), Rig.Flags.OwnerOf(35));
	}
	return true;
}

#endif
