#pragma once

#include "CoreMinimal.h"
#include "View/Tuning/StealthCameraParams.h"

/**
 * The third-person rig core (GDD camera-system.md CS-R4, F2; ADR-0015 section 3; story cs-006): plain C++ with an injected
 * trace backend, so the rules are unit-tested without a world. The camera manager (story cs-004) is the thin adapter that
 * reads the eye socket (owning client only), runs the traces on the Camera channel and applies the output.
 *
 *   Eye pivot   x/y on the capsule axis; z = the eye-socket height clamped inside the capsule, lag-filtered (CameraPivotLagSec;
 *               CameraPivotLagTraversalSec in traversal) with |dz/dt| <= PivotMaxVertCmPerSec. An owner correction below
 *               SCL's PawnSmoothSnapThresholdCm is absorbed into an offset that decays at <= PivotMaxVertCmPerSec (no step);
 *               above it the filter resets and the pivot snaps with the body on the same frame.
 *   Mask origin z = min(lagged pivot z, real eye-socket z): the mask never sees from above the real eye (D6).
 *   Boom (F2)   P_ideal = P + R(ViewYaw) (-L, s O_eff, H) (concealed: -L_c, 0, 0); d_max = |P_ideal - P|;
 *               d_tgt = max(0, min(t_ray, t_s) d_max) with the 2 cm guard ray and the 14 cm probe sweep; pull-in at once;
 *               recovery after BoomRecoverHoldSec: d += min((d_tgt - d)(1 - e^(-dt / tau)), V_rec dt). C = P + u d.
 *               Pitch rotates the lens in place (no orbit). Reduce camera motion turns recovery and the swap into cuts.
 *   Squeeze     a blocked shoulder slides O_eff to 0 over ShoulderSqueezeInSec and back over ShoulderSqueezeOutSec; never a flip.
 *   Swap        a request mirrors the shoulder over ShoulderSwapBlendSec; one press during the blend is buffered and runs once.
 *   Outputs     free boom d, close 3P below BoomCloseCm (the own head hides, story cs-009), the mask-off band (off below 25 cm,
 *               on again above 25 + CameraMaskOffHysteresisCm, decision D6 / C-W25) for story cs-013.
 *
 * Usage (per frame on the owning client):
 *   FStealthCameraCore Core(Params);
 *   const FStealthCameraOutput Out = Core.Step(BodyInput, Traces);   // Out.LensLocation, Out.LensRotation, Out.bMaskOff ...
 */

/** The collision queries the boom needs (the adapter runs them on the Camera channel; tests fake them). 1 = clear. */
class IStealthCameraTraces
{
public:
	virtual ~IStealthCameraTraces() = default;

	/** Hit fraction of a thin ray (radius RadiusCm, treated as a ray) from Start to End. */
	virtual double RayFraction(const FVector& Start, const FVector& End, double RadiusCm) const = 0;

	/** Hit fraction of a sphere sweep of RadiusCm from Start to End. */
	virtual double SweepFraction(const FVector& Start, const FVector& End, double RadiusCm) const = 0;
};

/** One frame of body facts (the owning client's own pawn). */
struct FStealthCameraBodyInput
{
	/** The capsule centre (the pawn's location) and its extents. */
	FVector CapsuleCentre = FVector::ZeroVector;
	double CapsuleHalfHeightCm = 92.0;
	/** The eye socket's world height on this frame (read on the owning client only). */
	double EyeSocketZ = 0.0;
	/** The view rotation (body yaw + envelope offset; pitch). */
	double ViewYawDeg = 0.0;
	double ViewPitchDeg = 0.0;
	double DtSec = 1.0 / 60.0;
	bool bInTraversal = false;
	/** The own pawn IsConcealed (the concealed boom, story cs-014). */
	bool bConcealed = false;
	/** Reduce camera motion (story cs-008): recovery and swap become cuts. */
	bool bReduceMotion = false;
	/** A network correction moved the capsule this frame by CorrectionDelta (owner prediction error). */
	bool bCorrection = false;
	FVector CorrectionDelta = FVector::ZeroVector;
};

struct FStealthCameraOutput
{
	FVector Pivot = FVector::ZeroVector;
	/** The eye-mask origin: the pivot with z = min(lagged pivot z, real eye-socket z). */
	FVector MaskOrigin = FVector::ZeroVector;
	FVector LensLocation = FVector::ZeroVector;
	FRotator LensRotation = FRotator::ZeroRotator;
	/** d_t: the free boom length this frame. */
	double FreeBoomCm = 0.0;
	/** d_max of the frame (the unobstructed length). */
	double IdealBoomCm = 0.0;
	/** The effective shoulder offset (signed by the side). */
	double ShoulderOffsetCm = 0.0;
	/** Below BoomCloseCm: close 3P (the own head hides). */
	bool bCloseThirdPerson = false;
	/** The mask-off band (off below 25 cm of free boom, on again above 25 + hysteresis). */
	bool bMaskOff = false;
	/** The 2 cm guard ray from the pivot to the lens is clear this frame. */
	bool bGuardClear = true;
};

class STEALTHGAMEPLAY_API FStealthCameraCore
{
public:
	/** The mask turns off below this free boom (decision D6, cross-review C-W25: the geometric minimum is 21 cm). */
	static constexpr double MaskOffBelowCm = 25.0;

	explicit FStealthCameraCore(const FStealthCameraParams& InParams);

	/** One frame. */
	FStealthCameraOutput Step(const FStealthCameraBodyInput& Body, const IStealthCameraTraces& Traces);

	/** The shoulder swap request (IIF R14 binding): starts the mirror blend, or buffers one press during a blend. */
	void RequestShoulderSwap();

	/** +1 right shoulder, -1 left (the side the blend is heading to). */
	int32 GetShoulderSide() const { return Side; }
	bool IsSwapBlending() const { return SwapElapsed < SwapBlendSec(); }
	bool HasBufferedSwap() const { return bSwapBuffered; }

	/** Forgets the filter state (possession, view-target change): the next frame snaps to the body. */
	void Reset() { bInitialised = false; }

private:
	double SwapBlendSec() const;

	FStealthCameraParams Params;
	bool bInitialised = false;
	double PivotZ = 0.0;
	FVector CorrectionOffset = FVector::ZeroVector;
	double Boom = 0.0;
	double HoldElapsed = 0.0;
	double SqueezeFrac = 1.0;
	int32 Side = 1;
	/** The lateral factor in [-1, 1] the blend moves through (s, eased). */
	double SideBlend = 1.0;
	double SwapFrom = 1.0;
	double SwapElapsed = 1.0e9;
	bool bSwapBuffered = false;
	bool bMaskOff = false;
};
