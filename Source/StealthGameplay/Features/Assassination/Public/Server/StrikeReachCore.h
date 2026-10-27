#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

struct FAssassinationTuning_Public;
struct FAssassinationTuning_Server;

/**
 * Reach and warp, GDD assassination-defence-reversal.md Formulas F1 (ADR-0018 sections 2.1 step 1 and 2.2; story asn-002).
 *
 *   Reach(A, V) = [d0 - r_c <= R + eps] AND [|dz| <= Z] AND [X_eff <= X_max] AND [|theta_T| <= Theta]
 *   d0      = |P_V - P_A|_xy at the slot (server capsule locations, no rewind, never animated bones)
 *   t_d     = T_v - lambda                         (victim free-motion time before the death write)
 *   B(v)    = v*lambda - a_b*lambda^2/2  if lambda < v/a_b, else v^2/(2*a_b)   (baseline braking after the write)
 *   D(v)    = v*t_d + B(v);  P_V(T_v) = P_V + v_hat * D(|v|)
 *   X       = |P_V(T_v) - P_A|_xy - 2*r_c - g;  X_eff = max(X, 0)
 *   theta_T = bearing of P_V(T_v) from the attacker's body facing at the slot
 *
 * The 0.5 cm epsilon applies to the range clause only (the GDD's inclusive bound); the vertical, warp and bearing
 * clauses are evaluated exactly. Plain C++ with no UWorld: every input is injected, so the formula runs headless
 * (ADR-0007 plain-core rule). Microseconds per call; the resolver evaluates it once per candidate.
 *
 * The result also carries X_eff and theta_T: the strike write (story asn-012) and the clip-variant choice (asn-018) reuse
 * them, so the lunge the client plays and the server's verdict come from one computation.
 *
 * Usage (resolver, in the Attack's slot):
 *   const FStrikeReachParams Params = FStrikeReachParams::FromTuning(Tuning.Assassination_Public, Tuning.Assassination_Server,
 *       EStrikeReachVariant::Standard, BushStrikeEmergeSec, CapsuleRadiusCm);
 *   FStrikeReachInput In;  In.AttackerLocation = A; In.AttackerFacingYawDeg = Yaw; In.VictimLocation = V;
 *   In.VictimVelocity = VictimVel; In.VictimBrakingCmPerSec2 = GaitBraking;   // SCL baseline braking of the victim's gait
 *   const FStrikeReachResult R = FStrikeReachCore::Evaluate(Params, In);       // R.bInReach, R.WarpTranslationEffCm, R.BearingDeg
 */

/** Which strike's timing T_v uses: the standard strike, or the bush strike (adds BushStrikeEmergeSec). */
enum class EStrikeReachVariant : uint8
{
	Standard,
	BushStrike,
};

/** The F1 constants of one evaluation (from validated tuning, never literals). */
struct FStrikeReachParams
{
	/** R, AssassinationRangeCm. */
	double RangeCm = 0.0;
	/** Z, StrikeVerticalReachCm. */
	double VerticalReachCm = 0.0;
	/** T_v: StrikeContactSec, plus BushStrikeEmergeSec for the bush strike. */
	double ContactSec = 0.0;
	/** lambda, StrikeNetLeadSec. */
	double NetLeadSec = 0.0;
	/** X_max, StrikeWarpLimitCm. */
	double WarpLimitCm = 0.0;
	/** g, StrikeContactGapCm. */
	double ContactGapCm = 0.0;
	/** Theta, StrikeBearingLimitDeg. */
	double BearingLimitDeg = 0.0;
	/** r_c, the SCL capsule radius shared by every pawn. */
	double CapsuleRadiusCm = 0.0;

	/** The parameters of Variant from the frozen Assassination tuning; BushStrikeEmergeSec is Concealment's key. */
	STEALTHGAMEPLAY_API static FStrikeReachParams FromTuning(const FAssassinationTuning_Public& Public, const FAssassinationTuning_Server& Server,
		EStrikeReachVariant Variant, double BushStrikeEmergeSec, double CapsuleRadiusCm);
};

/** One attacker-victim pair at the Attack's slot (server state only). */
struct FStrikeReachInput
{
	/** Attacker server capsule location. */
	FVector AttackerLocation = FVector::ZeroVector;
	/** Attacker body facing (actor yaw) at the slot, degrees. */
	double AttackerFacingYawDeg = 0.0;
	/** Victim server capsule location. */
	FVector VictimLocation = FVector::ZeroVector;
	/** Victim server velocity at the slot; only its horizontal part is extrapolated (held until the death write). */
	FVector VictimVelocity = FVector::ZeroVector;
	/** Baseline braking deceleration a_b of the victim's gait (SCL; walk 187, run 600 cm/s^2). 0 = no braking modelled. */
	double VictimBrakingCmPerSec2 = 0.0;
};

/** The verdict and every number behind it. */
struct FStrikeReachResult
{
	/** All four clauses hold. */
	bool bInReach = false;
	/** d0 - r_c <= R (with the 0.5 cm epsilon). */
	bool bRangeOk = false;
	/** |dz| <= Z. */
	bool bVerticalOk = false;
	/** X_eff <= X_max. */
	bool bWarpOk = false;
	/** |theta_T| <= Theta. */
	bool bBearingOk = false;
	/** d0, horizontal distance at the slot. */
	double DistanceCm = 0.0;
	/** dz, victim minus attacker capsule-centre height. */
	double DeltaZCm = 0.0;
	/** D(|v|), the victim's extrapolated travel until contact. */
	double ExtrapolationCm = 0.0;
	/** X, the raw warp translation (may be negative). */
	double WarpTranslationCm = 0.0;
	/** X_eff = max(X, 0), the warp translation the lunge covers. */
	double WarpTranslationEffCm = 0.0;
	/** theta_T in (-180, 180], Unreal yaw convention (positive toward +Y, the attacker's right). */
	double BearingDeg = 0.0;
	/** P_V(T_v), the victim's extrapolated location at contact (Z kept at the slot height). */
	FVector VictimAtContact = FVector::ZeroVector;
};

/** F1 as pure functions. */
class STEALTHGAMEPLAY_API FStrikeReachCore
{
public:
	/** The GDD's inclusive epsilon on the range clause only, cm (F1 "inclusive with 0.5 cm epsilon"). */
	static constexpr double RangeEpsilonCm = 0.5;

	/** B(v): braking distance over lambda after the death write, then to rest. 0 for v <= 0. */
	static double BrakingDistance(double SpeedCmPerSec, double NetLeadSec, double BrakingCmPerSec2);

	/** D(v) = v * max(T_v - lambda, 0) + B(v). */
	static double ExtrapolationDistance(double SpeedCmPerSec, double ContactSec, double NetLeadSec, double BrakingCmPerSec2);

	/** Evaluates F1 for one pair. Pure. */
	static FStrikeReachResult Evaluate(const FStrikeReachParams& Params, const FStrikeReachInput& Input);
};
