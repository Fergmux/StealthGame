// Story oc-009 (production/epics/objectives-chain/story-009-final-completion-and-win-declaration.md), AC1 / AC-OC-06 final
// rows CompletionGateFinal: the six-row table of the stage gate (ObjectivesGateRows.h) applied to a FINAL HoldTimer(Complete)
// slot (team 0's final Live; row 6 is a team 1 completer, whose final is still Locked): DeclareObjectiveWin is called exactly
// once for row 1 (with the slot's key, for team 0, after one FinalCompleted entry) and never for rows 2-6, whose site states
// and team views are unchanged. Fresh fixture per row (order independent). Edge: row 1 completing twice declares once.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesGateRows.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesCompletionGateFinalTest, "StealthGame.Objectives.CompletionGateFinal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesCompletionGateFinalTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	StealthAssassinationTest::FScopedWorld World(TEXT("ObjectivesCompletionGateFinal"));

	for (const EGateRow Row : AllGateRows)
	{
		FCoreRig Rig;
		Rig.Core.ApplyCountedCompletion(0, DefP1Team0);
		Rig.Core.ApplyCountedCompletion(0, DefP2Team0);
		const TArray<EObjectiveSiteState> Before = Rig.Snapshot();
		const int32 Pushes = Rig.Views.Pushes.Num();
		const FQueueKey Key(20, 4);
		const EObjectivesCompletion Outcome = RunGateRow(Rig, World, Row, CourtFinal, Key);
		const bool bWins = Row == EGateRow::HumanEligible;
		TestEqual(FString::Printf(TEXT("final %s: outcome"), LexGateRow(Row)), static_cast<int32>(Outcome),
			static_cast<int32>(bWins ? EObjectivesCompletion::CountedFinal : EObjectivesCompletion::NoEffect));
		TestEqual(FString::Printf(TEXT("final %s: DeclareObjectiveWin calls"), LexGateRow(Row)), Rig.Round.Wins.Num(), bWins ? 1 : 0);
		TestEqual(FString::Printf(TEXT("final %s: FinalCompleted entries"), LexGateRow(Row)), Rig.Round.Count(ERoundLogType::FinalCompleted), bWins ? 1 : 0);
		if (bWins)
		{
			TestTrue(TEXT("the win is declared for team 0 with the slot's key"), Rig.Round.Wins[0].Key == 0 && Rig.Round.Wins[0].Value == Key);
			// Edge: a second completion of the final (the latch is set; the site is Completed): declared once.
			AStealthPawn* Second = FCoreRig::PawnAt(World, CourtFinal, 1);
			Rig.Roster.Bind(Second, 3, 0);
			Rig.Core.OnInteractExecuted(*Second, FCourtFixtureId(CourtFinal), FQueueKey(1, 1));
			Rig.Core.OnInteractEnded(*Second, FCourtFixtureId(CourtFinal), EObjectivesInteractEnd::Completed, FQueueKey(21, 1));
			TestEqual(TEXT("row 1 completing twice declares once"), Rig.Round.Wins.Num(), 1);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("final %s: site states unchanged"), LexGateRow(Row)), Rig.Snapshot() == Before);
			TestEqual(FString::Printf(TEXT("final %s: no view push"), LexGateRow(Row)), Rig.Views.Pushes.Num(), Pushes);
		}
	}
	return true;
}

#endif
