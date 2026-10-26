// Story rmf-053 (production/epics/round-match-framework/story-053-strike-win-staging.md), GDD AC 12c / R9, R15;
// ADR-0002 sections 6, 8.2-8.4. A strike decides the round (T_v 0.40 s, StrikeNetLeadSec 0.15 s: death write 0.25 s after
// the slot; and a BushStrike, 0.75 s: 0.60 s). The server steps are recorded:
//   - latch step: only Terminated, the queue close and Rejected_Latched results; between the latch and the death write
//     the public state is unchanged (phase Active, score unchanged), no EndRoundAll (no Roster record RoundEnded), no
//     Halt or stop request (pawn velocities untouched by Round), no hold cancel and no held reply; a command submitted in
//     that window is Rejected_Latched;
//   - the death write's server step: phase Ended, FRoundResult, MatchScore[Winner] + 1, EndRoundAll, Halt, one stop
//     request per live pawn, the hold cancels and the held replies all occur, and EndedMinDwellSec counts from it;
//   - an earlier strike's death write due after that step still writes FDeathState (the DeathWrite applier runs).
// Fixture times come from the constants file (story rmf-005).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

namespace StrikeWinStagingTestData
{
	/** The calls Round makes only in the round-end step. */
	const TCHAR* const RoundEndCalls[] = { TEXT("EndRoundAll"), TEXT("Halt"), TEXT("RequestPawnStop"), TEXT("ReleaseHeldReplies"), TEXT("CancelFrozenHolds") };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundStrikeWinStagingTest, "StealthGame.Round.StrikeWinStaging",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundStrikeWinStagingTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace StrikeWinStagingTestData;
	for (const double Tv : { StrikeTvSec, BushStrikeTvSec })
	{
		const double Lead = Tv - StrikeNetLeadSec;
		const FString Run = FString::Printf(TEXT("T_v %.2f"), Tv);

		// Arrange: 2v2 (1, 2 team 0; 3, 4 team 1); three live pawns besides the players' (the stop set fixture).
		FRoundEndRig R;
		R.ToActive(2, 2);
		R.ScriptRoundEnd();
		R.Rig.Fakes.Spawner.LivePawnList = {
			{ FPawnHandle(), 101, ERoundPawnDriver::HumanDriven }, { FPawnHandle(), 102, ERoundPawnDriver::AiDriven },
			{ FPawnHandle(), 201, ERoundPawnDriver::Npc }, { FPawnHandle(), 202, ERoundPawnDriver::Npc } };
		// An earlier strike: 3 falls with a long (BushStrike) death write; team 1 keeps one Active member.
		R.DrainOneTick({ R.Strike(1, 1, 3, BushStrikeTvSec) });
		const double EarlierDeath = R.Rig.Clock.GetServerTimeSeconds() + (BushStrikeTvSec - StrikeNetLeadSec);
		TestEqual(*FString::Printf(TEXT("%s: the earlier strike's death write is pending"), *Run), R.Queue->GetPendingStagedWrites().Num(), 1);
		R.Frame();
		const FRoundPublicState Before = R.Rig.Core->GetPhaseCore().MakePublicState();
		const int32 Mark = R.Rig.Mark();

		// Act 1: the deciding strike (2 on 4, team 1's last Active member) in its slot.
		R.DrainOneTick({ R.Strike(2, 2, 4, Tv) });
		const double SlotTime = R.Rig.Clock.GetServerTimeSeconds();

		// Assert 1: the latch step only.
		TestTrue(*FString::Printf(TEXT("%s: latched (Terminated)"), *Run), R.Rig.Core->IsTerminated() && R.Rig.Core->IsRoundEndPending());
		TestTrue(*FString::Printf(TEXT("%s: the queue is closed"), *Run), R.Queue->IsLatched());
		TestTrue(*FString::Printf(TEXT("%s: the outcome is EliminationWin(0)"), *Run), R.Rig.Core->GetLatch().GetOutcome() == FRoundTerminalOutcome::Win(0, ERoundEndReason::Elimination));
		TestTrue(*FString::Printf(TEXT("%s: the public state is unchanged (Active, score)"), *Run), R.Rig.Core->GetPhaseCore().MakePublicState() == Before);
		for (const TCHAR* Call : RoundEndCalls)
		{
			TestEqual(*FString::Printf(TEXT("%s: latch step: no %s"), *Run, Call), R.Rig.CountSince(Mark, Call), 0);
		}

		// The window between the two steps: a command is Rejected_Latched; still nothing observable changes.
		const int32 RejectedBefore = R.CountLogged(ERoundLogType::Rejected_Latched);
		R.DrainOneTick({ R.Strike(3, 1, 4) });
		TestEqual(*FString::Printf(TEXT("%s: a command in the window is Rejected_Latched"), *Run), R.CountLogged(ERoundLogType::Rejected_Latched) - RejectedBefore, 1);
		R.RunUntil(SlotTime + Lead - RigFrameSec);
		TestTrue(*FString::Printf(TEXT("%s: one frame before the death write: still Active"), *Run), R.Rig.Core->GetPhase() == ERoundPhase::Active);
		TestTrue(*FString::Printf(TEXT("%s: before the death write: public state unchanged"), *Run), R.Rig.Core->GetPhaseCore().MakePublicState() == Before);
		for (const TCHAR* Call : RoundEndCalls)
		{
			TestEqual(*FString::Printf(TEXT("%s: before the death write: no %s"), *Run, Call), R.Rig.CountSince(Mark, Call), 0);
		}

		// Act 2: the death write's server step.
		R.RunUntil(SlotTime + Lead);

		// Assert 2: everything observable in that one step.
		const FRoundPublicState After = R.Rig.Core->GetPhaseCore().MakePublicState();
		TestTrue(*FString::Printf(TEXT("%s: Ended at the death write"), *Run), After.Phase == ERoundPhase::Ended);
		TestEqual(*FString::Printf(TEXT("%s: the step is the death write's (%.2f s after the slot)"), *Run, Lead), After.PhaseEnteredServerTime - SlotTime, Lead, 1e-6);
		TestTrue(*FString::Printf(TEXT("%s: FRoundResult elimination, team 0"), *Run), After.LastResult.WinningTeam == 0 && After.LastResult.PublicReason == RoundPublicReason::Elimination);
		TestTrue(*FString::Printf(TEXT("%s: MatchScore[0] + 1"), *Run), After.MatchScore[0] == Before.MatchScore[0] + 1 && After.MatchScore[1] == Before.MatchScore[1]);
		TestEqual(*FString::Printf(TEXT("%s: EndRoundAll"), *Run), R.Rig.CountSince(Mark, TEXT("EndRoundAll")), 1);
		TestEqual(*FString::Printf(TEXT("%s: Halt"), *Run), R.Rig.CountSince(Mark, TEXT("Halt")), 1);
		TestEqual(*FString::Printf(TEXT("%s: one stop request per live pawn"), *Run), R.Rig.CountSince(Mark, TEXT("RequestPawnStop")), R.Rig.Fakes.Spawner.LivePawnList.Num());
		TestEqual(*FString::Printf(TEXT("%s: the held replies"), *Run), R.Rig.CountSince(Mark, TEXT("ReleaseHeldReplies")), 1);
		TestEqual(*FString::Printf(TEXT("%s: the hold cancels"), *Run), R.Rig.CountSince(Mark, TEXT("CancelFrozenHolds")), 1);
		const FRoundRecordingApplier::FApplied* Deciding = R.Deaths.Applied.FindByPredicate([&](const FRoundRecordingApplier::FApplied& A)
		{
			return FMath::IsNearlyEqual(A.Write.ServerTime, SlotTime + Lead, 1e-6);
		});
		TestTrue(*FString::Printf(TEXT("%s: the deciding death write applied in the same step, before the round-end step"), *Run),
			Deciding != nullptr && FMath::IsNearlyEqual(Deciding->AppliedAt, After.PhaseEnteredServerTime, 1e-6) && Deciding->PhaseAtApply == ERoundPhase::Active);

		// EndedMinDwellSec counts from the round-end step.
		TestTrue(*FString::Printf(TEXT("%s: a reset before the dwell is held"), *Run), R.Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Reset) == ERoundCommandResult::Held);
		R.ScriptReset();
		R.RunUntil(After.PhaseEnteredServerTime + EndedMinDwellSec - RigFrameSec);
		TestTrue(*FString::Printf(TEXT("%s: held until the dwell from the round-end step"), *Run), R.Rig.Core->GetPhase() == ERoundPhase::Ended);

		// The earlier strike's death write, due after the round-end step, still plays (T_v 0.40 run: 0.60 > 0.25).
		if (EarlierDeath > After.PhaseEnteredServerTime)
		{
			const FRoundRecordingApplier::FApplied* Earlier = R.Deaths.Applied.FindByPredicate([&](const FRoundRecordingApplier::FApplied& A)
			{
				return FMath::IsNearlyEqual(A.Write.ServerTime, EarlierDeath, 1e-6);
			});
			TestTrue(*FString::Printf(TEXT("%s: the earlier death write still writes FDeathState after Ended"), *Run), Earlier != nullptr && Earlier->PhaseAtApply == ERoundPhase::Ended);
		}
		R.RunUntil(After.PhaseEnteredServerTime + EndedMinDwellSec + RigFrameSec);
		TestTrue(*FString::Printf(TEXT("%s: the held reset ran at the dwell's end"), *Run), R.Rig.Core->GetPhase() == ERoundPhase::Reset);
		TestTrue(*FString::Printf(TEXT("%s: no fallback binding (a defect) was needed"), *Run), R.Support.Faults.GetRecords().IsEmpty());
		TestTrue(*FString::Printf(TEXT("%s: no unexpected call"), *Run), R.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
