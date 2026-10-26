#pragma once

#include "CoreMinimal.h"

/**
 * The keyboard walk-slower modifier (GDD R15a, MVP item 11; ADR-0011 section 9 amendment; story iif-017): a pure
 * function of the owning client's Move vector, the held modifiers, Run and the device. It scales the KEYBOARD Move
 * vector before ServerMove; it is never a command, adds no RPC or replicated field, and the server sees only an
 * ordinary Move magnitude. Gamepad input is untouched (the stick already covers the pace set).
 *
 * The pace set {0.8, 0.9, 1.0} is an SCL R5.1 constant held by no key (SCL decision D2), so it lives here as named
 * constants for SCL's quantiser to share, not as tuning data.
 *
 * Usage:
 *   const FVector2D Sent = StealthWalkSlower::ScaleMove(RawMove, EMoveInputDevice::Keyboard,
 *       StealthWalkSlower::PaceFromReduction(WalkSlowerActionValue), bRunHeld);
 */
namespace StealthPaceSet
{
	/** Held Left Ctrl (and Ctrl + Alt): the slowest crowd pace. */
	inline constexpr float Slow = 0.8f;
	/** Held Left Alt. */
	inline constexpr float Middle = 0.9f;
	/** No modifier. */
	inline constexpr float Full = 1.0f;
}

/** Which device produced a Move vector. */
enum class EMoveInputDevice : uint8
{
	Keyboard,
	Gamepad,
};

namespace StealthWalkSlower
{
	/**
	 * The pace the held modifiers select: Ctrl 0.8, Alt 0.9, both 0.8 (an explicit minimum, not Enhanced Input's
	 * highest-absolute accumulation of the raw paces), neither 1.0.
	 */
	STEALTHGAMEPLAY_API float PaceFromModifiers(bool bCtrlHeld, bool bAltHeld);

	/**
	 * The pace from IA_WalkSlower's value. The action carries the magnitude REDUCTION (Ctrl 0.2, Alt 0.1; story iif-007)
	 * and keeps the default TakeHighestAbsoluteValue accumulation, so both held give 0.2: pace = 1 - value, i.e. the
	 * same minimum as PaceFromModifiers. Clamped to [0.8, 1.0] so a rebound key cannot leave the pace set.
	 */
	STEALTHGAMEPLAY_API float PaceFromReduction(float Reduction);

	/**
	 * The Move vector the owning client sends. Keyboard: clamped to magnitude 1 (a diagonal W+D is 1.0, not 1.41) and
	 * then scaled by Pace, unless Run is held (Run has no scale). Gamepad: returned unchanged.
	 */
	STEALTHGAMEPLAY_API FVector2D ScaleMove(const FVector2D& Move, EMoveInputDevice Device, float Pace, bool bRunHeld);
}
