// Objectives Chain story oc-017: the F5 stall metrics and the F3 tend posterior. See ObjectivesMetrics.h.

#include "ObjectivesMetrics.h"

#include "View/ObjectiveChainData.h"

namespace ObjectivesMetricsLocal
{
	/** The largest gap of Times with 0 and TEnd included (Times may be unsorted; times past TEnd are clamped to it). */
	double LargestGap(TArray<double> Times, double TEnd)
	{
		Times.Add(0.0);
		Times.Add(TEnd);
		for (double& Time : Times)
		{
			Time = FMath::Clamp(Time, 0.0, TEnd);
		}
		Times.Sort();
		double Largest = 0.0;
		for (int32 Index = 1; Index < Times.Num(); ++Index)
		{
			Largest = FMath::Max(Largest, Times[Index] - Times[Index - 1]);
		}
		return Largest;
	}
}

FObjectivesStallMetrics FObjectivesMetrics::ComputeStall(const FObjectivesStallInput& Input, double StallGapDefectSec, double FinalDwellDefectSec)
{
	FObjectivesStallMetrics Out;
	if (Input.bExcludeFromStallMetrics)
	{
		Out.bExcluded = true;
		return Out;
	}
	TArray<double> Merged = Input.Completions[0];
	Merged.Append(Input.Completions[1]);
	Out.GMutual = ObjectivesMetricsLocal::LargestGap(Merged, Input.TEnd);
	for (int32 Team = 0; Team < 2; ++Team)
	{
		Out.GTeam[Team] = ObjectivesMetricsLocal::LargestGap(Input.Completions[Team], Input.TEnd);
	}
	double EarliestP2 = -1.0;
	for (int32 Team = 0; Team < 2; ++Team)
	{
		if (Input.Prelim2Time[Team] >= 0.0 && (EarliestP2 < 0.0 || Input.Prelim2Time[Team] < EarliestP2))
		{
			EarliestP2 = Input.Prelim2Time[Team];
		}
	}
	Out.FinalDwell = EarliestP2 >= 0.0 ? Input.TEnd - EarliestP2 : -1.0;
	Out.bStallDefect = Input.bStallFlag || Out.GMutual > StallGapDefectSec || (Out.FinalDwell >= 0.0 && Out.FinalDwell > FinalDwellDefectSec);
	return Out;
}

FObjectivesStallInput FObjectivesMetrics::StallInputFromLog(TConstArrayView<FRoundLogEntry> Entries, double ActivePhaseStart, double LastServerTime, bool bStallFlag, uint16 ExcludeCode)
{
	FObjectivesStallInput Out;
	Out.bStallFlag = bStallFlag;
	double End = LastServerTime;
	for (const FRoundLogEntry& Entry : Entries)
	{
		if ((Entry.Flags & RoundLogFlags::Debug) != 0)
		{
			continue;
		}
		const double Time = Entry.ServerTime - ActivePhaseStart;
		switch (Entry.Type)
		{
		case ERoundLogType::ObjectiveStage:
			if (Entry.Data[0] < 2)
			{
				Out.Completions[Entry.Data[0]].Add(Time);
				if (Entry.Data[1] == static_cast<uint32>(EObjectiveStage::Prelim2))
				{
					Out.Prelim2Time[Entry.Data[0]] = Time;
				}
			}
			break;
		case ERoundLogType::FinalCompleted:
			if (Entry.Data[0] < 2)
			{
				Out.Completions[Entry.Data[0]].Add(Time);
			}
			break;
		case ERoundLogType::WinLatched:
			End = Entry.ServerTime;
			break;
		case ERoundLogType::ObjectivePacing:
			Out.bExcludeFromStallMetrics |= Entry.Code == ExcludeCode;
			break;
		default:
			break; // a latched final Complete is a TendCompleted: tend statistics only, never t_end
		}
	}
	Out.TEnd = End - ActivePhaseStart;
	return Out;
}

FObjectivesTendCounts FObjectivesMetrics::CountTends(TConstArrayView<FRoundLogEntry> Entries)
{
	FObjectivesTendCounts Out;
	for (const FRoundLogEntry& Entry : Entries)
	{
		if (Entry.Type != ERoundLogType::TendCompleted)
		{
			continue;
		}
		if (Entry.Code == static_cast<uint16>(ERoundTendResult::Counted))
		{
			++Out.Counted;
		}
		else
		{
			++Out.NoEffect;
			Out.Latched += (Entry.Flags & RoundLogFlags::Latched) != 0 ? 1 : 0;
		}
	}
	return Out;
}

double FObjectivesMetrics::DecoyRatePerNpcPerMinute(double CrowdDecoyTendIntervalSec)
{
	return CrowdDecoyTendIntervalSec > 0.0 ? 60.0 / CrowdDecoyTendIntervalSec : 0.0;
}

double FObjectivesMetrics::FullTendPosterior(double RFullPerMinute, int32 NpcCount, double DecoyRatePerNpcPerMinute, double CompletionShare)
{
	const double Denominator = RFullPerMinute + static_cast<double>(NpcCount) * DecoyRatePerNpcPerMinute * CompletionShare;
	return Denominator > 0.0 ? RFullPerMinute / Denominator : 0.0;
}

double FObjectivesMetrics::DecoyRateForPosterior(double RFullPerMinute, int32 NpcCount, double CompletionShare, double PFull)
{
	const double Denominator = static_cast<double>(NpcCount) * CompletionShare * PFull;
	return Denominator > 0.0 ? RFullPerMinute * (1.0 - PFull) / Denominator : 0.0;
}

bool FObjectivesMetrics::IsInPosteriorBand(double P)
{
	return P >= PosteriorBandLow - 1e-9 && P <= PosteriorBandHigh + 1e-9;
}
