#pragma once

// Story asn-018 (production/epics/assassination-defence-reversal/story-018-strike-clip-variants-warp-and-contact-scheduling.md):
// the strike entry variant from the bearing and the lunge's warp numbers (GDD ASN-R16, AC-ASN-21). Plain C++ with no
// Server include, so the server slot (FStrikeWarpPointCore delegates to it) and every client's presentation compute the
// same variant from the same numbers.

#include "CoreMinimal.h"

/**
 * FStrikeVariantCore (ASN-R16): theta is the bearing of the strike's target relative to the attacker's body facing,
 * degrees, positive to the right. |theta| < FrontHalfAngle (30) -> Strike_F; otherwise Strike_R60 (theta > 0) or
 * Strike_L60, so exactly +-30 takes the side variant. The variant's authored entry turn is 0 / -60 / +60 degrees; the
 * warp turn is the rest of the bearing, capped at StrikeWarpYawLimitDeg (I-ASN-8). The warp translation is the lunge
 * beyond the contact separation (two capsule radii plus the contact gap), capped at StrikeWarpLimitCm by the slot's clamp.
 *
 * Usage:
 *   const uint8 Entry = FStrikeVariantCore::EntryForBearing(Theta);
 *   const double WarpYaw = FStrikeVariantCore::WarpYawDeg(Theta, Entry, Params.StrikeWarpYawLimitDeg);
 */
struct STEALTHGAMEPLAY_API FStrikeVariantCore
{
	/** The entry variant of a bearing: StrikeVariant::Strike_F, Strike_L60 or Strike_R60. */
	static uint8 EntryForBearing(double BearingDeg, double FrontHalfAngleDeg = 30.0);

	/** The entry bits of an FActionState.Variant (the bush preface bit cleared). */
	static uint8 Entry(uint8 VariantBits);

	/** True when FActionState.Variant carries the bush preface. */
	static bool HasBushPreface(uint8 VariantBits);

	/** The variant's authored entry turn, degrees (Strike_F 0, Strike_L60 -60, Strike_R60 +60). */
	static double EntryYawDeg(uint8 VariantBits);

	/** The warp's share of the turn: theta minus the entry turn, clamped to +-YawLimitDeg. */
	static double WarpYawDeg(double BearingDeg, uint8 VariantBits, double YawLimitDeg);

	/** Theta of a horizontal offset from the attacker (the WarpPoint) against its facing yaw, in (-180, 180]. */
	static double BearingOfOffset(const FVector& OffsetCm, double FacingYawDeg);

	/** The lunge's warp translation: the horizontal offset to the target minus the contact separation, never negative. */
	static double WarpTranslationCm(const FVector& OffsetCm, double ContactSeparationCm);

	/** The variant's clip name for logs ("Strike_F", "Strike_L60", "Strike_R60"). */
	static const TCHAR* EntryName(uint8 VariantBits);
};
