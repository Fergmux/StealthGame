// Story oc-012 (production/epics/objectives-chain/story-012-rejoin-rebind-and-reset.md), AC1 (AC-OC-15):
// StealthGame.Objectives.ResetAndRejoin. A bound UObjectivesService subscribed to a REAL URosterStore (2v2), Active, with
// progress (team 0 banked Prelim1, so a rival release was requested: the pending signal of OC-12), a running hold's claim at
// team 1's court and every member's component written:
//   - rejoin inside the grace: the rejoiner's new controller gets a view equal to its teammate's;
//   - rejoin after the grace expired (the participant is now an Informant, Round R17.4): it gets the team view like any
//     informant (Team audience) and nothing else is restored: no claim, no stage change, no extra team-level push;
//   - the store's ResetAll (OnRosterReset): every site state, the binding, the claims, the stage indices, the rival counts and
//     the view caches are cleared, and every member's component holds the empty default (no site survives into the next
//     round). The pending release list itself is story oc-015's (HOLD: none exists before it; the rival count is 0);
//   - a second reset with nothing pending changes nothing and writes nothing.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"
#include "ObjectivesRosterFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesResetAndRejoinTest, "StealthGame.Objectives.ResetAndRejoin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesResetAndRejoinTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;

	// Arrange: the store, the rig subscribed to it, Active with progress and a running hold.
	FRosterStoreFixture Roster;
	FQueueRig Rig;
	for (int32 Index = 0; Index < FRosterStoreFixture::Players; ++Index)
	{
		const uint32 Id = Roster.Ids[Index].Value;
		Rig.Roster.Teams.Add(Id, Roster.TeamOf(Index));
		Rig.Roster.States.Add(Id, EParticipantState::Active);
		Rig.ViewTargets.Add(Id, Roster.TeamOf(Index));
	}
	Rig.Init.RosterEvents = Roster.Store;
	Rig.Service->Bind(Rig.Init);
	TestTrue(TEXT("to Active"), Rig.ToActive());
	Rig.CompleteStageFast(0);
	TestEqual(TEXT("fixture: team 0 at Stage2"), static_cast<int32>(Rig.Core().GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::Stage2));
	TestEqual(TEXT("fixture: a rival release was requested (the pending signal)"), Rig.Rival.Calls.Num(), 1);
	AStealthPawn* Tender = Rig.Pawn(CourtP1Team1, 0, Roster.Ids[2].Value, 1);
	Rig.Begin(Tender, CourtP1Team1);
	Rig.Run(0.5);
	TestEqual(TEXT("fixture: a running hold claims team 1's court"), Rig.Core().GetClaimCount(FCourtFixtureId(CourtP1Team1)), 1);
	const uint32 A0 = Roster.Ids[0].Value, A1 = Roster.Ids[1].Value;

	// Act + Assert: rejoin inside the grace.
	UObjectiveTeamViewComponent* Rejoined = Rig.ViewTargets.Replace(A1);
	Roster.Rebind(1);
	TestTrue(TEXT("rejoin inside the grace: the view equals the teammate's"), Rejoined->GetView() == Rig.ViewTargets.ViewOf(A0)->GetView());
	TestEqual(TEXT("rejoin inside the grace: team 0 at Stage2 in the view"), static_cast<int32>(Rejoined->GetView().StageIndex), static_cast<int32>(EObjectiveTeamStage::Stage2));

	// Rejoin after the grace expired: an Informant now (R17.4); the team view, nothing else.
	Rig.Roster.States.Add(A1, EParticipantState::Informant);
	const int32 PushesBefore = Rig.Views.Pushes.Num();
	UObjectiveTeamViewComponent* Late = Rig.ViewTargets.Replace(A1);
	Roster.Rebind(1);
	TestTrue(TEXT("rejoin after the grace: the informant gets the team view"), Late->GetView() == Rig.ViewTargets.ViewOf(A0)->GetView());
	TestEqual(TEXT("rejoin after the grace: no team-level push (nothing changed)"), Rig.Views.Pushes.Num(), PushesBefore);
	TestEqual(TEXT("rejoin after the grace: team 0's stage unchanged"), static_cast<int32>(Rig.Core().GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::Stage2));
	TestEqual(TEXT("rejoin after the grace: no claim restored at team 0's courts"),
		Rig.Core().GetClaimCount(FCourtFixtureId(CourtP1Team0)) + Rig.Core().GetClaimCount(FCourtFixtureId(CourtP2Team0)), 0);

	// Act: the Roster's reset.
	TestEqual(TEXT("ResetAll applied"), static_cast<int32>(Roster.Store->ResetAll(2)), static_cast<int32>(ERosterResult::Applied));

	// Assert: everything cleared.
	const FObjectivesCore& Core = Rig.Core();
	TestFalse(TEXT("reset: nothing is bound"), Core.IsBound());
	TestFalse(TEXT("reset: not Active"), Core.IsActive());
	TestEqual(TEXT("reset: no binding (no site state survives)"), Core.GetDefinitionCount(), 0);
	for (int32 Team = 0; Team < 2; ++Team)
	{
		TestEqual(FString::Printf(TEXT("reset: team %d StageIndex None"), Team), static_cast<int32>(Core.GetStageIndex(Team)), static_cast<int32>(EObjectiveTeamStage::None));
		TestEqual(FString::Printf(TEXT("reset: team %d completed mask 0"), Team), static_cast<int32>(Core.GetCompletedDefMask(Team)), 0);
		TestEqual(FString::Printf(TEXT("reset: team %d rival count 0 (no pending signal survives)"), Team), static_cast<int32>(Core.GetRivalPrelimCount(Team)), 0);
		TestTrue(FString::Printf(TEXT("reset: team %d view cache cleared"), Team), Core.GetLastPushedView(Team).IsEmpty() && Core.BuildTeamView(Team).IsEmpty());
	}
	TestEqual(TEXT("reset: the claim table is empty"), Core.GetClaimCount(FCourtFixtureId(CourtP1Team1)), 0);
	TestEqual(TEXT("reset: the prelim ordinal is 0"), static_cast<int64>(Core.GetPrelimOrdinal()), static_cast<int64>(0));
	for (int32 Index = 0; Index < FRosterStoreFixture::Players; ++Index)
	{
		TestTrue(FString::Printf(TEXT("reset: member %d's component holds the empty default"), Index), Rig.ViewTargets.ViewOf(Roster.Ids[Index].Value)->GetView().IsEmpty());
	}

	// A second reset with nothing pending changes nothing.
	const int32 WritesBefore = Rig.Service->GetViewWriteCount();
	TestEqual(TEXT("second ResetAll applied"), static_cast<int32>(Roster.Store->ResetAll(3)), static_cast<int32>(ERosterResult::Applied));
	TestEqual(TEXT("second reset: no component written"), Rig.Service->GetViewWriteCount(), WritesBefore);
	TestEqual(TEXT("second reset: no service fault"), Rig.Q.Faults.GetRecords().Num(), 0);
	return true;
}

#endif
