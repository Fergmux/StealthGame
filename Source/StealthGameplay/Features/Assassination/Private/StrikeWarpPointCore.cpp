// Story asn-012 (production/epics/assassination-defence-reversal/story-012-slot-write-outcome-blind-and-warp-point.md):
// the strike WarpPoint and Variant, ADR-0018 section 2.2. See Server/StrikeWarpPointCore.h.

#include "Server/StrikeWarpPointCore.h"

#include "Server/EncounterTypes.h"
#include "StrikeVariantCore.h"

FStrikeWarpPointParams FStrikeWarpPointParams::FromEncounter(const FEncounterParams& Params)
{
	FStrikeWarpPointParams Out;
	Out.CapsuleRadiusCm = Params.CapsuleRadiusCm;
	Out.StrikeContactGapCm = Params.Public.StrikeContactGapCm;
	Out.StrikeWarpLimitCm = Params.Public.StrikeWarpLimitCm;
	Out.StrikeVerticalReachCm = Params.Server.StrikeVerticalReachCm;
	return Out;
}

uint8 FStrikeWarpPointCore::EntryVariantForBearing(double BearingDeg, double FrontHalfAngleDeg)
{
	// One rule for the slot and every client's presentation (story asn-018).
	return FStrikeVariantCore::EntryForBearing(BearingDeg, FrontHalfAngleDeg);
}

FVector2D FStrikeWarpPointCore::ClampToAttackerSideEdge(const FBox2D& Box, const FVector2D& From, const FVector2D& To)
{
	const FVector2D Inside(FMath::Clamp(To.X, Box.Min.X, Box.Max.X), FMath::Clamp(To.Y, Box.Min.Y, Box.Max.Y));
	if (Box.IsInside(From))
	{
		return Inside; // striking from within the footprint: no attacker-side edge to clamp to
	}
	// Slab entry of the segment From -> Inside (Inside is in the box and From is not, so the entry lies in [0, 1]).
	const FVector2D Delta = Inside - From;
	double Enter = 0.0;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const double D = Delta[Axis];
		const double O = From[Axis];
		if (FMath::Abs(D) < UE_DOUBLE_SMALL_NUMBER)
		{
			continue; // parallel to this slab and, since the end point is inside, within it
		}
		const double T1 = (Box.Min[Axis] - O) / D;
		const double T2 = (Box.Max[Axis] - O) / D;
		Enter = FMath::Max(Enter, FMath::Min(T1, T2));
	}
	Enter = FMath::Clamp(Enter, 0.0, 1.0);
	return From + Delta * Enter;
}

FStrikeWarpPointResult FStrikeWarpPointCore::Evaluate(const FStrikeWarpPointParams& Params, const FStrikeWarpPointInput& Input)
{
	FStrikeWarpPointResult Out;
	FVector Target = Input.Source == EStrikeWarpSource::PawnRef ? Input.RefVictimAtContact : Input.WorldPoint;
	if (Input.Source == EStrikeWarpSource::BushSpot && Input.BushFootprint.bIsValid)
	{
		const FVector2D Edge = ClampToAttackerSideEdge(Input.BushFootprint, FVector2D(Input.AttackerLocation), FVector2D(Target));
		Target.X = Edge.X;
		Target.Y = Edge.Y;
	}

	// The clamp (every case): the horizontal radius around P_A, then the vertical reach. The bearing is not clamped here.
	FVector Offset = Target - Input.AttackerLocation;
	const FVector2D Horizontal(Offset.X, Offset.Y);
	const double Limit = Params.HorizontalLimitCm();
	if (Limit >= 0.0 && Horizontal.SizeSquared() > Limit * Limit)
	{
		const FVector2D Clamped = Horizontal.GetSafeNormal() * Limit;
		Offset.X = Clamped.X;
		Offset.Y = Clamped.Y;
	}
	Offset.Z = FMath::Clamp(Offset.Z, -Params.StrikeVerticalReachCm, Params.StrikeVerticalReachCm);

	Out.TargetLocation = Input.AttackerLocation + Offset;
	Out.WarpPoint = FFixedWarpOffset::FromOffsetCm(Offset);
	const double TargetYawDeg = FVector2D(Offset.X, Offset.Y).IsNearlyZero() ? Input.AttackerFacingYawDeg : FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
	Out.BearingDeg = FRotator::NormalizeAxis(TargetYawDeg - Input.AttackerFacingYawDeg);
	Out.Variant = EntryVariantForBearing(Out.BearingDeg, Params.FrontVariantHalfAngleDeg);
	if (Input.bBushPreface)
	{
		Out.Variant |= StrikeVariant::BushPreface;
	}
	return Out;
}
