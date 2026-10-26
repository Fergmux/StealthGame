#pragma once

#include "StealthServerOnlyGuard.h"
#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Templates/Function.h"

class AActor;
class APlayerController;
class UActorChannel;
class UNetConnection;

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

	// ---- IVP S10 re-run (story-056, ADR-0003 amendment "S10 resolution 2026-10-25" S10-R4/R5) ----------------

	/**
	 * Client: called from AStealthCharacter::OnActorChannelOpen after Super (E18: after every successful
	 * SerializeNewActor, including a re-bind of a live actor) with the opening channel index and the packet id the
	 * open bunch arrived in.
	 */
	using FChannelOpenHook = TFunction<void(AActor* Pawn, int32 ChIndex, int32 PacketId, UNetConnection* Connection)>;
	STEALTHGAME_API void SetChannelOpenHook(FChannelOpenHook InHook);
	STEALTHGAME_API const FChannelOpenHook& GetChannelOpenHook();

	/** Client: called from AStealthCharacter::PostNetInit after Super (a newly spawned network actor, E11). */
	using FPostNetInitHook = TFunction<void(AActor* Pawn)>;
	STEALTHGAME_API void SetPostNetInitHook(FPostNetInitHook InHook);
	STEALTHGAME_API const FPostNetInitHook& GetPostNetInitHook();

	/** Server: called from AStealthPlayerController::NotifyActorChannelFailure (E18; runs inside UControlChannel::ReceivedBunch: queue only). */
	using FChannelFailureHook = TFunction<void(APlayerController* Controller, UActorChannel* Channel)>;
	STEALTHGAME_API void SetChannelFailureHook(FChannelFailureHook InHook);
	STEALTHGAME_API const FChannelFailureHook& GetChannelFailureHook();
}
#endif // !UE_BUILD_SHIPPING
