#pragma once

#include "CoreMinimal.h"
#include "View/Core/StealthViewEnvelope.h"

struct FStealthCameraUserSettings;
struct FStealthCameraParams;

/**
 * The look response (GDD camera-system.md F6, CS-R9, CS-R14; ADR-0015 section 7.5; story cs-003): the stick's radial dead
 * zone and curve to yaw and pitch rates, the mouse's raw delta times sensitivity, and the one look pipeline step that feeds
 * the view envelope (F1, story cs-002). No smoothing, no acceleration ramp, no filtering, and no aim assist: nothing here
 * reads a pawn, a target, a registry or the world (CS-R9, IIF R15; StealthGame.Camera.NoAimAssist).
 *
 *   m = clamp((|x| - dz) / (outer - dz), 0, 1) on the radial magnitude |x|;  d = x / |x|  (the direction is preserved)
 *   w_yaw = m^k * d_x * Omega_yaw * sens_x;   w_pitch = m^k * d_y * Omega_pitch * sens_y
 *
 * Usage:
 *   const FVector2D Rates = StealthLookResponse::StickRates(Stick, Settings, Params.StickOuterZoneFrac);
 *   StealthLookResponse::Step(Envelope, Frame, Settings, Params, EnvelopeParams);  // one frame of look
 */
struct FLookSensitivity
{
	/** sens_x, sens_y (user setting, 0.33-2, default 1). */
	double Yaw = 1.0;
	double Pitch = 1.0;
};

/** One frame of raw look input. */
struct FLookFrameInput
{
	/** Gamepad: the right stick deflection (each axis -1..1); used when bStick. */
	FVector2D Stick = FVector2D::ZeroVector;
	/** Mouse: this frame's raw delta in degrees (after the device scale); used when !bStick. */
	FVector2D MouseDeltaDeg = FVector2D::ZeroVector;
	bool bStick = true;
	FLookSensitivity Sensitivity;
	double DtSec = 1.0 / 60.0;
	/** The own pawn IsConcealed (the envelope's overshoot rule). */
	bool bConcealed = false;
};

namespace StealthLookResponse
{
	/** F6: the yaw (X) and pitch (Y) rates in degrees per second for a stick deflection. */
	STEALTHGAMEPLAY_API FVector2D StickRates(const FVector2D& Stick, const FStealthCameraUserSettings& Settings, double OuterZoneFrac, const FLookSensitivity& Sensitivity = FLookSensitivity());

	/** This frame's raw look delta in degrees (stick: rate * dt; mouse: delta * sensitivity, no smoothing or acceleration). */
	STEALTHGAMEPLAY_API FVector2D FrameDeltaDeg(const FLookFrameInput& Frame, const FStealthCameraUserSettings& Settings, double OuterZoneFrac);

	/** One frame of the look pipeline: F6 then F1. The only inputs are the frame, the settings and the parameters. */
	STEALTHGAMEPLAY_API void Step(FViewEnvelopeState& State, const FLookFrameInput& Frame, const FStealthCameraUserSettings& Settings, double OuterZoneFrac,
		const FViewEnvelopeParams& EnvelopeParams);
}
