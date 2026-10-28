// Story oc-004 (production/epics/objectives-chain/story-004-core-site-state-and-progress.md), AC3 (OC-4, OC-17)
// StageLifecycle:
//   - at Active entry both teams are Stage1 with Prelim1 Live and everything else Locked;
//   - completing every Prelim2 makes the Final Live for that team only (both teams can have their final Live at once);
//   - progress never regresses when the completer later dies, disconnects or is removed (stub Roster events);
//   - after Ended every state is frozen and a completion arriving afterwards is ignored.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesStageLifecycleTest, "StealthGame.Objectives.StageLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesStageLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	StealthAssassinationTest::FScopedWorld World(TEXT("ObjectivesStageLifecycle"));

	FCoreRig Rig(/*bActive*/ false);
	TestEqual(TEXT("before Active entry: team 0 is None"), static_cast<int32>(Rig.Core.GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::None));
	TestTrue(TEXT("Active entry runs"), Rig.Core.HandleActiveEntry(RoundEpoch, ActivePhaseStart));
	TestEqual(TEXT("t0 is stored"), Rig.Core.GetActivePhaseStart(), ActivePhaseStart);

	// Active entry: Stage1, Prelim1 Live, everything else Locked.
	for (int32 Team = 0; Team < 2; ++Team)
	{
		TestEqual(FString::Printf(TEXT("team %d is Stage1"), Team), static_cast<int32>(Rig.Core.GetStageIndex(Team)), static_cast<int32>(EObjectiveTeamStage::Stage1));
		const int32 OwnP1 = Team == 0 ? DefP1Team0 : DefP1Team1;
		for (int32 Def = 0; Def < Rig.Core.GetDefinitionCount(); ++Def)
		{
			const EObjectiveSiteState Want = Def == OwnP1 ? EObjectiveSiteState::Live : EObjectiveSiteState::Locked;
			TestEqual(FString::Printf(TEXT("team %d row %d at entry"), Team, Def), static_cast<int32>(Rig.Core.GetSiteState(Team, Def)), static_cast<int32>(Want));
		}
	}
	TestEqual(TEXT("both views pushed at Active entry"), Rig.Views.Pushes.Num(), 2);

	// A completer who later dies, disconnects, is removed: progress stays.
	AStealthPawn* Completer = FCoreRig::PawnAt(World, CourtP1Team0, 0);
	if (!TestNotNull(TEXT("completer pawn"), Completer))
	{
		return false;
	}
	Rig.Roster.Bind(Completer, 1, 0);
	TestEqual(TEXT("team 0's Prelim1 counts"), static_cast<int32>(Rig.Tend(*Completer, CourtP1Team0)), static_cast<int32>(EObjectivesCompletion::CountedStage));
	const TArray<EObjectiveSiteState> AfterStage = Rig.Snapshot();
	Rig.Roster.States[1] = EParticipantState::Informant; // killed
	Rig.Session.States.Add(1, EObjectivesConnectionState::Disconnected); // disconnected
	Rig.Roster.Bindings.Remove(Completer); // removed
	TestTrue(TEXT("progress never regresses on death, disconnect or removal"), Rig.Snapshot() == AfterStage);
	TestEqual(TEXT("team 0 stays at Stage2"), static_cast<int32>(Rig.Core.GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::Stage2));

	// Every Prelim2 completed makes the Final Live for that team only; both teams' finals can be Live at once.
	Rig.Core.ApplyCountedCompletion(0, DefP2Team0);
	TestEqual(TEXT("team 0's final is Live"), static_cast<int32>(Rig.Core.GetSiteState(0, DefFinal)), static_cast<int32>(EObjectiveSiteState::Live));
	TestEqual(TEXT("team 1's final stays Locked"), static_cast<int32>(Rig.Core.GetSiteState(1, DefFinal)), static_cast<int32>(EObjectiveSiteState::Locked));
	TestEqual(TEXT("team 0 is at Final"), static_cast<int32>(Rig.Core.GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::Final));
	TestEqual(TEXT("team 0 completed both its prelims (mask bits 0 and 1)"), static_cast<int32>(Rig.Core.GetCompletedDefMask(0)), (1 << DefP1Team0) | (1 << DefP2Team0));
	TestEqual(TEXT("team 0 completed two preliminary stages"), Rig.Core.GetCompletedPreliminaryStages(0), 2);
	Rig.Core.ApplyCountedCompletion(1, DefP1Team1);
	Rig.Core.ApplyCountedCompletion(1, DefP2Team1);
	TestEqual(TEXT("both teams' finals are Live at once"), static_cast<int32>(Rig.Core.GetSiteState(1, DefFinal)), static_cast<int32>(EObjectiveSiteState::Live));

	// After Ended: frozen; a completion arriving afterwards is ignored.
	Rig.Core.HandleRoundEnded();
	const TArray<EObjectiveSiteState> Frozen = Rig.Snapshot();
	TestFalse(TEXT("a completion after Ended is ignored"), Rig.Core.ApplyCountedCompletion(0, DefFinal).bApplied);
	TestTrue(TEXT("all states frozen after Ended"), Rig.Snapshot() == Frozen);
	return true;
}

#endif
