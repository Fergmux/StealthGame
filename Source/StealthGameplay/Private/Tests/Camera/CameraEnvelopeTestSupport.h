#pragma once

// Shared fixture of the view-envelope tests (Camera story cs-002): the schema-default parameters and a fixed-dt driver
// (1/60 s, GDD AC preamble).

#include "CoreMinimal.h"
#include "View/Core/StealthViewEnvelope.h"
#include "View/Tuning/StealthCameraParams.h"

namespace StealthCameraEnvelopeTest
{
	inline constexpr double Dt = 1.0 / 60.0;

	inline FViewEnvelopeParams DefaultParams() { return FViewEnvelopeParams::FromCamera(FStealthCameraParams::Defaults()); }

	/** One frame of a constant rate (degrees per second) on yaw and pitch. */
	inline void Frame(FViewEnvelopeState& State, const FViewEnvelopeParams& Params, double YawRate, double PitchRate = 0.0, bool bConcealed = false)
	{
		FViewEnvelopeInput In;
		In.DeltaYawDeg = YawRate * Dt;
		In.DeltaPitchDeg = PitchRate * Dt;
		In.DtSec = Dt;
		In.bConcealed = bConcealed;
		StealthViewEnvelope::Step(State, In, Params);
	}

	/** Frames covering Seconds. */
	inline int32 FramesOf(double Seconds) { return FMath::RoundToInt(Seconds / Dt); }
}
