// Story rmf-044 (production/epics/round-match-framework/story-044-elimination-truth-table.md), GDD AC 48 / R15, F2:
//   - the F2 zeroing event with every clause true (Decision reason Assassinated, so ActiveCountAfter[W] >= 1 applies)
//     gives EliminationWin(W);
//   - flipping exactly one clause at a time gives no outcome: Old not Active; New not Informant; ActiveCountAfter[e.Team]
//     not 0; RosterCount(e.Team) 0; RosterCount(W) 0; phase not Active; Terminated true; a batch event;
//   - a preliminary (stage) completion never ends the round (ObjectiveWin requires FinalCompletion).
// The expected rows are written by hand from F2 (no reuse of the story rmf-043 reference). Rows beyond AC 48's list
// (e.Applied, the Decision/Expiry partition) are not added (qa-lead approval needed).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundWin.h"
#include "Server/Testing/StealthRoundTestConstants.h"

namespace EliminationTruthTableTestData
{
	/** Team 0's last Active member is assassinated: e{team 0, ActiveCountAfter [0, 2]}, 2v2 rosters, Active, not latched. */
	void AllClausesTrue(FRoundTransitionEvent& E, FRoundWinContext& C)
	{
		using namespace RoundTestFixtures;
		E = FRoundTransitionEvent();
		E.Participant = FParticipantId(1);
		E.Team = ERosterTeam::Team0;
		E.Old = EParticipantState::Active;
		E.New = EParticipantState::Informant;
		E.Reason = ERosterReason::Assassinated;
		E.ActiveCountAfter[0] = 0;
		E.ActiveCountAfter[1] = Layout2v2.Team1;
		C = FRoundWinContext();
		C.Phase = ERoundPhase::Active;
		C.bTerminated = false;
		C.RosterCount[0] = Layout2v2.Team0;
		C.RosterCount[1] = Layout2v2.Team1;
		C.ConnectedActiveCount[1] = Layout2v2.Team1;
	}

	struct FFlip
	{
		const TCHAR* Clause;
		void (*Apply)(FRoundTransitionEvent&, FRoundWinContext&);
	};

	const FFlip Flips[] = {
		{ TEXT("Old is not Active"), [](FRoundTransitionEvent& E, FRoundWinContext&) { E.Old = EParticipantState::Informant; } },
		{ TEXT("New is not Informant"), [](FRoundTransitionEvent& E, FRoundWinContext&) { E.New = EParticipantState::RoundEnded; } },
		{ TEXT("ActiveCountAfter[e.Team] is not 0"), [](FRoundTransitionEvent& E, FRoundWinContext&) { E.ActiveCountAfter[0] = 1; } },
		{ TEXT("RosterCount(e.Team) is 0"), [](FRoundTransitionEvent&, FRoundWinContext& C) { C.RosterCount[0] = 0; } },
		{ TEXT("RosterCount(W) is 0"), [](FRoundTransitionEvent&, FRoundWinContext& C) { C.RosterCount[1] = 0; } },
		{ TEXT("phase is not Active"), [](FRoundTransitionEvent&, FRoundWinContext& C) { C.Phase = ERoundPhase::Ended; } },
		{ TEXT("Terminated is true"), [](FRoundTransitionEvent&, FRoundWinContext& C) { C.bTerminated = true; } },
		{ TEXT("a batch event"), [](FRoundTransitionEvent& E, FRoundWinContext&) { E.bBatch = true; } },
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundEliminationTruthTableTest, "StealthGame.Round.EliminationTruthTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundEliminationTruthTableTest::RunTest(const FString& Parameters)
{
	using namespace EliminationTruthTableTestData;

	// All clauses true: EliminationWin(1).
	FRoundTransitionEvent E;
	FRoundWinContext C;
	AllClausesTrue(E, C);
	TestTrue(TEXT("all clauses: Zero(e) holds"), StealthRoundWin::IsZeroing(E, C));
	const TOptional<FRoundTerminalOutcome> Win = StealthRoundWin::EvaluateTransition(E, C);
	TestTrue(TEXT("all clauses: EliminationWin(W = 1)"), Win.IsSet() && Win->IsWin() && Win->WinningTeam == 1 && Win->EndReason == ERoundEndReason::Elimination);

	// One clause flipped at a time: no outcome.
	for (const FFlip& Flip : Flips)
	{
		AllClausesTrue(E, C);
		Flip.Apply(E, C);
		TestFalse(FString::Printf(TEXT("%s: no outcome"), Flip.Clause), StealthRoundWin::EvaluateTransition(E, C).IsSet());
	}

	// A preliminary (stage) completion never ends the round; the same completion as the final one does.
	FRoundObjectiveCompletion Stage;
	Stage.bFinal = false;
	Stage.Team = 0;
	Stage.bCompleterActive = Stage.bCompleterConnected = Stage.bHumanControlled = Stage.bCompleterOnTeam = Stage.bSiteLive = true;
	AllClausesTrue(E, C);
	TestFalse(TEXT("a preliminary completion never ends the round"), StealthRoundWin::EvaluateObjectiveCompletion(Stage, C).IsSet());
	FRoundObjectiveCompletion Final = Stage;
	Final.bFinal = true;
	const TOptional<FRoundTerminalOutcome> Objective = StealthRoundWin::EvaluateObjectiveCompletion(Final, C);
	TestTrue(TEXT("the final completion gives ObjectiveWin(0)"), Objective.IsSet() && Objective->WinningTeam == 0 && Objective->EndReason == ERoundEndReason::ObjectiveWin);
	return true;
}

#endif
