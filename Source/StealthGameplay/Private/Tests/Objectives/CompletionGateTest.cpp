// Story oc-008 (production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md), AC1 / AC-OC-06 stage rows
// CompletionGate, table-driven with a fresh fixture per row (so row order cannot matter): a stage HoldTimer(Complete) slot at
// team 0's live Prelim1 court with a completer that is (1) Active + Connected + human on an eligible team, (2) AI-driven
// (hand-over), (3) Unresponsive, (4) Disconnected, (5) an Informant at the slot, (6) Active on a team for which the site is
// not Live: exactly one ObjectiveStage entry for row 1 and none for rows 2-6, whose site states and team views are unchanged.
// Every row's completer is bound, so each appends one server-only TendCompleted (Counted for row 1, NoEffect for 2-6; the
// TendCompleted reading of the story). Edge: row 1 repeated in the same slot still logs once.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesGateRows.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesCompletionGateTest, "StealthGame.Objectives.CompletionGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesCompletionGateTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	StealthAssassinationTest::FScopedWorld World(TEXT("ObjectivesCompletionGate"));

	for (const EGateRow Row : AllGateRows)
	{
		FCoreRig Rig;
		const TArray<EObjectiveSiteState> Before = Rig.Snapshot();
		const int32 PushesBefore = Rig.Views.Pushes.Num();
		const FQueueKey Key(10, 3);
		const EObjectivesCompletion Outcome = RunGateRow(Rig, World, Row, CourtP1Team0, Key);
		const bool bCounts = Row == EGateRow::HumanEligible;
		TestEqual(FString::Printf(TEXT("%s: outcome"), LexGateRow(Row)), static_cast<int32>(Outcome),
			static_cast<int32>(bCounts ? EObjectivesCompletion::CountedStage : EObjectivesCompletion::NoEffect));
		TestEqual(FString::Printf(TEXT("%s: ObjectiveStage entries"), LexGateRow(Row)), Rig.Round.Count(ERoundLogType::ObjectiveStage), bCounts ? 1 : 0);
		const TArray<FRoundLogEntry> Tends = Rig.Round.OfType(ERoundLogType::TendCompleted);
		if (TestEqual(FString::Printf(TEXT("%s: one TendCompleted (bound completer)"), LexGateRow(Row)), Tends.Num(), 1))
		{
			TestEqual(FString::Printf(TEXT("%s: TendCompleted result"), LexGateRow(Row)), static_cast<int32>(Tends[0].Code),
				static_cast<int32>(bCounts ? ERoundTendResult::Counted : ERoundTendResult::NoEffect));
		}
		if (!bCounts)
		{
			TestTrue(FString::Printf(TEXT("%s: site states unchanged"), LexGateRow(Row)), Rig.Snapshot() == Before);
			TestEqual(FString::Printf(TEXT("%s: team views unchanged (no push)"), LexGateRow(Row)), Rig.Views.Pushes.Num(), PushesBefore);
		}
		else
		{
			// Edge: the same completion again in the same slot logs no second ObjectiveStage (the site is Completed).
			AStealthPawn* Again = FCoreRig::PawnAt(World, CourtP1Team0, 1);
			Rig.Roster.Bind(Again, 1, 0);
			Rig.Core.OnInteractExecuted(*Again, FCourtFixtureId(CourtP1Team0), FQueueKey(1, 1));
			Rig.Core.OnInteractEnded(*Again, FCourtFixtureId(CourtP1Team0), EObjectivesInteractEnd::Completed, Key);
			TestEqual(TEXT("row 1 repeated in the same slot still logs one ObjectiveStage"), Rig.Round.Count(ERoundLogType::ObjectiveStage), 1);
		}
	}
	return true;
}

#endif
