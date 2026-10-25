#pragma once

/**
 * THROWAWAY SPIKE CODE (S1 / S2 / S14 for ADR-0001, 2026-10-25). Not production code, not promoted.
 * Results: production/qa/spikes/2026-10-25_S1-S2-S14.md. Compiled out of Shipping.
 *
 * Dev switch: console variable `stealth.Spike.IdentityTrims` (bitmask, default 0 = prototype behaviour).
 * It is read when a net driver builds the replication layout of a class, i.e. once per play session:
 * set it BEFORE starting PIE (or on the command line with -ini/-ExecCmds), never mid-session.
 *   1  ADR-0001 §4.1: APawn::PlayerState, APawn::Controller, AActor::Owner -> COND_OwnerOnly on AStealthCharacter
 *   2  ADR-0001 §4.3: APlayerState engine-default trims (table) + PlayerState Owner -> COND_OwnerOnly, ping updates off
 *   4  ADR-0001 §4.3 strict allowlist: prototype AStealthPlayerState::Team / LifeState -> COND_OwnerOnly
 *   8  ADR-0001 §8 fallback: PlayerState / Controller / Owner DISABLED on AStealthCharacter (overrides 1)
 */

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

class FLifetimeProperty;

namespace StealthSpike
{
	enum EIdentityTrim : int32
	{
		PawnLinkOwnerOnly = 1,
		PlayerStateEngineTrims = 2,
		PlayerStatePrototypeFieldsOwnerOnly = 4,
		PawnLinkDisabled = 8,
	};

	/** Current value of stealth.Spike.IdentityTrims. */
	int32 GetIdentityTrims();

	/** Hook for AStealthCharacter::GetLifetimeReplicatedProps (after Super and the class's own DOREPLIFETIMEs). */
	void ApplyPawnTrims(const UClass* ThisClass, TArray<FLifetimeProperty>& OutLifetimeProps);

	/** Hook for AStealthPlayerState::GetLifetimeReplicatedProps (after Super and the class's own DOREPLIFETIMEs). */
	void ApplyPlayerStateTrims(const UClass* ThisClass, TArray<FLifetimeProperty>& OutLifetimeProps);
}

#endif // !UE_BUILD_SHIPPING
