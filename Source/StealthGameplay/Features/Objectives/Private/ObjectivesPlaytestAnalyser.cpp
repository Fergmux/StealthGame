// Objectives Chain story oc-023: the offline playtest analyser (F1 pacing, F2 tell timing, F3 measured shares). See
// ObjectivesPlaytestAnalyser.h.

#include "ObjectivesPlaytestAnalyser.h"

double FObjectivesPlaytestAnalyser::Median(TArray<double> Values)
{
	if (Values.Num() == 0)
	{
		return 0.0;
	}
	Values.Sort();
	const int32 Mid = Values.Num() / 2;
	return Values.Num() % 2 == 1 ? Values[Mid] : 0.5 * (Values[Mid - 1] + Values[Mid]);
}

FObjectivesPacingReport FObjectivesPlaytestAnalyser::ReportPacing(TConstArrayView<FObjectivesPacingRound> Rounds, double TChainSec)
{
	FObjectivesPacingReport Out;
	Out.TChainSec = TChainSec;
	TArray<double> Durations;
	TArray<double> Legs[3];
	for (const FObjectivesPacingRound& Round : Rounds)
	{
		if (!Round.bObjectiveWin || Round.bExcluded)
		{
			continue;
		}
		Durations.Add(Round.DurationSec);
		for (int32 Team = 0; Team < 2; ++Team)
		{
			TArray<double> Times = Round.Completions[Team];
			Times.Sort();
			double Previous = 0.0;
			for (int32 Leg = 0; Leg < FMath::Min(3, Times.Num()); ++Leg)
			{
				Legs[Leg].Add(Times[Leg] - Previous);
				Previous = Times[Leg];
			}
		}
	}
	Out.Rounds = Durations.Num();
	Out.bEnoughData = Out.Rounds >= PacingMinRounds;
	if (Out.bEnoughData)
	{
		Out.MedianDurationSec = Median(Durations);
		Out.bPacingLeversTriggered = Out.MedianDurationSec < PacingFloorSec;
		for (int32 Leg = 0; Leg < 3; ++Leg)
		{
			Out.MedianLegSec[Leg] = Legs[Leg].Num() > 0 ? Median(Legs[Leg]) : -1.0;
		}
	}
	return Out;
}

FObjectivesTellReport FObjectivesPlaytestAnalyser::ReportTellTiming(TConstArrayView<FObjectivesTellRound> Rounds, double WalkThresholdCmPerSec, double ETellSec, double TendCueDelaySec)
{
	FObjectivesTellReport Out;
	double Sum = 0.0;
	int32 Count = 0;
	for (const FObjectivesTellRound& Round : Rounds)
	{
		for (const FObjectivesTendAttempt& Attempt : Round.Attempts)
		{
			if (Attempt.bByPlayer && !Attempt.bFullLength && !Attempt.bAtOwnLiveCourt && Attempt.EndTime - Attempt.StartTime >= TendCueDelaySec && Attempt.Stage >= 0 && Attempt.Stage < 3)
			{
				++Out.ProbesPerStage[Attempt.Stage];
			}
		}
		if (Round.CountedCompletions.Num() == 0)
		{
			continue; // a round with no counted completion is skipped
		}
		++Out.RoundsUsed;
		for (const FObjectivesTellSample& Sample : Round.CountedCompletions)
		{
			// The last stop: walking back from Complete, the earliest sample of the final run below the threshold.
			double LastStop = Sample.CompleteTime;
			TArray<TPair<double, double>> Sorted = Sample.SpeedSamples;
			Sorted.Sort([](const TPair<double, double>& A, const TPair<double, double>& B) { return A.Key < B.Key; });
			for (int32 Index = Sorted.Num() - 1; Index >= 0; --Index)
			{
				if (Sorted[Index].Key > Sample.CompleteTime)
				{
					continue;
				}
				if (Sorted[Index].Value >= WalkThresholdCmPerSec)
				{
					break;
				}
				LastStop = Sorted[Index].Key;
			}
			Sum += Sample.CompleteTime - LastStop;
			++Count;
		}
	}
	Out.bEnoughData = Out.RoundsUsed >= TellMinRounds;
	Out.MeanTellSec = Count > 0 ? Sum / Count : 0.0;
	Out.bInBand = Out.bEnoughData && FMath::Abs(Out.MeanTellSec - ETellSec) <= 1.0 + 1e-9;
	return Out;
}

FObjectivesShareReport FObjectivesPlaytestAnalyser::ReportShares(TConstArrayView<FObjectivesTellRound> Rounds, bool bSimulation)
{
	FObjectivesShareReport Out;
	Out.Rounds = Rounds.Num();
	Out.bSimulation = bSimulation;
	Out.bEnoughData = Out.Rounds >= ShareMinRounds;
	int32 FullPlayer = 0;
	int32 FullNpc = 0;
	int32 ShortPlayer = 0;
	int32 ShortNpc = 0;
	for (const FObjectivesTellRound& Round : Rounds)
	{
		for (const FObjectivesTendAttempt& Attempt : Round.Attempts)
		{
			if (Attempt.bFullLength)
			{
				(Attempt.bByPlayer ? FullPlayer : FullNpc) += 1;
				if (Attempt.bByPlayer)
				{
					(Attempt.bCounted ? Out.PlayerCounted : Out.PlayerFeint) += 1;
				}
			}
			else
			{
				(Attempt.bByPlayer ? ShortPlayer : ShortNpc) += 1;
			}
		}
	}
	Out.PFull = FullPlayer + FullNpc > 0 ? static_cast<double>(FullPlayer) / (FullPlayer + FullNpc) : 0.0;
	Out.PShort = ShortPlayer + ShortNpc > 0 ? static_cast<double>(ShortPlayer) / (ShortPlayer + ShortNpc) : 0.0;
	Out.bPFullInBand = FObjectivesMetrics::IsInPosteriorBand(Out.PFull);
	Out.bPShortAlarm = Out.PShort > ShortAlarm;
	Out.bHighByFeinting = Out.PFull > FObjectivesMetrics::PosteriorBandHigh && Out.PlayerFeint > Out.PlayerCounted;
	return Out;
}

FString FObjectivesPlaytestAnalyser::ToMarkdown(const FObjectivesPacingReport& Pacing, const FObjectivesTellReport& Tell, const FObjectivesShareReport& Shares)
{
	FString Out;
	Out += TEXT("## F1 pacing (AC-OC-19, ADVISORY)\n\n");
	if (!Pacing.bEnoughData)
	{
		Out += FString::Printf(TEXT("NOT ENOUGH DATA: %d objective-ended round(s), %d needed. T_chain %.0f s.\n\n"), Pacing.Rounds, PacingMinRounds, Pacing.TChainSec);
	}
	else
	{
		Out += FString::Printf(TEXT("Rounds %d; median ObjectiveWin duration %.1f s (%.2f min) vs T_chain %.0f s; legs (median) %.1f / %.1f / %.1f s; %s\n\n"),
			Pacing.Rounds, Pacing.MedianDurationSec, Pacing.MedianDurationSec / 60.0, Pacing.TChainSec, Pacing.MedianLegSec[0], Pacing.MedianLegSec[1], Pacing.MedianLegSec[2],
			Pacing.bPacingLeversTriggered ? TEXT("below 6 min: the F1 pacing levers apply") : TEXT("not below 6 min"));
	}
	Out += TEXT("## F2 tell timing (AC-OC-20, ADVISORY)\n\n");
	if (!Tell.bEnoughData)
	{
		Out += FString::Printf(TEXT("NOT ENOUGH DATA: %d round(s) with a counted completion, %d needed.\n\n"), Tell.RoundsUsed, TellMinRounds);
	}
	else
	{
		Out += FString::Printf(TEXT("Rounds %d; mean last-stop-to-Complete %.2f s (%s); probes per stage %d / %d / %d\n\n"), Tell.RoundsUsed, Tell.MeanTellSec,
			Tell.bInBand ? TEXT("in E_tell +- 1 s") : TEXT("OUT of E_tell +- 1 s"), Tell.ProbesPerStage[0], Tell.ProbesPerStage[1], Tell.ProbesPerStage[2]);
	}
	Out += FString::Printf(TEXT("## F3 measured share (AC-OC-21, ADVISORY)%s\n\n"), Shares.bSimulation ? TEXT(" - SIMULATION, not a playtest") : TEXT(""));
	if (!Shares.bEnoughData)
	{
		Out += FString::Printf(TEXT("NOT ENOUGH DATA: %d round(s), %d needed.\n"), Shares.Rounds, ShareMinRounds);
	}
	else
	{
		Out += FString::Printf(TEXT("Rounds %d; P_full %.3f (%s); P_short %.3f%s; player tends counted %d, feint %d%s\n"), Shares.Rounds, Shares.PFull,
			Shares.bPFullInBand ? TEXT("in 0.65-0.75") : TEXT("outside 0.65-0.75"), Shares.PShort, Shares.bPShortAlarm ? TEXT(" (ALARM above 0.90)") : TEXT(""),
			Shares.PlayerCounted, Shares.PlayerFeint, Shares.bHighByFeinting ? TEXT("; P_full high by feinting: a chosen tell, not a defect") : TEXT(""));
	}
	return Out;
}
