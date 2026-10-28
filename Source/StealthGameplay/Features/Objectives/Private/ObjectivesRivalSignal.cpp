// Objectives Chain story oc-014: the rival signal's draw and grid. See ObjectivesRivalSignal.h.

#include "ObjectivesRivalSignal.h"

#include "Server/Random/StealthSecretStream.h"
#include "View/Random/StealthRangedDraw.h"

int64 FObjectivesRivalSignal::ToMs(double Seconds)
{
	return static_cast<int64>(FMath::RoundHalfFromZero(Seconds * 1000.0));
}

int32 FObjectivesRivalSignal::DrawRivalDelayMsFromSubSeed(uint64 SubSeed, int32 MinMs, int32 MaxMs)
{
	if (SubSeed == 0 || MinMs < 0 || MinMs > MaxMs)
	{
		return -1;
	}
	FSecretStream Stream(SubSeed, StreamTag);
	return StealthRandom::UniformInRange(Stream, MinMs, MaxMs);
}

int32 FObjectivesRivalSignal::DrawRivalDelayMs(uint32 Ordinal, const IObjectivesSecretDraw& Draw, int32 MinMs, int32 MaxMs)
{
	return DrawRivalDelayMsFromSubSeed(Draw.GetStageSignalSubSeed(Ordinal), MinMs, MaxMs);
}

int64 FObjectivesRivalSignal::ComputeReleaseMs(int64 T0Ms, int64 TcMs, int64 DeltaMs, int64 QMs)
{
	const int64 Numerator = TcMs + DeltaMs - T0Ms;
	if (QMs <= 0 || TcMs < T0Ms || DeltaMs <= 0 || Numerator <= 0)
	{
		return -1;
	}
	const int64 Ticks = (Numerator + QMs - 1) / QMs; // integer ceiling of a positive numerator
	return T0Ms + QMs * Ticks;
}
