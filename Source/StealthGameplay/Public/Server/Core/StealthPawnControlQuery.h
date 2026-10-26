#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

class AActor;

/**
 * The pawn factory's two server-only per-pawn facts that the crowd reads, without the factory header (which names the
 * Roster call types): the control epoch (ADR-0010 section 4.4) and the spawn ordinal (Crowd CNB-R14). Implemented by
 * UStealthPawnFactory (story cnb-005); crowd code includes only this header, so no Roster header reaches it (AC-CNB-01).
 */
namespace StealthPawnControl
{
	/** No spawn ordinal (a pawn the factory did not spawn). */
	inline constexpr uint32 NoSpawnOrdinal = MAX_uint32;

	/** The pawn's current control epoch (0 for a pawn the factory never possessed or an unknown world). */
	STEALTHGAMEPLAY_API uint32 GetControlEpoch(const AActor* Pawn);

	/** The pawn's spawn ordinal, or NoSpawnOrdinal. */
	STEALTHGAMEPLAY_API uint32 GetSpawnOrdinal(const AActor* Pawn);
}
