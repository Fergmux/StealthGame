// Story cs-003: the look response (GDD camera-system.md F6) and the look pipeline step. See View/Core/StealthLookResponse.h.

#include "View/Core/StealthLookResponse.h"

#include "View/Tuning/StealthCameraParams.h"

FVector2D StealthLookResponse::StickRates(const FVector2D& Stick, const FStealthCameraUserSettings& Settings, double OuterZoneFrac, const FLookSensitivity& Sensitivity)
{
	const double Magnitude = Stick.Size();
	const double DeadZone = Settings.StickDeadZoneFrac;
	if (Magnitude <= DeadZone || OuterZoneFrac <= DeadZone)
	{
		return FVector2D::ZeroVector;
	}
	// Scaled radial dead zone: computed on the magnitude, applied along the direction (diagonals match every implementation).
	const double M = FMath::Clamp((Magnitude - DeadZone) / (OuterZoneFrac - DeadZone), 0.0, 1.0);
	const double Curve = FMath::Pow(M, Settings.StickCurveExp);
	const FVector2D Direction = Stick / Magnitude;
	return FVector2D(Curve * Direction.X * Settings.StickYawRateDegPerSec * Sensitivity.Yaw, Curve * Direction.Y * Settings.StickPitchRateDegPerSec * Sensitivity.Pitch);
}

FVector2D StealthLookResponse::FrameDeltaDeg(const FLookFrameInput& Frame, const FStealthCameraUserSettings& Settings, double OuterZoneFrac)
{
	if (Frame.bStick)
	{
		return StickRates(Frame.Stick, Settings, OuterZoneFrac, Frame.Sensitivity) * Frame.DtSec;
	}
	return FVector2D(Frame.MouseDeltaDeg.X * Frame.Sensitivity.Yaw, Frame.MouseDeltaDeg.Y * Frame.Sensitivity.Pitch);
}

void StealthLookResponse::Step(FViewEnvelopeState& State, const FLookFrameInput& Frame, const FStealthCameraUserSettings& Settings, double OuterZoneFrac,
	const FViewEnvelopeParams& EnvelopeParams)
{
	const FVector2D Delta = FrameDeltaDeg(Frame, Settings, OuterZoneFrac);
	FViewEnvelopeInput In;
	In.DeltaYawDeg = Delta.X;
	In.DeltaPitchDeg = Delta.Y;
	In.DtSec = Frame.DtSec;
	In.bConcealed = Frame.bConcealed;
	StealthViewEnvelope::Step(State, In, EnvelopeParams);
}
