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

	static FChannelOpenHook GChannelOpenHook;
	static FPostNetInitHook GPostNetInitHook;
	static FChannelFailureHook GChannelFailureHook;

	void SetChannelOpenHook(FChannelOpenHook InHook)
	{
		check(IsInGameThread());
		GChannelOpenHook = MoveTemp(InHook);
	}

	const FChannelOpenHook& GetChannelOpenHook()
	{
		return GChannelOpenHook;
	}

	void SetPostNetInitHook(FPostNetInitHook InHook)
	{
		check(IsInGameThread());
		GPostNetInitHook = MoveTemp(InHook);
	}

	const FPostNetInitHook& GetPostNetInitHook()
	{
		return GPostNetInitHook;
	}

	void SetChannelFailureHook(FChannelFailureHook InHook)
	{
		check(IsInGameThread());
		GChannelFailureHook = MoveTemp(InHook);
	}

	const FChannelFailureHook& GetChannelFailureHook()
	{
		return GChannelFailureHook;
	}
}

#endif // !UE_BUILD_SHIPPING
