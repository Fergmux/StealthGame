#pragma once

#include "CoreMinimal.h"

struct FStealthCameraParams;

/**
 * The eye-origin visibility mask core (GDD camera-system.md F3, CS-R5b; ADR-0015 sections 4.2-4.3; story cs-012): which
 * registered targets (pawns, and eye-masked elements such as attachments, bush rustle emitters and objective-site effects)
 * the third-person view may draw, from the body's line of sight. Plain C++: traces through an injected async backend, time
 * from the caller. Kind-blind: a target is an index and its probe points; nothing here reads a team, a binding, a Roster
 * state or a controller (AC-CS-07; StealthGame.Camera.MaskParityAndInputs scans for it).
 *
 *   P_m        the mask origin (the eye pivot with z = min(lagged pivot z, real eye-socket z), story cs-006)
 *   InCone(q)  x_q > 0, |atan2(y_q, x_q)| <= C_h, |atan2(z_q, x_q)| <= C_v in the view space at P_m (the fixed eye cone, never
 *              the FOV); a probe outside the cone costs no trace and never counts as visible
 *   Pass(T)    some in-cone probe has a clear ViewBlock trace from P_m (all of a target's in-cone probes are submitted together;
 *              the first clear result decides), at every distance (rate bands, never a cutoff)
 *   Visible    Pass, or (Visible before and fails < N_h); fails = consecutive failed tests, reset by a Pass
 *   Alpha      moveToward(alpha, Visible, dt / T_f); mask-visible <=> alpha > 0
 *   Rate       f_near within EyeMaskNearCm, f_far to EyeMaskRangeCm, f_beyond past it
 *   Cap        at most EyeMaskTracesPerFrameMax traces are submitted per frame; due targets beyond it wait one frame, taken
 *              round-robin in registry index order (never by kind, team or distance)
 *
 * Usage (each frame on the owning client, third person):
 *   FEyeMaskCore Mask(Params);
 *   const int32 Id = Mask.Register();                                  // a pawn or an eye-masked element
 *   Mask.Step(Now, Dt, View, [&](int32 Id, FEyeMaskProbes& Out) { ... return bExists; }, Backend);
 *   const float Alpha = Mask.GetAlpha(Id);                             // the CPD_EyeMaskAlpha dither (story cs-015)
 */

/** A trace handle of the async backend. */
using FEyeMaskTraceHandle = uint32;

/** The async ViewBlock traces (the adapter runs them on the ViewBlock channel; tests script them). */
class IEyeMaskTraceBackend
{
public:
	virtual ~IEyeMaskTraceBackend() = default;

	/** Submits a ViewBlock trace From -> To (async); returns its handle. */
	virtual FEyeMaskTraceHandle SubmitTrace(const FVector& From, const FVector& To) = 0;

	/** The trace's result once it landed: true when blocked. Unset while pending. A polled result is consumed. */
	virtual TOptional<bool> PollResult(FEyeMaskTraceHandle Handle) = 0;
};

/** The mask's view this frame. */
struct FEyeMaskView
{
	/** P_m. */
	FVector Origin = FVector::ZeroVector;
	/** The view rotation (the cone's frame). */
	FRotator Rotation = FRotator::ZeroRotator;
};

/** A target's probe points this frame (pawns: head, chest, pelvis and two lateral capsule points; never bones). */
struct FEyeMaskProbes
{
	static constexpr int32 MaxProbes = 5;
	FVector Points[MaxProbes];
	int32 Num = 0;

	void Add(const FVector& Point)
	{
		if (Num < MaxProbes)
		{
			Points[Num++] = Point;
		}
	}
};

class STEALTHGAMEPLAY_API FEyeMaskCore
{
public:
	/** ADR-0015 section 4.3.5: the per-frame trace cap (a code constant; spike OQ10 confirms it). */
	static constexpr int32 TracesPerFrameMax = 160;

	explicit FEyeMaskCore(const FStealthCameraParams& InParams);

	/** Registers a target; returns its id (the registry index, client-local and kind-blind). */
	int32 Register();
	/** Removes a target (its id is not reused). */
	void Unregister(int32 Id);

	/** The probe provider: false when the target no longer exists this frame. */
	using FProbeProvider = TFunctionRef<bool(int32 /*Id*/, FEyeMaskProbes& /*OutProbes*/)>;

	/** One frame: poll landed results, apply them, submit the due tests within the cap, advance every alpha. */
	void Step(double NowSec, double DtSec, const FEyeMaskView& View, FProbeProvider Probes, IEyeMaskTraceBackend& Backend);

	/** Alpha in [0, 1] (0 when unknown). */
	float GetAlpha(int32 Id) const;
	/** Mask-visible: alpha > 0. */
	bool IsVisible(int32 Id) const { return GetAlpha(Id) > 0.0f; }

	/** InCone(q) at View (pure). */
	bool IsInCone(const FEyeMaskView& View, const FVector& Point) const;

	/** The test period of a target at DistanceCm (the rate bands). */
	double TestPeriodSec(double DistanceCm) const;

	/** Observation (tests, the perf spike): traces submitted in the last Step and tests started. */
	int32 GetLastFrameTraces() const { return LastFrameTraces; }
	int32 GetTestsStarted(int32 Id) const;

private:
	struct FTargetState
	{
		bool bRegistered = false;
		bool bVisible = false;
		int32 Fails = 0;
		float Alpha = 0.0f;
		double LastTestSec = -1.0e9;
		bool bInFlight = false;
		int32 NumPending = 0;
		FEyeMaskTraceHandle Pending[FEyeMaskProbes::MaxProbes] = {};
		bool bLanded[FEyeMaskProbes::MaxProbes] = {};
		int32 TestsStarted = 0;
	};

	void ApplyResult(FTargetState& State, bool bPass) const;

	double NearCm;
	double RangeCm;
	double NearHz;
	double FarHz;
	double BeyondHz;
	double ConeHalfHDeg;
	double ConeHalfVDeg;
	int32 HideFailCount;
	double FadeSec;

	TArray<FTargetState> Targets;
	int32 Cursor = 0;
	int32 LastFrameTraces = 0;
};
