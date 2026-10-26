// Story rmf-016 (production/epics/round-match-framework/story-016-pacing-metrics.md): GDD AC 35, F7.
// From fixture logs: PacingRatio = 1.3 for 936 s; ObjectiveEndShare = 0.7 for 7 objective and 3 elimination wins;
// AbortedActiveShare = 2/12 with 2 aborts; forfeits excluded from ObjectiveEndShare; 1080.0 s is not a stall and
// 1080.001 s is at StallFlagMinutes 18; an Active abort at 540.0 s is not flagged and at 540.001 s is; shares per
// team-size bucket (a 2v2 round is its own bucket and does not count for the Pillar 4 gate of >= 0.70 over >= 20
// rounds in 4v4 and larger); DebugForced rounds excluded; an empty set reports "n/a".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/EventLog/StealthRoundPacingMetrics.h"
#include "Server/Testing/StealthRoundLogTestSupport.h"

namespace PacingMetricsTestData
{
	constexpr double ActiveStartSec = 100.0;
	constexpr double RatioTolerance = 1e-9;

	/** A fixture round log (header, Active entry, WinLatched) read back into a pacing input. */
	FRoundPacingInput RoundFromLog(double DurationSec, ERoundEndReason Reason, int32 Team0 = 5, int32 Team1 = 5, bool bDebug = false)
	{
		using namespace RoundLogFixtures;
		FStealthRoundLogRig Rig;
		Rig.Log.WriteHeader(MakeHeader(Team0, Team1));
		Rig.Log.Append(MakePhaseChanged(PhasePreparing, PhaseActive, FQueueKey(10, 0), ActiveStartSec));
		Rig.Log.Append(MakeWinLatched(Reason, 0, FQueueKey(20, 1), ActiveStartSec + DurationSec, bDebug));
		FRoundPacingInput Input;
		StealthRoundPacing::MakeInputFromLog(Rig.Log, Input);
		return Input;
	}

	FRoundPacingTuning Tuning()
	{
		FRoundPacingTuning T;
		T.TargetRoundMinutes = RoundTestFixtures::TargetRoundMinutes;
		T.StallFlagMinutes = RoundTestFixtures::StallFlagMinutes;
		return T;
	}

	FRoundPillar4GateSpec GateSpec()
	{
		FRoundPillar4GateSpec Spec;
		Spec.MinRounds = RoundTestFixtures::Pillar4MinRounds;
		Spec.MinShare = RoundTestFixtures::Pillar4MinShare;
		return Spec;
	}

	void AddRounds(TArray<FRoundPacingInput>& Rounds, int32 Count, ERoundEndReason Reason, int32 Team0 = 5, int32 Team1 = 5, bool bDebug = false)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Rounds.Add(RoundFromLog(600.0, Reason, Team0, Team1, bDebug));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundPacingMetricsTest, "StealthGame.Round.PacingMetrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundPacingMetricsTest::RunTest(const FString& Parameters)
{
	using namespace PacingMetricsTestData;
	using namespace RoundTestFixtures;

	// Per-round values from fixture logs.
	{
		FRoundPacingInput Input;
		FStealthRoundLogRig Empty;
		TestFalse(TEXT("a log with no WinLatched yields no input"), StealthRoundPacing::MakeInputFromLog(Empty.Log, Input));

		// Regression (code review): a long round evicts the unprotected PhaseChanged row; DurationSec survives.
		FStealthRoundLogRig Long(FRoundEventLog::MaxEntriesHardMin);
		Long.Log.WriteHeader(RoundLogFixtures::MakeHeader());
		Long.Log.Append(RoundLogFixtures::MakePhaseChanged(RoundLogFixtures::PhasePreparing, RoundLogFixtures::PhaseActive, FQueueKey(10, 0), ActiveStartSec));
		for (int32 Index = 0; Index < FRoundEventLog::MaxEntriesHardMin * 2; ++Index)
		{
			Long.Log.Append(RoundLogFixtures::MakeEntry(ERoundLogType::RespawnScheduled, FQueueKey(11 + Index, 1), ActiveStartSec + Index));
		}
		Long.Log.Append(RoundLogFixtures::MakeWinLatched(ERoundEndReason::ObjectiveWin, 0, FQueueKey(5000, 1), ActiveStartSec + PacingDurationSec));
		TestTrue(TEXT("the PhaseChanged row was evicted"), Long.Log.GetTruncatedCount(ERoundLogType::PhaseChanged) == 1);
		FRoundPacingInput LongInput;
		TestTrue(TEXT("the input is still read after the Active row's eviction"), StealthRoundPacing::MakeInputFromLog(Long.Log, LongInput));
		TestEqual(TEXT("DurationSec survives the eviction"), LongInput.DurationSec, PacingDurationSec, RatioTolerance);

		const FRoundPacingInput Round = RoundFromLog(PacingDurationSec, ERoundEndReason::ObjectiveWin);
		TestEqual(TEXT("DurationSec read from the log"), Round.DurationSec, PacingDurationSec, RatioTolerance);
		TestEqual(TEXT("PacingRatio = 1.3 for 936 s"), StealthRoundPacing::ComputeRound(Round, Tuning()).PacingRatio, PacingExpectedRatio, RatioTolerance);
		TestEqual(TEXT("bucket = min(5, 5)"), StealthRoundPacing::ComputeRound(Round, Tuning()).Bucket, 5);

		TestFalse(TEXT("1080.0 s is not a stall"), StealthRoundPacing::ComputeRound(RoundFromLog(StallBoundarySec, ERoundEndReason::Elimination), Tuning()).bStallFlag);
		TestTrue(TEXT("1080.001 s is a stall at StallFlagMinutes 18"), StealthRoundPacing::ComputeRound(RoundFromLog(StallJustAboveSec, ERoundEndReason::Elimination), Tuning()).bStallFlag);
		TestFalse(TEXT("an Active abort at 540.0 s is not flagged"), StealthRoundPacing::ComputeRound(RoundFromLog(AbortBoundarySec, ERoundEndReason::Aborted), Tuning()).bAbortFlagged);
		TestTrue(TEXT("an Active abort at 540.001 s is flagged"), StealthRoundPacing::ComputeRound(RoundFromLog(AbortJustAboveSec, ERoundEndReason::Aborted), Tuning()).bAbortFlagged);
		TestFalse(TEXT("a win at 540.001 s is not an abort flag"), StealthRoundPacing::ComputeRound(RoundFromLog(AbortJustAboveSec, ERoundEndReason::ObjectiveWin), Tuning()).bAbortFlagged);
	}

	// Shares: 7 objective wins, 3 eliminations and 2 aborts in 5v5.
	{
		TArray<FRoundPacingInput> Rounds;
		AddRounds(Rounds, ObjectiveWinRounds, ERoundEndReason::ObjectiveWin);
		AddRounds(Rounds, EliminationRounds, ERoundEndReason::Elimination);
		AddRounds(Rounds, AbortRounds, ERoundEndReason::Aborted);
		const FRoundPacingAggregate Agg = StealthRoundPacing::Aggregate(Rounds, GateSpec());
		TestTrue(TEXT("ObjectiveEndShare = 0.7 for 7 objective and 3 elimination wins"), Agg.ObjectiveEndShareAll.IsAvailable() && FMath::IsNearlyEqual(Agg.ObjectiveEndShareAll.Value(), ExpectedObjectiveEndShare, RatioTolerance));
		TestTrue(TEXT("AbortedActiveShare = 2/12 with 2 aborts"), Agg.AbortedActiveShareAll.Numerator == 2 && Agg.AbortedActiveShareAll.Denominator == 12);

		// Forfeits are excluded from ObjectiveEndShare (they still count in the abort share's denominator).
		TArray<FRoundPacingInput> WithForfeits = Rounds;
		AddRounds(WithForfeits, 4, ERoundEndReason::ForfeitElimination);
		const FRoundPacingAggregate AggF = StealthRoundPacing::Aggregate(WithForfeits, GateSpec());
		TestTrue(TEXT("forfeits leave ObjectiveEndShare at 0.7"), FMath::IsNearlyEqual(AggF.ObjectiveEndShareAll.Value(), ExpectedObjectiveEndShare, RatioTolerance) && AggF.ObjectiveEndShareAll.Denominator == 10);
		TestEqual(TEXT("forfeits enter the AbortedActiveShare denominator"), AggF.AbortedActiveShareAll.Denominator, 16);

		// DebugForced (and Debug-flagged) rounds are excluded from every share.
		TArray<FRoundPacingInput> WithDebug = Rounds;
		AddRounds(WithDebug, 3, ERoundEndReason::DebugForced);
		AddRounds(WithDebug, 3, ERoundEndReason::Elimination, 5, 5, true);
		const FRoundPacingAggregate AggD = StealthRoundPacing::Aggregate(WithDebug, GateSpec());
		TestTrue(TEXT("DebugForced and Debug-flagged rounds are excluded"), AggD.ObjectiveEndShareAll.Denominator == 10 && AggD.AbortedActiveShareAll.Denominator == 12);

		// PreparingFailed and SessionEnded rounds enter no count.
		TArray<FRoundPacingInput> WithOthers = Rounds;
		AddRounds(WithOthers, 2, ERoundEndReason::PreparingFailed);
		AddRounds(WithOthers, 1, ERoundEndReason::SessionEnded);
		const FRoundPacingAggregate AggO = StealthRoundPacing::Aggregate(WithOthers, GateSpec());
		TestEqual(TEXT("PreparingFailed and SessionEnded enter no count"), AggO.AbortedActiveShareAll.Denominator, 12);
	}

	// Buckets and the Pillar 4 gate.
	{
		TArray<FRoundPacingInput> Rounds;
		AddRounds(Rounds, 15, ERoundEndReason::ObjectiveWin, 4, 4);
		AddRounds(Rounds, 5, ERoundEndReason::Elimination, 5, 4);
		const FRoundPacingAggregate Base = StealthRoundPacing::Aggregate(Rounds, GateSpec());
		TestTrue(TEXT("20 rounds at 0.75 in 4v4+ pass the Pillar 4 gate"), Base.Pillar4 == ERoundPillar4Gate::Pass);
		TestTrue(TEXT("bucket 4 holds the 4v4 and 5v4 rounds"), Base.ObjectiveEndShareByBucket.Contains(4) && Base.ObjectiveEndShareByBucket[4].Denominator == 20);

		TArray<FRoundPacingInput> With2v2 = Rounds;
		AddRounds(With2v2, 10, ERoundEndReason::Elimination, 2, 2);
		const FRoundPacingAggregate Agg2 = StealthRoundPacing::Aggregate(With2v2, GateSpec());
		TestTrue(TEXT("2v2 rounds sit in their own bucket"), Agg2.ObjectiveEndShareByBucket.Contains(2) && Agg2.ObjectiveEndShareByBucket[2].Denominator == 10);
		TestTrue(TEXT("adding 2v2 rounds does not change the gate"), Agg2.Pillar4 == Base.Pillar4 && Agg2.Pillar4Share.Denominator == Base.Pillar4Share.Denominator);

		TArray<FRoundPacingInput> Few;
		AddRounds(Few, 19, ERoundEndReason::ObjectiveWin, 5, 5);
		TestTrue(TEXT("fewer than 20 rounds: insufficient for the gate"), StealthRoundPacing::Aggregate(Few, GateSpec()).Pillar4 == ERoundPillar4Gate::InsufficientRounds);

		TArray<FRoundPacingInput> Low;
		AddRounds(Low, 13, ERoundEndReason::ObjectiveWin, 5, 5);
		AddRounds(Low, 7, ERoundEndReason::Elimination, 5, 5);
		TestTrue(TEXT("0.65 over 20 rounds fails the gate"), StealthRoundPacing::Aggregate(Low, GateSpec()).Pillar4 == ERoundPillar4Gate::Fail);
	}

	// An empty set reports "n/a", never 0.
	{
		const FRoundPacingAggregate Empty = StealthRoundPacing::Aggregate(TConstArrayView<FRoundPacingInput>(), GateSpec());
		TestFalse(TEXT("empty set: ObjectiveEndShare is n/a"), Empty.ObjectiveEndShareAll.IsAvailable());
		TestFalse(TEXT("empty set: AbortedActiveShare is n/a"), Empty.AbortedActiveShareAll.IsAvailable());
		TestTrue(TEXT("empty set: no bucket"), Empty.ObjectiveEndShareByBucket.Num() == 0);
		TArray<FRoundPacingInput> OnlyAborts;
		AddRounds(OnlyAborts, 2, ERoundEndReason::Aborted);
		TestFalse(TEXT("only aborts: ObjectiveEndShare is n/a (denominator 0)"), StealthRoundPacing::Aggregate(OnlyAborts, GateSpec()).ObjectiveEndShareAll.IsAvailable());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
