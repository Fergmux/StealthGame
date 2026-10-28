// Story oc-004 (production/epics/objectives-chain/story-004-core-site-state-and-progress.md), AC2 / AC-OC-17
// FinalNeedsOwnUnlock (SP-4): GIVEN team 0 has unlocked the final (its Prelim1 and Prelim2 completed) and team 1 has not
// WHEN a team 1 pawn tends the true final to D (the completion slot) THEN no effect and no state change: no ObjectiveStage or
// FinalCompleted entry, no win declared (team 1's final is Locked for team 1, gate (d)). A team 0 unlock never unlocks team 1;
// team 1 with only Prelim1 done is still locked. The server-only TendCompleted(NoEffect) measurement entry of a bound pawn's
// completion is story oc-008's and is the only entry written. Control: once team 0's final is Live, team 0's own tend counts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesFinalNeedsOwnUnlockTest, "StealthGame.Objectives.FinalNeedsOwnUnlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesFinalNeedsOwnUnlockTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	StealthAssassinationTest::FScopedWorld World(TEXT("ObjectivesFinalNeedsOwnUnlock"));

	FCoreRig Rig;
	AStealthPawn* Team1Pawn = FCoreRig::PawnAt(World, CourtFinal, 0);
	AStealthPawn* Team0Pawn = FCoreRig::PawnAt(World, CourtFinal, 1);
	if (!TestNotNull(TEXT("pawns"), Team1Pawn) || !TestNotNull(TEXT("pawns"), Team0Pawn))
	{
		return false;
	}
	Rig.Roster.Bind(Team1Pawn, 11, 1);
	Rig.Roster.Bind(Team0Pawn, 1, 0);

	// Team 0 unlocks its final.
	Rig.Core.ApplyCountedCompletion(0, DefP1Team0);
	Rig.Core.ApplyCountedCompletion(0, DefP2Team0);
	TestEqual(TEXT("team 0's final is Live"), static_cast<int32>(Rig.Core.GetSiteState(0, DefFinal)), static_cast<int32>(EObjectiveSiteState::Live));
	TestEqual(TEXT("a team 0 unlock never unlocks team 1's final"), static_cast<int32>(Rig.Core.GetSiteState(1, DefFinal)), static_cast<int32>(EObjectiveSiteState::Locked));

	// Team 1 tends the true final before its unlock: no effect.
	{
		const TArray<EObjectiveSiteState> Before = Rig.Snapshot();
		const EObjectivesCompletion Outcome = Rig.Tend(*Team1Pawn, CourtFinal);
		TestEqual(TEXT("team 1's tend at the true final has no effect"), static_cast<int32>(Outcome), static_cast<int32>(EObjectivesCompletion::NoEffect));
		TestTrue(TEXT("no state change"), Rig.Snapshot() == Before);
		TestEqual(TEXT("no win declared"), Rig.Round.Wins.Num(), 0);
		TestEqual(TEXT("no ObjectiveStage entry"), Rig.Round.Count(ERoundLogType::ObjectiveStage), 0);
		TestEqual(TEXT("no FinalCompleted entry"), Rig.Round.Count(ERoundLogType::FinalCompleted), 0);
	}

	// Team 1 with only Prelim1 done is still locked.
	{
		Rig.Core.ApplyCountedCompletion(1, DefP1Team1);
		TestEqual(TEXT("team 1 at Stage2 after its Prelim1"), static_cast<int32>(Rig.Core.GetStageIndex(1)), static_cast<int32>(EObjectiveTeamStage::Stage2));
		const EObjectivesCompletion Outcome = Rig.Tend(*Team1Pawn, CourtFinal, FQueueKey(3, 1));
		TestEqual(TEXT("team 1 with only Prelim1 done: the final still has no effect"), static_cast<int32>(Outcome), static_cast<int32>(EObjectivesCompletion::NoEffect));
		TestEqual(TEXT("team 1's final is still Locked"), static_cast<int32>(Rig.Core.GetSiteState(1, DefFinal)), static_cast<int32>(EObjectiveSiteState::Locked));
		TestEqual(TEXT("still no win declared"), Rig.Round.Wins.Num(), 0);
	}

	// Control: team 0's own tend at its live final counts.
	const EObjectivesCompletion Control = Rig.Tend(*Team0Pawn, CourtFinal, FQueueKey(4, 1));
	TestEqual(TEXT("control: team 0's tend at its Live final counts"), static_cast<int32>(Control), static_cast<int32>(EObjectivesCompletion::CountedFinal));
	return true;
}

#endif
