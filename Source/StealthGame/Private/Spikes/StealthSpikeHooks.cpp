// THROWAWAY SPIKE CODE (IVP S9, 2026-10-25). See Server/Spikes/StealthSpikeHooks.h.
#include "Server/Spikes/StealthSpikeHooks.h"

#if !UE_BUILD_SHIPPING

namespace StealthSpikeHooks
{
	static FRelevancyOverride GRelevancyOverride;

	void SetRelevancyOverride(FRelevancyOverride InOverride)
	{
		check(IsInGameThread());
		GRelevancyOverride = MoveTemp(InOverride);
	}

	const FRelevancyOverride& GetRelevancyOverride()
	{
		return GRelevancyOverride;
	}
}

#endif // !UE_BUILD_SHIPPING
