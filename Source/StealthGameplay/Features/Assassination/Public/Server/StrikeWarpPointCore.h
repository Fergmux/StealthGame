#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "View/StealthEncounterWire.h"

struct FEncounterParams;

/** Where a strike's warp target comes from (ADR-0018 section 2.2): never a culled or concealed pawn's position. */
enum class EStrikeWarpSource : uint8
{
	/** An honoured PawnRef (hit or reach-failed whiff): F1's extrapolated victim capsule centre P_V(T_v). */
	PawnRef,
	/** An open TargetSpot (hit or whiff): the submitted WorldPoint. */
	OpenSpot,
	/** A bush spot (hit or whiff): the WorldPoint clamped to the bush footprint edge on the attacker's side. */
	BushSpot,
};

/** The clamp of every warp target (ADR-0018 section 2.2), from the frozen tuning. */
struct STEALTHGAMEPLAY_API FStrikeWarpPointParams
{
	double CapsuleRadiusCm = 35.0;
	double StrikeContactGapCm = 0.0;
	double StrikeWarpLimitCm = 0.0;
	double StrikeVerticalReachCm = 0.0;
	/** |theta| below this takes Strike_F; at or above, the side variant (ASN-R16, 30 degrees). */
	double FrontVariantHalfAngleDeg = 30.0;

	/** The clamp of Params: horizontal 2 r_c + StrikeContactGapCm + StrikeWarpLimitCm, vertical StrikeVerticalReachCm. */
	static FStrikeWarpPointParams FromEncounter(const FEncounterParams& Params);

	/** The horizontal clamp radius. */
	double HorizontalLimitCm() const { return 2.0 * CapsuleRadiusCm + StrikeContactGapCm + StrikeWarpLimitCm; }
};

/** One strike's inputs: only the attacker's server capsule, the request and public geometry. */
struct FStrikeWarpPointInput
{
	/** P_A: the attacker's server capsule location at the slot. */
	FVector AttackerLocation = FVector::ZeroVector;
	/** Body facing (actor yaw), degrees. */
	double AttackerFacingYawDeg = 0.0;
	EStrikeWarpSource Source = EStrikeWarpSource::OpenSpot;
	/** The submitted WorldPoint (OpenSpot, BushSpot). */
	FVector WorldPoint = FVector::ZeroVector;
	/** PawnRef: F1's P_V(T_v) of the honoured reference (knowable to the attacker by definition). */
	FVector RefVictimAtContact = FVector::ZeroVector;
	/** BushSpot: the bush footprint (ADR-0017 FBushRuntime.Footprint, public geometry); an invalid box clamps nothing. */
	FBox2D BushFootprint = FBox2D(ForceInit);
	/** The attacker struck from a bush (FActionState.Variant BushPreface bit). */
	bool bBushPreface = false;
};

/** The derived target, the wire offset and the variant. */
struct FStrikeWarpPointResult
{
	/** The clamped world target (P_A + the unquantised offset). */
	FVector TargetLocation = FVector::ZeroVector;
	/** The quantised offset from P_A (0.1 cm per axis, half away from zero). */
	FFixedWarpOffset WarpPoint;
	/** StrikeVariant: the entry variant from theta, plus BushPreface. */
	uint8 Variant = StrikeVariant::Strike_F;
	/** Theta: bearing of the target relative to body facing, degrees in (-180, 180]; positive to the right. */
	double BearingDeg = 0.0;
};

/**
 * FStrikeWarpPointCore: the strike's WarpPoint and Variant (ADR-0018 sections 2.1 step 2 and 2.2; GDD ASN-R16, ASN-R21,
 * AC-ASN-47; story asn-012). Pure: the same inputs give the same bits, and no input can carry a hidden pawn's position
 * (the bush case reads only the WorldPoint and the footprint; the open spot only the WorldPoint), so the value is
 * identical for a kill, a whiff and a reversal at the same request.
 *
 * Variant (ASN-R16): theta is measured to the clamped warp target (the honoured victim's extrapolation, else the
 * WorldPoint); |theta| < 30 degrees -> Strike_F, otherwise Strike_R60 (theta > 0, to the right) or Strike_L60, with
 * exactly +-30 taking the side variant. The bearing itself is not clamped here (the clip caps the warp yaw).
 *
 * Usage (the strike slot, FAttackResolverCore):
 *   FStrikeWarpPointInput In; In.AttackerLocation = P_A; In.AttackerFacingYawDeg = Yaw; In.Source = EStrikeWarpSource::OpenSpot;
 *   In.WorldPoint = Cmd.WorldPoint;
 *   const FStrikeWarpPointResult Warp = FStrikeWarpPointCore::Evaluate(FStrikeWarpPointParams::FromEncounter(Params), In);
 */
class STEALTHGAMEPLAY_API FStrikeWarpPointCore
{
public:
	static FStrikeWarpPointResult Evaluate(const FStrikeWarpPointParams& Params, const FStrikeWarpPointInput& Input);

	/** The entry variant of a bearing (Strike_F, Strike_L60 or Strike_R60). */
	static uint8 EntryVariantForBearing(double BearingDeg, double FrontHalfAngleDeg = 30.0);

	/** The point where the segment From -> To enters Box, To first clamped into Box (From inside the box: To clamped). */
	static FVector2D ClampToAttackerSideEdge(const FBox2D& Box, const FVector2D& From, const FVector2D& To);
};
