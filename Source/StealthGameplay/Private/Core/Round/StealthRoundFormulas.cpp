#include "Server/Core/Round/StealthRoundFormulas.h"

#include "Server/Random/StealthSecretStream.h"
#include "View/Random/StealthRangedDraw.h"

namespace StealthRoundFormulas
{
	double EffectiveGraceSec(double DisconnectGraceSec, double RejoinWindowSec)
	{
		return FMath::Min(DisconnectGraceSec, RejoinWindowSec - GraceMarginSec);
	}

	double GraceExpiry(double DisconnectedAt, double ActivePhaseStart, double EffectiveGrace)
	{
		return FMath::Max(DisconnectedAt, ActivePhaseStart) + EffectiveGrace;
	}

	void ComputeSpawnOrder(int32 D, uint64 SpawnOrderSubSeed, TArray<int32>& OutSigma)
	{
		OutSigma.Reset(FMath::Max(D, 0));
		for (int32 Slot = 0; Slot < D; ++Slot)
		{
			OutSigma.Add(Slot);
		}
		// ADR-0006 Guideline 5: the input is the canonical sorted slot list; the result is a pure function of the stream.
		FSecretStream Stream(SpawnOrderSubSeed, SpawnOrderStreamTag);
		StealthRandom::FisherYates(Stream, MakeArrayView(OutSigma));
	}

	TOptional<double> ZoneShare(const FRoundPlacementZone& Zone)
	{
		if (Zone.Players <= 0)
		{
			return TOptional<double>();
		}
		return static_cast<double>(Zone.Players) / static_cast<double>(Zone.Players + FMath::Max(Zone.Npcs, 0));
	}

	bool PlacementSharesHold(TConstArrayView<FRoundPlacementZone> Zones)
	{
		for (const FRoundPlacementZone& Zone : Zones)
		{
			const TOptional<double> Share = ZoneShare(Zone);
			if (Share.IsSet() && Share.GetValue() > MaxZoneShare)
			{
				return false;
			}
		}
		return true;
	}

	double GlobalShare(int32 Participants, int32 NpcCount)
	{
		const int32 Total = Participants + NpcCount;
		return Total > 0 ? static_cast<double>(Participants) / static_cast<double>(Total) : 0.0;
	}

	FWatchdogBound ComputePreparingWatchdog(const FRoundPreparingTimings& T)
	{
		FWatchdogBound Out;
		Out.PrepMaxSec = FMath::Max(T.TravelLoadTimeoutSec, T.ServerLoadBoundSec + T.LayoutTotalBudgetSec + T.TuningFreezeBoundSec + T.TravelAckGraceSec);
		Out.NeededSec = Out.PrepMaxSec + T.SpawnBoundSec + T.WorldReadyTimeoutSec + T.StartLeadSec + WatchdogMarginSec;
		Out.bNeededWins = Out.NeededSec > T.PreparingWatchdogSec;
		Out.EffectiveSec = FMath::Max(T.PreparingWatchdogSec, Out.NeededSec);
		return Out;
	}
}
