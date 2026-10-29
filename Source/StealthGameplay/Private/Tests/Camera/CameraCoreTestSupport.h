#pragma once

// Shared fixture of the rig-core tests (Camera story cs-006): scripted collision (the boom's free length, a blocked
// shoulder) and a body standing at the origin facing +X (view yaw 0) with the eye socket 64 cm above the capsule centre.

#include "CoreMinimal.h"
#include "View/Core/StealthCameraCore.h"
#include "View/Tuning/StealthCameraParams.h"

namespace StealthCameraCoreTest
{
	inline constexpr double Dt = 1.0 / 60.0;
	inline constexpr double EyeAboveCentreCm = 64.0;

	/** Scripted traces for a body facing +X: segments pointing backward are the boom, lateral ones the shoulder probe. */
	class FScriptedTraces final : public IStealthCameraTraces
	{
	public:
		/** The free boom length (cm) both the ray and the sweep see; very large = open. */
		double BoomFreeCm = 1.0e6;
		/** A wall at the shoulder (the lateral probe hits it). */
		bool bShoulderBlocked = false;
		/** Calls of the guard ray (observation). */
		mutable int32 RayCalls = 0;

		virtual double RayFraction(const FVector& Start, const FVector& End, double) const override
		{
			++RayCalls;
			return Fraction(Start, End);
		}
		virtual double SweepFraction(const FVector& Start, const FVector& End, double) const override
		{
			const FVector D = End - Start;
			if (FMath::Abs(D.Y) > FMath::Abs(D.X))
			{
				return bShoulderBlocked ? 0.5 : 1.0;
			}
			return Fraction(Start, End);
		}

	private:
		double Fraction(const FVector& Start, const FVector& End) const
		{
			const double Length = FVector::Dist(Start, End);
			return Length > 0.0 ? FMath::Min(1.0, BoomFreeCm / Length) : 1.0;
		}
	};

	/** A body at Centre (capsule half-height 92) with the eye socket at Centre.Z + 64. */
	inline FStealthCameraBodyInput Body(const FVector& Centre = FVector(0.0, 0.0, 92.0))
	{
		FStealthCameraBodyInput In;
		In.CapsuleCentre = Centre;
		In.CapsuleHalfHeightCm = 92.0;
		In.EyeSocketZ = Centre.Z + EyeAboveCentreCm;
		In.DtSec = Dt;
		return In;
	}

	inline FStealthCameraParams Params() { return FStealthCameraParams::Defaults(); }
}
