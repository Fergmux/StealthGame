#include "Server/CrowdStallRegulator.h"

#include "Server/StealthTuningSubstructs.h"

double FCrowdStallRegulator::Update(uint32 Ordinal, double Speed2D, bool bConcealed, double Now)
{
	FStretch& Stretch = Stretches.FindOrAdd(Ordinal);
	const double Dt = FMath::Max(Now - Stretch.LastUpdate, 0.0);
	Stretch.LastUpdate = Now;
	if (bConcealed)
	{
		// Suspended: an unseen pawn is not a visible stand (CNB-R10.1, AC-CNB-13); the stretch neither grows nor ends.
		if (Stretch.Start >= 0.0)
		{
			Stretch.Start += Dt;
		}
		return Stretch.Start >= 0.0 ? Now - Stretch.Start : 0.0;
	}
	if (Speed2D <= StationarySpeedCm)
	{
		if (Stretch.Start < 0.0)
		{
			Stretch.Start = Now;
		}
		return Now - Stretch.Start;
	}
	Stretch.Start = -1.0;
	return 0.0;
}

double FCrowdStallRegulator::GetAge(uint32 Ordinal, double Now) const
{
	const FStretch* Stretch = Stretches.Find(Ordinal);
	return Stretch != nullptr && Stretch->Start >= 0.0 ? Now - Stretch->Start : 0.0;
}

int32 FCrowdStallRegulator::CountCovered(double Now, double ThresholdSec) const
{
	int32 Count = 0;
	for (const TPair<uint32, FStretch>& Pair : Stretches)
	{
		Count += Pair.Value.Start >= 0.0 && Now - Pair.Value.Start >= ThresholdSec ? 1 : 0;
	}
	return Count;
}

double FCrowdStallRegulator::EffectiveLongPausePct(double Now, int32 LiveCount, const FCrowdTuning_Server& Tuning, double StallThresholdSec, int32 ProjectedCovered) const
{
	const int32 Floor = FMath::CeilToInt32(Tuning.CrowdStallCoverMinPct / 100.0 * FMath::Max(LiveCount, 0));
	const double Base = Tuning.CrowdLongPausePct;
	// The trigger leads the floor (CoverMarginNpcs) and looks P_th ahead (ProjectedCovered): a long pause takes P_th to
	// count, so a regulator that waits for the floor itself recovers too late to keep 98 % of samples above it
	// (AC-CNB-12; decision logged 2026-10-26).
	const int32 Trigger = Floor + (Floor > 0 ? CoverMarginNpcs : 0);
	if (Floor > 0 && (CountCovered(Now, StallThresholdSec) < Trigger || (ProjectedCovered >= 0 && ProjectedCovered < Trigger)))
	{
		return FMath::Min(2.0 * Base, static_cast<double>(Tuning.CrowdLongPauseMaxPct));
	}
	return Base;
}
