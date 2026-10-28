// Story oc-012 (production/epics/objectives-chain/story-012-rejoin-rebind-and-reset.md), AC3 (GDD Edge Cases):
// StealthGame.Objectives.RejoinHoldDoesNotResume. On the real resolution queue with the Interact hold stand-in (the IIF side
// is stubbed: the IIF's cancel reaches Objectives only as OnInteractEnded(Cancelled), which the stand-in's InteractEnd slot
// delivers):
//   Rejoin inside the grace   team 0's tender begins a hold at its live P1 court; the disconnect cancels it (IIF); the
//                             rejoin rebinds the view. The hold does not resume: running well past D completes nothing, no
//                             hold and no claim exist, team stage state is unchanged. A NEW Begin then completes after a full D.
//   Unresponsive mid-hold     after 1.5 s of a hold the tender goes Unresponsive and the IIF cancels the hold: no running hold
//                             remains (its elapsed time reads 0), the claim is released, the stage is unchanged; back to
//                             Connected, a new Begin needs the full D again (nothing completes at 1.5 s + D - 1.5 s) and then counts.
//   Same tick as D (core)     a cancel and a Complete in the same frame, cancel first: the Complete has no claim and is NoEffect.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesRejoinHoldDoesNotResumeTest, "StealthGame.Objectives.RejoinHoldDoesNotResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesRejoinHoldDoesNotResumeTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	const FCourtFixtureId Court(CourtP1Team0);

	// --- Rejoin inside the grace ------------------------------------------------------------------------------------------
	{
		FQueueRig Rig;
		AStealthPawn* Tender = Rig.Pawn(CourtP1Team0, 0, 1, 0);
		Rig.ViewTargets.Add(1, 0);
		TestTrue(TEXT("to Active"), Rig.ToActive());
		const double D = Rig.Service->GetInteractHoldSec();
		Rig.Begin(Tender, CourtP1Team0);
		Rig.Run(1.0);
		TestTrue(TEXT("rejoin: the hold runs"), Rig.Holds->FindHold(Tender) != nullptr);
		// The disconnect: the IIF cancels the hold (the grace AI never continues it); then the rejoin rebinds the view.
		Rig.End(Tender);
		Rig.Run(0.2);
		Rig.ViewTargets.Replace(1);
		Rig.Service->HandleViewRebound(FParticipantId(1));
		Rig.Run(D + 1.0);
		TestEqual(TEXT("rejoin: the hold did not resume (nothing completed)"), Rig.Logged(ERoundLogType::ObjectiveStage), 0);
		TestTrue(TEXT("rejoin: no running hold"), Rig.Holds->FindHold(Tender) == nullptr);
		TestEqual(TEXT("rejoin: no claim"), Rig.Core().GetClaimCount(Court), 0);
		TestEqual(TEXT("rejoin: team stage state unchanged"), static_cast<int32>(Rig.Core().GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::Stage1));
		TestEqual(TEXT("rejoin: the rebound view shows Stage1"), static_cast<int32>(Rig.ViewTargets.ViewOf(1)->GetView().StageIndex), static_cast<int32>(EObjectiveTeamStage::Stage1));
		// A new Begin is required, and it counts after a full D.
		Rig.Begin(Tender, CourtP1Team0);
		Rig.Run(D + 0.5);
		TestEqual(TEXT("rejoin: a new Begin completes the stage"), Rig.Logged(ERoundLogType::ObjectiveStage), 1);
		TestEqual(TEXT("rejoin: Stage2 after the new hold"), static_cast<int32>(Rig.Core().GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::Stage2));
	}

	// --- Unresponsive mid-hold ----------------------------------------------------------------------------------------------
	{
		FQueueRig Rig;
		AStealthPawn* Tender = Rig.Pawn(CourtP1Team0, 0, 1, 0);
		TestTrue(TEXT("to Active"), Rig.ToActive());
		const double D = Rig.Service->GetInteractHoldSec();
		Rig.Begin(Tender, CourtP1Team0);
		Rig.Run(1.5);
		Rig.Session.States.Add(1, EObjectivesConnectionState::Unresponsive);
		Rig.End(Tender); // the IIF's cancel (Round hands the pawn to AI by the normal blend)
		Rig.Run(0.2);
		TestTrue(TEXT("unresponsive: no running hold (elapsed reads 0)"), Rig.Holds->FindHold(Tender) == nullptr);
		TestEqual(TEXT("unresponsive: the claim is released"), Rig.Core().GetClaimCount(Court), 0);
		TestEqual(TEXT("unresponsive: the last end was a claim release only"), static_cast<int32>(Rig.Service->GetLastCompletion()), static_cast<int32>(EObjectivesCompletion::ClaimReleaseOnly));
		TestEqual(TEXT("unresponsive: team stage state unchanged"), static_cast<int32>(Rig.Core().GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::Stage1));
		// Back to Connected: a new Begin must run the full D (the 1.5 s already held do not count).
		Rig.Session.States.Add(1, EObjectivesConnectionState::Connected);
		Rig.Begin(Tender, CourtP1Team0);
		Rig.Run(D - 1.0);
		TestEqual(TEXT("unresponsive: nothing completes before a full D of the new hold"), Rig.Logged(ERoundLogType::ObjectiveStage), 0);
		Rig.Run(1.5);
		TestEqual(TEXT("unresponsive: the new hold counts after a full D"), Rig.Logged(ERoundLogType::ObjectiveStage), 1);
	}

	// --- Same tick as D (core level): the cancel first, then the Complete -------------------------------------------------
	{
		StealthAssassinationTest::FScopedWorld World(TEXT("RejoinHoldSameTick"));
		FCoreRig Rig;
		AStealthPawn* Tender = FCoreRig::PawnAt(World, CourtP1Team0);
		Rig.Roster.Bind(Tender, 1, 0);
		Rig.Core.OnInteractExecuted(*Tender, Court, FQueueKey(1, 1));
		TestEqual(TEXT("same tick: the cancel is a claim release"), static_cast<int32>(Rig.Core.OnInteractEnded(*Tender, Court, EObjectivesInteractEnd::Cancelled, FQueueKey(2, 1))),
			static_cast<int32>(EObjectivesCompletion::ClaimReleaseOnly));
		TestEqual(TEXT("same tick: the Complete after it has no effect"), static_cast<int32>(Rig.Core.OnInteractEnded(*Tender, Court, EObjectivesInteractEnd::Completed, FQueueKey(2, 2))),
			static_cast<int32>(EObjectivesCompletion::NoEffect));
		TestEqual(TEXT("same tick: team stage state unchanged"), static_cast<int32>(Rig.Core.GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::Stage1));
	}
	return true;
}

#endif
