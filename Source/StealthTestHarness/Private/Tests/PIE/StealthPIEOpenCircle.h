#pragma once

// Layout-independent walking for PIE locomotion tests (the 2026-10-26 WalkSlowerModifier fix, shared 2026-10-27 with
// LocomotionPie.ReleaseBaselineStopUnderLag and Assassination.PIE.KillAtArmsLength). The round seed and the spawn slot come from the session's OS entropy, so a
// test that walks open-loop from its spawn crosses a different city every run and sometimes walks into a wall, a prop or
// the other player's pawn. These helpers find a clear, flat circle near a point (on the server world: the layout is the
// same on every PIE world because RoundSeed replicates) and steer a walker closed-loop round it, so neither the layout,
// the spawn slot nor the frame time reaches a measurement.

#include "CoreMinimal.h"
#include "Templates/Function.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "WorldCollision.h"

namespace StealthPIEOpenCircle
{
	/** Search and shape parameters of FindOpenCircle (defaults: the WalkSlowerModifier values). */
	struct FParams
	{
		/** Orbit radius (cm). */
		double RadiusCm = 300.0;
		/** Clearance beyond the circle: capsule radius plus the overshoot of a pace change (cm). */
		double MarginCm = 120.0;
		/** Rings of candidate centres round the origin, this far apart (cm), out to SearchRadiusCm. */
		double SearchStepCm = 200.0;
		double SearchRadiusCm = 6000.0;
		/** The clear box starts this far above the floor (kerb-free ground) and is this tall (a standing pawn). */
		double BoxFloorGapCm = 5.0;
		double BoxHeightCm = 190.0;
		/** Rim floor traces must land within this of the centre's floor (a flat disc, no ledge or step). */
		double FloorFlatnessCm = 4.0;
		int32 RimTraces = 12;
		/** Further actors the clear box ignores (the pawns about to be placed there). */
		TArray<const AActor*> AlsoIgnored;
		/** Optional extra test of a candidate centre's floor point (e.g. far from every bush); unset accepts any. */
		TFunction<bool(const FVector&)> Accept;
	};

	/** Floor height under (X, Y) between Z + 150 and Z - 400, or unset (no floor: water, a hole or outside the map). */
	inline TOptional<double> FloorAt(UWorld* World, double X, double Y, double Z)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(StealthPIEOpenCircleFloor), false);
		if (World->LineTraceSingleByChannel(Hit, FVector(X, Y, Z + 150.0), FVector(X, Y, Z - 400.0), ECC_Visibility, Params) && Hit.ImpactNormal.Z > 0.99)
		{
			return Hit.ImpactPoint.Z;
		}
		return TOptional<double>();
	}

	/**
	 * A centre near Origin (a floor point) whose whole circle (radius + margin) is flat floor with nothing standing on it (no
	 * wall, prop or pawn other than Ignore): a box overlap over the disc and floor traces round its rim. Rings outwards, so
	 * the nearest clear centre wins. Returns the centre's floor point, or unset when none is clear within the search radius.
	 */
	inline TOptional<FVector> FindOpenCircle(UWorld* World, const FVector& Origin, const AActor* Ignore, const FParams& P = FParams())
	{
		const double Extent = P.RadiusCm + P.MarginCm;
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(StealthPIEOpenCircle), false, Ignore);
		for (const AActor* Also : P.AlsoIgnored)
		{
			Params.AddIgnoredActor(Also);
		}
		const FCollisionShape Box = FCollisionShape::MakeBox(FVector(Extent, Extent, P.BoxHeightCm * 0.5));
		for (double Ring = 0.0; Ring <= P.SearchRadiusCm; Ring += P.SearchStepCm)
		{
			const int32 Count = Ring > 0.0 ? FMath::Max(6, FMath::CeilToInt32(2.0 * UE_DOUBLE_PI * Ring / P.SearchStepCm)) : 1;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const double Angle = 2.0 * UE_DOUBLE_PI * Index / Count;
				const double X = Origin.X + Ring * FMath::Cos(Angle);
				const double Y = Origin.Y + Ring * FMath::Sin(Angle);
				const TOptional<double> Floor = FloorAt(World, X, Y, Origin.Z);
				if (!Floor.IsSet())
				{
					continue;
				}
				bool bFlat = true;
				for (int32 Rim = 0; Rim < P.RimTraces && bFlat; ++Rim)
				{
					const double RimAngle = 2.0 * UE_DOUBLE_PI * Rim / P.RimTraces;
					const TOptional<double> RimFloor = FloorAt(World, X + Extent * FMath::Cos(RimAngle), Y + Extent * FMath::Sin(RimAngle), Floor.GetValue());
					bFlat = RimFloor.IsSet() && FMath::Abs(RimFloor.GetValue() - Floor.GetValue()) <= P.FloorFlatnessCm;
				}
				if (!bFlat)
				{
					continue;
				}
				if (P.Accept && !P.Accept(FVector(X, Y, Floor.GetValue())))
				{
					continue;
				}
				const FVector BoxCentre(X, Y, Floor.GetValue() + P.BoxFloorGapCm + P.BoxHeightCm * 0.5);
				if (!World->OverlapAnyTestByObjectType(BoxCentre, FQuat::Identity, Objects, Box, Params))
				{
					return FVector(X, Y, Floor.GetValue());
				}
			}
		}
		return TOptional<FVector>();
	}

	/** Moves CurrentYaw toward TargetYaw by at most MaxStepDeg (a mouse-like turn inside the gait's turn rate). */
	inline double SteerToward(double CurrentYaw, double TargetYaw, double MaxStepDeg)
	{
		const double Delta = FRotator::NormalizeAxis(TargetYaw - CurrentYaw);
		return CurrentYaw + FMath::Clamp(Delta, -MaxStepDeg, MaxStepDeg);
	}

	/**
	 * The client timestamp (s) of the last ServerMove the server applied to ServerPawn (the owner's move clock), or -1 before
	 * the first one. A sample taken once per new value is taken once per processed move, so server frame timing never reaches
	 * it (the 2026-10-27 ReleaseBaselineStopUnderLag fix, 8015f9f).
	 */
	inline double ServerMoveTime(const ACharacter* ServerPawn)
	{
		UCharacterMovementComponent* Movement = ServerPawn ? ServerPawn->GetCharacterMovement() : nullptr;
		const FNetworkPredictionData_Server_Character* Data = Movement ? Movement->GetPredictionData_Server_Character() : nullptr;
		return Data ? static_cast<double>(Data->CurrentClientTimeStamp) : -1.0;
	}

	/** The owner's move clock (s): the timestamp of the last move it created, the clock ServerMoveTime reads on the server. */
	inline double OwnerMoveTime(const ACharacter* OwnerPawn)
	{
		UCharacterMovementComponent* Movement = OwnerPawn ? OwnerPawn->GetCharacterMovement() : nullptr;
		const FNetworkPredictionData_Client_Character* Data = Movement ? Movement->GetPredictionData_Client_Character() : nullptr;
		return Data ? static_cast<double>(Data->CurrentTimeStamp) : -1.0;
	}

	/**
	 * Closed-loop anticlockwise orbit: the heading (yaw, degrees) of the circle's tangent at Location, bent back toward the
	 * circle by the radial error (straight back at CorrectionCm of error).
	 */
	inline double OrbitYawDeg(const FVector& Location, const FVector& Centre, double RadiusCm, double CorrectionCm)
	{
		FVector Radial = Location - Centre;
		Radial.Z = 0.0;
		const double Distance = Radial.Size();
		if (Distance < 1.0)
		{
			return 0.0;
		}
		Radial /= Distance;
		const FVector Tangent(-Radial.Y, Radial.X, 0.0);
		const FVector Heading = Tangent + Radial * FMath::Clamp((RadiusCm - Distance) / CorrectionCm, -1.0, 1.0);
		return FMath::RadiansToDegrees(FMath::Atan2(Heading.Y, Heading.X));
	}
}

#endif
