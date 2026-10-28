// Story oc-010 (production/epics/objectives-chain/story-010-team-view-struct-and-builder.md), AC1 (AC-OC-29, core level):
// StealthGame.Objectives.NoTeamViewBeforeActive. A bound UObjectivesService (the real core) with one view component per
// member (team 0: participants 1, 2; team 1: participant 3) is taken through Preparing (ValidateSelectedChain) and
// OnLayoutReady: no component is written and every one holds the empty default (no site id, no stage card); a rejoin during
// Preparing (the rebind handler) leaves it empty. At OnRoundActive each team's view is pushed in that step exactly once
// (the core's team-level pushes) and every member's component is written exactly once with its own team's Stage1 view. The
// replicated multi-client check is story oc-019.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesNoTeamViewBeforeActiveTest, "StealthGame.Objectives.NoTeamViewBeforeActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesNoTeamViewBeforeActiveTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;

	// Arrange: three members with their components; the stub Roster knows their teams.
	FQueueRig Rig;
	const uint32 Members[] = { 1, 2, 3 };
	const int32 Teams[] = { 0, 0, 1 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Rig.ViewTargets.Add(Members[Index], Teams[Index]);
		Rig.Roster.Teams.Add(Members[Index], Teams[Index]);
		Rig.Roster.States.Add(Members[Index], EParticipantState::Active);
	}

	// Act: Preparing and OnLayoutReady.
	TestTrue(TEXT("Preparing: the chain validates"), Rig.Service->ValidateSelectedChain(FStealthServerTuning()).bValid);
	TestTrue(TEXT("OnLayoutReady binds"), Rig.Service->HandleLayoutReady(1, 0));
	// Edge case: a rejoin during Preparing.
	Rig.ViewTargets.Replace(2);
	Rig.Service->HandleViewRebound(FParticipantId(2));

	// Assert: nothing written, every sink holds the empty default.
	TestEqual(TEXT("Preparing: no team-level push"), Rig.Views.Pushes.Num(), 0);
	TestEqual(TEXT("Preparing: no component write"), Rig.Service->GetViewWriteCount(), 0);
	for (const uint32 Member : Members)
	{
		const UObjectiveTeamViewComponent* View = Rig.ViewTargets.ViewOf(Member);
		TestTrue(FString::Printf(TEXT("participant %u: the empty default in Preparing"), Member), View != nullptr && View->GetView().IsEmpty());
		TestEqual(FString::Printf(TEXT("participant %u: no site id"), Member), View != nullptr ? View->GetView().LiveSites.Num() : -1, 0);
		TestEqual(FString::Printf(TEXT("participant %u: no stage card (StageIndex None)"), Member), View != nullptr ? static_cast<int32>(View->GetView().StageIndex) : -1,
			static_cast<int32>(EObjectiveTeamStage::None));
		TestEqual(FString::Printf(TEXT("participant %u: never written"), Member), View != nullptr ? View->GetServerWriteCount() : -1, 0);
	}
	TestTrue(TEXT("the core's builder gives the empty default before Active"), Rig.Core().BuildTeamView(0).IsEmpty() && Rig.Core().BuildTeamView(1).IsEmpty());

	// Act: Active entry.
	TestTrue(TEXT("OnRoundActive"), Rig.Service->HandleRoundActive(Rig.Q.Epoch.Current, Rig.Q.Clock.GetServerTimeSeconds()));

	// Assert: each team pushed exactly once in that step; each member written once with its own team's view.
	TestEqual(TEXT("Active entry: team 0 pushed once"), Rig.Views.PushesFor(0), 1);
	TestEqual(TEXT("Active entry: team 1 pushed once"), Rig.Views.PushesFor(1), 1);
	TestEqual(TEXT("Active entry: two team-level pushes in all"), Rig.Views.Pushes.Num(), 2);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const UObjectiveTeamViewComponent* View = Rig.ViewTargets.ViewOf(Members[Index]);
		TestEqual(FString::Printf(TEXT("participant %u written exactly once"), Members[Index]), View->GetServerWriteCount(), 1);
		TestTrue(FString::Printf(TEXT("participant %u holds its team's view"), Members[Index]), View->GetView() == Rig.Core().BuildTeamView(Teams[Index]));
		TestEqual(FString::Printf(TEXT("participant %u at Stage1"), Members[Index]), static_cast<int32>(View->GetView().StageIndex), static_cast<int32>(EObjectiveTeamStage::Stage1));
	}

	// A second Active entry for the same epoch is ignored: still exactly one push per team.
	Rig.Service->HandleRoundActive(Rig.Q.Epoch.Current, Rig.Q.Clock.GetServerTimeSeconds());
	TestEqual(TEXT("a repeated Active entry pushes nothing more"), Rig.Views.Pushes.Num(), 2);
	return true;
}

#endif
