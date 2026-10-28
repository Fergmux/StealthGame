// Objectives Chain story oc-023 (the analyser's T1 fixture test, QA-lead condition): StealthGame.Objectives.PlaytestAnalyserFixture.
// The three ADVISORY reports on fixture rounds: F1 needs >= 20 objective-ended rounds (19 is NOT ENOUGH DATA), a median below
// 6 min triggers the pacing levers and exactly 6 min does not; F2's mean last-stop-to-Complete is in band at 3.0 s and 5.0 s
// and out at 5.01 s, rounds with no counted completion are skipped, probes are counted per stage; F3 reports P_full, P_short
// and counted vs feint separately, labels a simulation, and records P_full high by feinting as a chosen tell.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesPlaytestAnalyser.h"

namespace PlaytestAnalyserFixtureTestData
{
	TArray<FObjectivesPacingRound> PacingRounds(int32 Count, double Duration)
	{
		TArray<FObjectivesPacingRound> Out;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FObjectivesPacingRound& Round = Out.AddDefaulted_GetRef();
			Round.DurationSec = Duration;
			Round.Completions[0] = { 100.0, 220.0, Duration };
			Round.Completions[1] = { 130.0, 260.0 };
		}
		return Out;
	}

	/** A round whose one counted completion's tender stopped TellSec before Complete (walking at 140 cm/s before). */
	FObjectivesTellRound TellRound(double TellSec)
	{
		FObjectivesTellRound Round;
		FObjectivesTellSample& Sample = Round.CountedCompletions.AddDefaulted_GetRef();
		Sample.CompleteTime = 100.0;
		for (double Time = 90.0; Time <= 100.0 + 1e-9; Time += 0.25)
		{
			Sample.SpeedSamples.Emplace(Time, Time < 100.0 - TellSec - 1e-9 ? 140.0 : 0.0);
		}
		return Round;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesPlaytestAnalyserFixtureTest, "StealthGame.Objectives.PlaytestAnalyserFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesPlaytestAnalyserFixtureTest::RunTest(const FString& Parameters)
{
	using namespace PlaytestAnalyserFixtureTestData;
	// F1
	TestFalse(TEXT("19 rounds: NOT ENOUGH DATA"), FObjectivesPlaytestAnalyser::ReportPacing(PacingRounds(19, 300.0), 402.0).bEnoughData);
	const FObjectivesPacingReport Short = FObjectivesPlaytestAnalyser::ReportPacing(PacingRounds(20, 300.0), 402.0);
	TestTrue(TEXT("20 rounds at 5 min: the pacing levers apply"), Short.bEnoughData && Short.bPacingLeversTriggered && Short.MedianDurationSec == 300.0);
	TestEqual(TEXT("the first leg median is 115 s ((100 + 130) / 2)"), Short.MedianLegSec[0], 115.0);
	const FObjectivesPacingReport AtFloor = FObjectivesPlaytestAnalyser::ReportPacing(PacingRounds(20, 360.0), 402.0);
	TestFalse(TEXT("a median of exactly 6 min is not flagged"), AtFloor.bPacingLeversTriggered);
	TArray<FObjectivesPacingRound> WithExcluded = PacingRounds(20, 400.0);
	WithExcluded[0].bExcluded = true;
	TestFalse(TEXT("an excluded round does not count toward the 20"), FObjectivesPlaytestAnalyser::ReportPacing(WithExcluded, 402.0).bEnoughData);

	// F2: means of 3.0 s and 5.0 s are in band, 5.01 s is not (E_tell 4 s = t_settle 1 + D 3).
	for (const TPair<double, bool>& Case : TArray<TPair<double, bool>>{ { 3.0, true }, { 5.0, true }, { 5.01, false } })
	{
		TArray<FObjectivesTellRound> Rounds;
		for (int32 Index = 0; Index < 10; ++Index)
		{
			FObjectivesTellRound Round = TellRound(Case.Key == 5.01 ? 5.0 : Case.Key);
			if (Case.Key == 5.01)
			{
				Round.CountedCompletions[0].CompleteTime = 100.01; // the stop stays at 95.0
			}
			Rounds.Add(Round);
		}
		Rounds.AddDefaulted(); // a round with no counted completion: skipped
		const FObjectivesTellReport Tell = FObjectivesPlaytestAnalyser::ReportTellTiming(Rounds, 10.0, 4.0, 1.0);
		AddInfo(FString::Printf(TEXT("OC23 tell case %.2f s: mean %.3f s, in band %d, rounds used %d"), Case.Key, Tell.MeanTellSec, Tell.bInBand ? 1 : 0, Tell.RoundsUsed));
		TestEqual(*FString::Printf(TEXT("tell %.2f s: band verdict"), Case.Key), Tell.bInBand, Case.Value);
		TestEqual(TEXT("the empty round is skipped"), Tell.RoundsUsed, 10);
	}
	TArray<FObjectivesTellRound> Nine;
	Nine.Init(TellRound(4.0), 9);
	TestFalse(TEXT("9 rounds: NOT ENOUGH DATA"), FObjectivesPlaytestAnalyser::ReportTellTiming(Nine, 10.0, 4.0, 1.0).bEnoughData);
	FObjectivesTellRound ProbeRound = TellRound(4.0);
	FObjectivesTendAttempt Probe;
	Probe.Stage = 1;
	Probe.StartTime = 10.0;
	Probe.EndTime = 11.5;
	ProbeRound.Attempts.Add(Probe);
	Probe.EndTime = 10.5; // released before TendCueDelaySec: not a probe
	ProbeRound.Attempts.Add(Probe);
	TArray<FObjectivesTellRound> Probes;
	Probes.Init(ProbeRound, 10);
	TestEqual(TEXT("probes counted per stage (stage 2: one per round)"), FObjectivesPlaytestAnalyser::ReportTellTiming(Probes, 10.0, 4.0, 1.0).ProbesPerStage[1], 10);

	// F3: 70 player and 30 NPC full-length tends -> 0.70; 95 / 5 early releases -> 0.95 (alarm); 40 counted / 30 feint.
	TArray<FObjectivesTellRound> ShareRounds;
	ShareRounds.AddDefaulted(60);
	auto Add = [&ShareRounds](int32 Count, bool bFull, bool bPlayer, bool bCounted)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FObjectivesTendAttempt Attempt;
			Attempt.bFullLength = bFull;
			Attempt.bByPlayer = bPlayer;
			Attempt.bCounted = bCounted;
			ShareRounds[Index % ShareRounds.Num()].Attempts.Add(Attempt);
		}
	};
	Add(40, true, true, true);
	Add(30, true, true, false);
	Add(30, true, false, false);
	Add(95, false, true, false);
	Add(5, false, false, false);
	const FObjectivesShareReport Shares = FObjectivesPlaytestAnalyser::ReportShares(ShareRounds, /*bSimulation*/ true);
	TestTrue(TEXT("P_full 0.70 in band"), FMath::IsNearlyEqual(Shares.PFull, 0.70, 1e-9) && Shares.bPFullInBand);
	TestTrue(TEXT("P_short 0.95 raises the alarm"), FMath::IsNearlyEqual(Shares.PShort, 0.95, 1e-9) && Shares.bPShortAlarm);
	TestTrue(TEXT("counted vs feint reported separately"), Shares.PlayerCounted == 40 && Shares.PlayerFeint == 30);
	const FString Report = FObjectivesPlaytestAnalyser::ToMarkdown(Short, FObjectivesPlaytestAnalyser::ReportTellTiming(Nine, 10.0, 4.0, 1.0), Shares);
	TestTrue(TEXT("the simulation is labelled"), Report.Contains(TEXT("SIMULATION, not a playtest")));
	TestTrue(TEXT("too few rounds report NOT ENOUGH DATA"), Report.Contains(TEXT("NOT ENOUGH DATA")));
	TArray<FObjectivesTellRound> Feinty = ShareRounds;
	for (FObjectivesTellRound& Round : Feinty)
	{
		Round.Attempts.RemoveAll([](const FObjectivesTendAttempt& A) { return !A.bByPlayer && A.bFullLength; });
	}
	const FObjectivesShareReport High = FObjectivesPlaytestAnalyser::ReportShares(Feinty, false);
	TestFalse(TEXT("feint-heavy: P_full above the band"), High.bPFullInBand);
	TestFalse(TEXT("but 40 counted vs 30 feint is not feint-dominated"), High.bHighByFeinting);
	Add(20, true, true, false); // now 40 counted vs 50 feint
	Feinty = ShareRounds;
	for (FObjectivesTellRound& Round : Feinty)
	{
		Round.Attempts.RemoveAll([](const FObjectivesTendAttempt& A) { return !A.bByPlayer && A.bFullLength; });
	}
	TestTrue(TEXT("P_full high by feinting is recorded as a chosen tell"), FObjectivesPlaytestAnalyser::ReportShares(Feinty, false).bHighByFeinting);
	return true;
}

#endif
