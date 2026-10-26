// Story rmf-047 (production/epics/round-match-framework/story-047-empty-team-no-elimination.md), GDD AC 18 / R15, R16, F2, F3:
//   - a 1v0 dev round (bAllowEmptyTeamDev): when the solo player becomes an Informant and the fake clock jumps
//     10 x TargetRoundMinutes in one jump (not a tick loop) and one drain runs, there is no outcome, the phase is Active
//     and the queue is empty (team 0's Active count is 0, but the rival's RosterCount is 0: no elimination, and no timer
//     ends an Active round);
//   - with the same setup, the solo player (human, Connected, Active) completing the final objective gives ObjectiveWin.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundWinQueueRig.h"

namespace EmptyTeamNoEliminationTestData
{
	FRoundRuleParams DevParams()
	{
		FRoundRuleParams Params = FRoundExitBRig::DefaultParams();
		Params.bAllowEmptyTeamDev = true;
		return Params;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundEmptyTeamNoEliminationTest, "StealthGame.Round.EmptyTeamNoElimination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundEmptyTeamNoEliminationTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace EmptyTeamNoEliminationTestData;
	constexpr uint32 Solo = FRoundExitBRig::HostIdValue;

	// Run 1: the solo player becomes an Informant.
	{
		FRoundWinQueueRig W(DevParams());
		W.ToActive(Layout1v0.Team0, Layout1v0.Team1);
		TestTrue(TEXT("the 1v0 dev round is Active"), W.Rig.Core->GetPhase() == ERoundPhase::Active);
		W.Kill(Solo, ERosterReason::StrikeLimit);
		TestEqual(TEXT("team 0's Active count is 0"), W.ActiveOn(ERosterTeam::Team0), 0);

		// One clock jump of 10 x TargetRoundMinutes, then one drain.
		W.Rig.Clock.Advance(static_cast<double>(EmptyTeamClockJumpMultiple) * TargetRoundMinutes * 60.0, 1);
		W.Rig.Core->Tick();
		W.Queue->Drain();

		TestFalse(TEXT("no outcome (the empty rival never wins by elimination)"), W.Rig.Core->IsTerminated());
		TestTrue(TEXT("phase Active"), W.Rig.Core->GetPhase() == ERoundPhase::Active);
		TestTrue(TEXT("the queue is empty (no item, no timer)"), W.Queue->GetPendingMailboxItemCount() == 0 && W.Queue->GetPendingTimerCount() == 0
			&& W.Queue->GetLastDrainRecords().Num() == 0);
		TestEqual(TEXT("no WinLatched entry"), W.CountLogged(ERoundLogType::WinLatched), 0);
		TestTrue(TEXT("run 1: no unexpected call"), W.Rig.Log.Unexpected.IsEmpty());
	}

	// Run 2: the solo player completes the final objective.
	{
		FRoundWinQueueRig W(DevParams());
		W.ToActive(Layout1v0.Team0, Layout1v0.Team1);
		FRoundObjectiveCompletion Final;
		Final.bFinal = true;
		Final.Team = 0;
		Final.bCompleterActive = Final.bCompleterConnected = Final.bHumanControlled = Final.bCompleterOnTeam = Final.bSiteLive = true;
		TestTrue(TEXT("the final completion is accepted"), W.Rig.Core->SubmitObjectiveCompletion(Final));
		const FRoundTerminalLatch& Latch = W.Rig.Core->GetLatch();
		TestTrue(TEXT("ObjectiveWin(0)"), Latch.IsTerminated() && Latch.GetOutcome().IsWin() && Latch.GetOutcome().WinningTeam == 0
			&& Latch.GetOutcome().EndReason == ERoundEndReason::ObjectiveWin);
		TestTrue(TEXT("Ended with team 0 the winner"), W.Rig.Core->GetPhase() == ERoundPhase::Ended
			&& W.Rig.Core->GetPhaseCore().GetLastResult().WinningTeam == 0 && W.Rig.Core->GetPhaseCore().GetMatchScore(0) == 1);
		TestTrue(TEXT("run 2: no unexpected call"), W.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
