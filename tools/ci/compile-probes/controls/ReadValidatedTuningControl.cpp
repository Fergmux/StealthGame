// Positive control of tools/ci/compile-probes/MutateValidatedTuning.cpp (story tdc-054). Reads the member the
// probe writes, through the same holders; must compile in StealthFoundation. Lives in controls/ so the runner,
// which globs only the top-level *.cpp of tools/ci/compile-probes/, never treats it as a probe.

#include "Server/StealthValidatedTuning.h"

int32 StealthProbeReadValidatedTuning(const FValidatedTuning& Tuning, const TSharedRef<const FValidatedTuning>& Held)
{
	return Tuning.Values().Round_Public.TeamSize + Held->Values().Round_Public.TeamSize;
}
