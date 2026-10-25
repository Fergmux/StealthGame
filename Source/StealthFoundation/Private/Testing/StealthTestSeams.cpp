#include "Server/Testing/StealthTestSeams.h"

#if WITH_STEALTH_TEST_SEAMS

namespace
{
	// volatile: the stores below must survive optimisation so the literals stay in the binary.
	const TCHAR* volatile GStealthTestSeamSentinelWide = nullptr;
	const ANSICHAR* volatile GStealthTestSeamSentinelAnsi = nullptr;
}

void StealthRegisterTestSeamSentinel()
{
	GStealthTestSeamSentinelWide = TEXT("STEALTH_TEST_SEAM_SENTINEL_7F3A9C2E");
	GStealthTestSeamSentinelAnsi = "STEALTH_TEST_SEAM_SENTINEL_7F3A9C2E";
}

const TCHAR* StealthGetTestSeamSentinel()
{
	return GStealthTestSeamSentinelWide;
}

#endif // WITH_STEALTH_TEST_SEAMS
