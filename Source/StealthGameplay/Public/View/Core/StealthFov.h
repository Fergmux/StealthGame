#pragma once

#include "CoreMinimal.h"

struct FStealthCameraParams;

/**
 * Field of view (GDD camera-system.md F5, CS-R13; ADR-0015 section 7.2; story cs-010). Client-local presentation: the
 * vertical slider (user setting FpFovVDeg / TpFovVDeg, 55-75) converts to horizontal at the display aspect and is capped at
 * FovHMaxDeg for any aspect (fairness: a super-ultrawide player never sees more periphery than 16:9 at the slider top).
 * Nothing here reads the eye-mask cone (I-CS-3 is story cs-001's load check) and nothing here is replicated: a FOV change
 * sends no RPC (the ease is a plain struct the camera manager steps).
 *
 *   HFOV = min(2 atan(tan(VFOV / 2) A), H_max);  capped: VFOV_eff = 2 atan(tan(H_max / 2) / A)
 *   slider top for aspect A: VFOV_top(A) = min(FovVMaxDeg, 2 atan(tan(H_max / 2) / A)), floored at FovVMinDeg
 *
 * Usage:
 *   const FStealthFovResult Fov = StealthFov::Compute(SliderVDeg, Aspect, Params);   // Fov.VerticalEffDeg drives the lens
 *   FStealthFovEase Ease; Ease.SetTarget(70.0, bReduceMotion); Ease.Step(Dt);          // Ease.GetCurrentDeg()
 */
struct FStealthFovResult
{
	/** The horizontal FOV after the cap, degrees. */
	double HorizontalDeg = 0.0;
	/** The vertical FOV that remains after the cap (the chosen vertical when uncapped), degrees. */
	double VerticalEffDeg = 0.0;
	bool bCapped = false;
};

/** The slider's usable range at one aspect. */
struct FStealthFovSliderRange
{
	double MinDeg = 0.0;
	double TopDeg = 0.0;
	/** Every slider position is capped at this aspect: the slider is shown disabled and the view is VFOV_eff. */
	bool bDisabled = false;
};

namespace StealthFov
{
	/** The ease length of a mid-round FOV change (GDD AC-CS-31; instant under Reduce camera motion). */
	inline constexpr double EaseSec = 0.15;

	/** F5 at a vertical slider value and aspect (width / height). */
	STEALTHGAMEPLAY_API FStealthFovResult Compute(double VerticalDeg, double Aspect, const FStealthCameraParams& Params);

	/** The slider's top (and whether it is disabled) at an aspect. */
	STEALTHGAMEPLAY_API FStealthFovSliderRange SliderRange(double Aspect, const FStealthCameraParams& Params);
}

/** The eased FOV the manager applies (smoothstep over EaseSec; instant when Reduce camera motion is on). */
struct STEALTHGAMEPLAY_API FStealthFovEase
{
	/** Starts easing from the current value to TargetDeg (jumps when bReduceMotion or on the first target). */
	void SetTarget(double TargetDeg, bool bReduceMotion);

	/** Advances by DtSec. */
	void Step(double DtSec);

	double GetCurrentDeg() const { return Current; }
	double GetTargetDeg() const { return Target; }
	bool IsEasing() const { return Elapsed < StealthFov::EaseSec && From != Target; }

private:
	double From = 0.0;
	double Target = 0.0;
	double Current = 0.0;
	double Elapsed = StealthFov::EaseSec;
	bool bHasValue = false;
};
