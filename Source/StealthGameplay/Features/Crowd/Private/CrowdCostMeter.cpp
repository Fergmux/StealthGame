// Spike story cnb-032: the crowd's game-thread cost meter (see CrowdCostMeter.h).

#include "Server/CrowdCostMeter.h"

#if !UE_BUILD_SHIPPING

FCrowdCostMeter& FCrowdCostMeter::Get()
{
	static FCrowdCostMeter Meter;
	return Meter;
}

const TCHAR* FCrowdCostMeter::DebugSentinel()
{
	static const TCHAR* volatile Marker = TEXT("STEALTH_TEST_SEAM_SENTINEL_CROWD_DEBUG");
	return Marker;
}

double FCrowdCostMeter::TakeFrame(double OutScopesMs[static_cast<int32>(ECrowdCostScope::Count)])
{
	const double MsPerCycle = FPlatformTime::GetSecondsPerCycle64() * 1000.0;
	for (int32 Index = 0; Index < static_cast<int32>(ECrowdCostScope::Count); ++Index)
	{
		OutScopesMs[Index] = static_cast<double>(Cycles[Index]) * MsPerCycle;
		Cycles[Index] = 0;
	}
	const double Total = static_cast<double>(TopCycles) * MsPerCycle;
	TopCycles = 0;
	return Total;
}

#endif
