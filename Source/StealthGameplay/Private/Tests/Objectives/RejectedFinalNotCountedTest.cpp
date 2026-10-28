// Objectives Chain story oc-017 AC3 (F5 note, cross-review C-W35 [U]): StealthGame.Objectives.RejectedFinalNotCounted.
// A final Complete rejected by the latch is logged TendCompleted(..., NoEffect, Latched) and counts in tend statistics (two such
// completions count twice), not as an extra FinalCompleted row, and does not move t_end or FinalDwell. Driven through the real
// core's gate: both teams tend their preliminaries, team 0 tends the final (the win latches), then two team-1 pawns' final
// Completes arrive after the latch.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "ObjectivesMetrics.h"
#include "ObjectivesTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesRejectedFinalNotCountedTest, "StealthGame.Objectives.RejectedFinalNotCounted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesRejectedFinalNotCountedTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	// Arrange: team 0 completes P1, P2 and the final (the win latches the round); team 1 completes P1 and P2.
	StealthAssassinationTest::FScopedWorld World(TEXT("RejectedFinalNotCounted"));
	FCoreRig Rig;
	// Real tends through the gate (a debug completion is not a playtest completion, R22).
	auto TendAt = [&World, &Rig](uint16 Court, uint32 Participant, int32 Team, double SinceActive, int32 Slot)
	{
		AStealthPawn* Pawn = FCoreRig::PawnAt(World, Court, 0);
		Rig.Roster.Bind(Pawn, Participant, Team);
		Rig.Round.Now = ActivePhaseStart + SinceActive;
		return Rig.Tend(*Pawn, Court, FQueueKey(Slot, 1));
	};
	TendAt(CourtP1Team0, 1, 0, 95.0, 2);
	TendAt(CourtP1Team1, 2, 1, 180.0, 3);
	TendAt(CourtP2Team0, 1, 0, 260.0, 4);
	TendAt(CourtP2Team1, 2, 1, 330.0, 5);
	TestEqual(TEXT("team 0's final counts"), static_cast<int32>(TendAt(CourtFinal, 1, 0, 420.0, 6)), static_cast<int32>(EObjectivesCompletion::CountedFinal));
	TestTrue(TEXT("the win latched"), Rig.Round.bLatched && Rig.Round.Wins.Num() == 1);
	FRoundLogEntry Win;
	Win.Type = ERoundLogType::WinLatched;
	Win.ServerTime = ActivePhaseStart + 420.0;
	Rig.Round.AppendEvent(Win); // Round's own record of the terminal key

	// Act: two team-1 pawns' final Completes after the latch (the queue's OnRejectedLatched path).
	AStealthPawn* A = FCoreRig::PawnAt(World, CourtFinal, 0);
	AStealthPawn* B = FCoreRig::PawnAt(World, CourtFinal, 1);
	Rig.Roster.Bind(A, 21, 1);
	Rig.Roster.Bind(B, 22, 1);
	Rig.Round.Now = ActivePhaseStart + 421.0;
	Rig.Core.OnInteractExecuted(*A, FCourtFixtureId(CourtFinal), FQueueKey(7, 1));
	Rig.Core.OnInteractExecuted(*B, FCourtFixtureId(CourtFinal), FQueueKey(7, 2));
	Rig.Core.OnInteractCompleteLatched(*A, FCourtFixtureId(CourtFinal), FQueueKey(8, 1));
	Rig.Round.Now = ActivePhaseStart + 425.0;
	Rig.Core.OnInteractCompleteLatched(*B, FCourtFixtureId(CourtFinal), FQueueKey(8, 2));

	// Assert: tend statistics count both; no extra FinalCompleted; t_end and FinalDwell unmoved.
	const FObjectivesTendCounts Tends = FObjectivesMetrics::CountTends(Rig.Round.Entries);
	TestEqual(TEXT("two latched NoEffect tends counted (twice, not once)"), Tends.Latched, 2);
	TestEqual(TEXT("both are NoEffect"), Tends.NoEffect, 2);
	TestEqual(TEXT("one FinalCompleted only"), Rig.Round.Count(ERoundLogType::FinalCompleted), 1);
	const FObjectivesStallInput Input = FObjectivesMetrics::StallInputFromLog(Rig.Round.Entries, ActivePhaseStart, ActivePhaseStart + 425.0, false);
	const FObjectivesStallMetrics M = FObjectivesMetrics::ComputeStall(Input, 300.0, 240.0);
	AddInfo(FString::Printf(TEXT("OC17 latched finals: t_end %.1f, FinalDwell %.1f, G_mutual %.1f"), Input.TEnd, M.FinalDwell, M.GMutual));
	TestEqual(TEXT("t_end stays at the win (420 s)"), Input.TEnd, 420.0);
	TestEqual(TEXT("FinalDwell stays 160"), M.FinalDwell, 160.0);
	return true;
}

#endif
