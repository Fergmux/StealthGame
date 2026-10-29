#pragma once

#include "CoreMinimal.h"

/**
 * Concealment (Bushes) public vocabulary and pure bush geometry (ADR-0017 sections 1-3, Key Interfaces; GDD
 * concealment-bushes.md CON-R1, CON-R3, CON-R8, F4; stories con-002, con-003, con-005, con-007, con-015). Client-safe:
 * every value here is public geometry built from the Manifest on every machine, or a state name. The server-only record
 * (state, occupancy, commit sequence) is FConcealmentCore's (Server/ConcealmentCore.h) and never crosses the wire.
 *
 * Bush-local frame: bushes are axis-aligned (the footprint is a world-aligned FBox2D), so bush-local yaw is world yaw.
 * Side k (0..3) is the footprint side whose outward normal has yaw k * 90 degrees: 0 = +X, 1 = +Y, 2 = -X, 3 = -Y. Side k
 * covers the yaw sector [k * 90 - 45, k * 90 + 45), lower bound inclusive (story con-005 AC3).
 */

/** The per-pawn concealment state (ADR-0017 section 1; GDD States and Transitions). */
enum class EConcealState : uint8
{
	Outside,
	Entering,
	Concealed,
	Exiting,
	Emerging,
};

/** The phase a TimerFired(ConcealmentTransition) item carries in FTimerPayload::Phase (ADR-0017 Key Interfaces). */
enum class EConcealPhase : uint8
{
	Commit,
	Cross,
	End,
};

/** One bush as every machine builds it from the Manifest (ADR-0017 section 1 FBushRuntime). */
struct FBushRuntime
{
	/** The Level Socket Contract BushId (1-based; 0 is none). */
	uint16 Id = 0;
	/** The planter footprint, 2.0 x 2.0 m, world-aligned (its outer edge is the planter's outer edge). */
	FBox2D Footprint = FBox2D(ForceInit);
	/** The hide volume: the footprint x 2.25 m, from the anchor's ground height. */
	FBox HideVolume = FBox(ForceInit);
	/** Open sides (bit k = side k open). A layout with an empty set is rejected (V-BUSH-1). */
	uint8 EntryAxisMask = 0;
	/** The footprint centre at ground level: where a hider stands. */
	FVector Anchor = FVector::ZeroVector;

	bool IsSideOpen(uint8 Side) const { return Side < 4 && (EntryAxisMask & (1 << Side)) != 0; }
};

namespace ConcealmentGeometry
{
	/** Footprint edge length (ADR-0017 section 1). */
	inline constexpr double FootprintSizeCm = 200.0;
	/** Hide volume height above the anchor's ground. */
	inline constexpr double HideVolumeHeightCm = 225.0;
	/** Clearance beyond the capsule at an exit point (the capsule ends this far outside the footprint edge). */
	inline constexpr double ExitClearanceCm = 5.0;
	/** Every side open. */
	inline constexpr uint8 AllSidesMask = 0x0F;

	/** Wraps a yaw to [0, 360). */
	STEALTHGAMEPLAY_API double WrapYaw360(double YawDeg);

	/** The side whose sector [k * 90 - 45, k * 90 + 45) holds YawDeg (lower bound inclusive). */
	STEALTHGAMEPLAY_API uint8 SideFromYaw(double YawDeg);

	/** Smallest angular distance between two yaws, in [0, 180]. */
	STEALTHGAMEPLAY_API double AngularDistanceDeg(double A, double B);

	/**
	 * The open side nearest YawDeg: angular distance from YawDeg to each open side's centre k * 90; a tie goes to the lower
	 * index. With no open side, the side of the yaw itself (a V-BUSH-1 layout never reaches a round).
	 */
	STEALTHGAMEPLAY_API uint8 NearestOpenSide(uint8 EntryAxisMask, double YawDeg);

	/** The requested side if open, else the open side nearest its centre yaw (CON-R8 side validation). */
	STEALTHGAMEPLAY_API uint8 ValidateSide(uint8 EntryAxisMask, uint8 RequestedSide);

	/** The outward unit normal of side k (horizontal). */
	STEALTHGAMEPLAY_API FVector SideNormal(uint8 Side);

	/** The yaw from the bush anchor to Point (horizontal), degrees. */
	STEALTHGAMEPLAY_API double YawFromAnchor(const FBushRuntime& Bush, const FVector& Point);

	/** Horizontal distance from Point to the footprint (0 inside). */
	STEALTHGAMEPLAY_API double DistanceToFootprintCm(const FBox2D& Footprint, const FVector& Point);

	/**
	 * F4 TargetsBush: d_xy(WorldPoint, Footprint) <= ToleranceCm (the zone is the footprint grown by the tolerance with
	 * rounded corners; the boundary is inclusive, story con-015 decision).
	 */
	STEALTHGAMEPLAY_API bool TargetsBush(const FBushRuntime& Bush, const FVector& WorldPoint, double ToleranceCm);

	/** The exit point of side k: the capsule centre just outside the footprint edge, at the anchor's ground height. */
	STEALTHGAMEPLAY_API FVector ExitPoint(const FBushRuntime& Bush, uint8 Side, double CapsuleRadiusCm);

	/** True when a capsule of CapsuleRadiusCm and HalfHeightCm centred at Location lies wholly inside the hide volume. */
	STEALTHGAMEPLAY_API bool IsCapsuleInsideHideVolume(const FBushRuntime& Bush, const FVector& Location, double CapsuleRadiusCm, double HalfHeightCm);

	/** Builds the runtime of one bush from its Manifest site: centre (ground level) and open sides. */
	STEALTHGAMEPLAY_API FBushRuntime MakeBushRuntime(uint16 Id, const FVector& GroundCentre, uint8 EntryAxisMask);

	/** A stable canonical hash of {Id, HideVolume, EntryAxisMask, Anchor} (BushSiteParity; 0.1 mm quantised). */
	STEALTHGAMEPLAY_API uint64 HashBushRuntime(const FBushRuntime& Bush);
}
