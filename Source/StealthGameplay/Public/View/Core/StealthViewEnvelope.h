#pragma once

#include "CoreMinimal.h"

struct FStealthCameraParams;

/**
 * The view envelope (GDD camera-system.md F1, CS-R3; ADR-0015 section 2; story cs-002): a pure per-frame function from the
 * frame's raw look delta (after the stick response F6 and sensitivity, before any gain) to the view offset from the body
 * and the head-look target. No UObject, clock or static state: the camera (the owning client) and Crowd (the server, ADR-0016)
 * call the same function, and the unit tests need no world.
 *
 *   View yaw    outward |v| <- min(Ylim, |v| + g * |D|), g = max(g_min, 1 - smoothstep(Ylim - S_y, Ylim, |v|));
 *               inward v <- v + D (gain 1, the same frame). ViewYaw = BodyYaw + v.
 *   Target      r = v while |v| < Ylim. At |v| = Ylim: pushing: |r| <- min(|r| + |D|, Ylim + E_eff); not pushing:
 *               |r| <- max(Ylim, |r| - rho_relax * dt). Any inward input first sets |r| <- min(|r|, Ylim).
 *   Pushing     some outward input in [t - T_latch, t] had a raw rate |D| / dt >= rho_min (the gain is never applied here).
 *   Overshoot   E_eff = E, or 0 while the own pawn IsConcealed.
 *   View pitch  the same gain ramp over S_p at both limits, pitch in [-P_down, +P_up], no push.
 *   Head lead   CS-R3.5 fallback (OQ2): with ViewHeadLeadMaxDeg enabled, |v - HeadYawOffset| <= ViewHeadLeadMaxDeg; disabled
 *               (the default sentinel) it is a no-op, so enabling it later changes data, not code.
 *
 * Usage (per frame, fixed dt):
 *   FViewEnvelopeState State;
 *   const FViewEnvelopeParams Params = FViewEnvelopeParams::FromCamera(CameraParams);
 *   StealthViewEnvelope::Step(State, { DeltaYawDeg, DeltaPitchDeg, Dt, bOwnConcealed, HeadYawOffsetDeg }, Params);
 *   const double ViewYaw = BodyYaw + State.ViewYawOffsetDeg;   // the head-look target: BodyYaw + State.TargetYawOffsetDeg
 */
struct FViewEnvelopeParams
{
	/** SCL LookYawLimitDeg (Ylim). */
	double YawLimitDeg = 70.0;
	/** ViewPitchDownLimitDeg (P_down) and ViewPitchUpLimitDeg (P_up). */
	double PitchDownLimitDeg = 60.0;
	double PitchUpLimitDeg = 40.0;
	/** CameraEdgeSoftYawDeg (S_y) and CameraEdgeSoftPitchDeg (S_p). */
	double SoftYawDeg = 10.0;
	double SoftPitchDeg = 5.0;
	/** CameraEdgeSoftMinGain (g_min). */
	double MinGain = 0.1;
	/** CameraEdgePushDeg (E). */
	double PushDeg = 5.0;
	/** EdgePushMinRateDegPerSec (rho_min). */
	double PushMinRateDegPerSec = 15.0;
	/** EdgePushLatchSec (T_latch). */
	double PushLatchSec = 0.15;
	/** EdgePushRelaxDegPerSec (rho_relax). */
	double RelaxDegPerSec = 60.0;
	/** ViewHeadLeadMaxDeg (0: the disabled sentinel). */
	double HeadLeadMaxDeg = 0.0;

	/** The envelope's parameters from the camera parameter set (Ylim from the consumed SCL key). */
	STEALTHGAMEPLAY_API static FViewEnvelopeParams FromCamera(const FStealthCameraParams& Camera);
};

/** The envelope's state (view offset, target, push latch). */
struct FViewEnvelopeState
{
	/** v: the view yaw relative to the body yaw, degrees. */
	double ViewYawOffsetDeg = 0.0;
	/** r: the head-look target yaw relative to the body yaw, degrees. */
	double TargetYawOffsetDeg = 0.0;
	/** The view (and aim) pitch, degrees, in [-P_down, +P_up]. */
	double ViewPitchDeg = 0.0;
	/** The envelope's own time, accumulated from the frames' dt. */
	double TimeSec = 0.0;
	/** The time of the last outward input whose raw rate counted as a push (very negative: none). */
	double LastPushSec = -1.0e9;
	/** The latched push (for observers: SCL R5.5 reads the target's crossing, not this flag). */
	bool bPushing = false;
};

/** One frame's inputs. */
struct FViewEnvelopeInput
{
	/** Raw look delta this frame (after F6 and sensitivity, before any gain), degrees; positive yaw is right, pitch up. */
	double DeltaYawDeg = 0.0;
	double DeltaPitchDeg = 0.0;
	/** The frame's dt, seconds. */
	double DtSec = 1.0 / 60.0;
	/** The own pawn IsConcealed: no overshoot (E_eff = 0). */
	bool bConcealed = false;
	/** The replicated head yaw relative to the body (the CS-R3.5 clamp's reference; unused while it is disabled). */
	double HeadYawOffsetDeg = 0.0;
};

namespace StealthViewEnvelope
{
	/** The soft-edge gain at |Value| against Limit over Soft: max(g_min, 1 - smoothstep(Limit - Soft, Limit, |Value|)). */
	STEALTHGAMEPLAY_API double SoftGain(double AbsValue, double Limit, double Soft, double MinGain);

	/** Advances State by one frame. Pure in State, Input and Params. */
	STEALTHGAMEPLAY_API void Step(FViewEnvelopeState& State, const FViewEnvelopeInput& Input, const FViewEnvelopeParams& Params);
}
