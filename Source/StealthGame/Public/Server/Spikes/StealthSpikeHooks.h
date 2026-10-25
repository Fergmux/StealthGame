#pragma once

#include "StealthServerOnlyGuard.h"
#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Templates/Function.h"

class AActor;

/**
 * THROWAWAY SPIKE HOOKS (IVP spike S9, story production/epics/information-visibility-policy/
 * story-004-spike-s9-cull-latency-and-explicit-channel-close.md, 2026-10-25). Not production code,
 * never promoted: the production cull lives in AStealthPawn (ADR-0003 section 5).
 *
 * Lets the multi-process harness scenario (StealthTestHarness, T3) replace the prototype pawn's
 * relevancy decision without the prototype module depending on the harness. Compiled out of Shipping.
 */
#if !UE_BUILD_SHIPPING
namespace StealthSpikeHooks
{
	/**
	 * Replaces AStealthCharacter::IsNetRelevantFor when set. Returns the relevancy decision for
	 * (Subject, RealViewer, ViewTarget), or an unset optional to fall through to APawn's decision.
	 */
	using FRelevancyOverride = TFunction<TOptional<bool>(const AActor* Subject, const AActor* RealViewer, const AActor* ViewTarget)>;

	/** Installs (or, with an empty function, removes) the relevancy override. Game thread only. */
	STEALTHGAME_API void SetRelevancyOverride(FRelevancyOverride InOverride);

	/** The installed override; empty when none is installed. */
	STEALTHGAME_API const FRelevancyOverride& GetRelevancyOverride();
}
#endif // !UE_BUILD_SHIPPING
