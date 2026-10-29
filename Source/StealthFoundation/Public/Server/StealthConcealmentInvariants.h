#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningInvariants.h"

struct FStealthServerTuning;

/**
 * The Concealment (Bushes) F3 timing chain as ADR-0004 Check rows (GDD concealment-bushes.md F3, Tuning Knobs, AC-CON-11
 * data part; ADR-0017 section 3; story con-001). F3 is a pass/fail constraint set over the clamped (post F-1) value set and
 * injected constants, never a set of independent safe ranges: the per-key range gate (F-1) runs first and this validator
 * after it, and the report names both. Pure; never writes a value (CR-6). Runs at preset load only, never per frame.
 *
 *   (a) BushEnterDurSec >= BushEnterSwapSec + ConcealCullDelayMs / 1000              every exit starts Culled
 *   (b) BushExitCrossSec >= T_uncull = L99 + 1 / f_net + T_reopen                     the body shows after the re-open
 *   (c) BushStrikeEmergeSec >= T_uncull                                               the attacker exists before it shows
 *   (d) (BushEntryReachCm + r_c) / BushEnterSwapSec <= 1.5 * v_walk                    calm-gait warp to the anchor
 *   (e) BushEnterSwapSec < BushEnterDurSec AND BushExitCrossSec < BushExitDurSec
 *   (f) StrikeContactSec + BushStrikeEmergeSec <= 1.2                                 Assassination I-ASN-3
 *
 * Injected constants at preset load (FConcealmentF3Inputs::FromValues): ConcealCullDelayMs (IVP owner key, pending: 250),
 * L99 150 ms and T_reopen 33 ms (the IVP F4 LeakNetProfile inputs, owned by the IVP epic), f_net = PawnNetUpdateHz (SCL
 * schema key), v_walk = WalkSpeedCmPerSec (SCL schema key), r_c = SclCapsuleRadiusCm (schema constant, provisional 35),
 * StrikeContactSec (Assassination schema key). Comparisons use CompareToleranceAbs (keys are floats authored in decimals).
 *
 * Usage (test, range gate bypassed):
 *   FStealthServerTuning V; V.Concealment_Public.BushExitCrossSec = 0.2f;
 *   const uint8 Violated = StealthConcealmentInvariants::EvaluateF3(V, FConcealmentF3Inputs::FromValues(V));
 *   // Violated == StealthConcealmentInvariants::F3b
 */

/** The values F3 reads that are not Concealment keys. */
struct FConcealmentF3Inputs
{
	/** IVP ConcealCullDelayMs (commit to cull), ms. */
	double ConcealCullDelayMs = 250.0;
	/** IVP F4 p99 one-way latency, ms. */
	double L99Ms = 150.0;
	/** Pawn net update rate f_net, Hz. */
	double PawnNetUpdateHz = 15.0;
	/** IVP F4 channel re-open time T_reopen, ms. */
	double ReopenMs = 33.0;
	/** SCL walk speed v_walk, cm/s. */
	double WalkSpeedCmPerSec = 140.0;
	/** SCL capsule radius r_c, cm. */
	double CapsuleRadiusCm = 35.0;
	/** Assassination StrikeContactSec (execution to contact of the standard strike), s. */
	double StrikeContactSec = 0.4;

	/** T_uncull = L99 + 1 / f_net + T_reopen, in seconds. */
	double UncullSec() const { return (L99Ms + ReopenMs) / 1000.0 + (PawnNetUpdateHz > 0.0 ? 1.0 / PawnNetUpdateHz : 0.0); }

	/** The inputs a preset load sees: schema keys from Values, the IVP constants and pending owner keys otherwise. */
	STEALTHFOUNDATION_API static FConcealmentF3Inputs FromValues(const FStealthServerTuning& Values);
};

namespace StealthConcealmentInvariants
{
	/** Absolute tolerance of every comparison (seconds and cm/s; far below authored steps). */
	inline constexpr double CompareToleranceAbs = 1e-4;
	/** IVP F4 LeakNetProfile p99 latency used at preset load until the IVP epic registers its profile constants. */
	inline constexpr double LeakNetProfileL99Ms = 150.0;
	/** IVP F4 provisional channel re-open time T_reopen (spike S10 measured 13 ms p99; the GDD keeps the 33 ms budget). */
	inline constexpr double ChannelReopenMs = 33.0;
	/** F3(d): the warp's mean speed may be at most this multiple of the walk speed. */
	inline constexpr double WarpSpeedWalkMult = 1.5;
	/** F3(f): StrikeContactSec + BushStrikeEmergeSec may be at most this (I-ASN-3). */
	inline constexpr double StrikePlusEmergeMaxSec = 1.2;

	/** F3 constraint bits (EvaluateF3's result). */
	inline constexpr uint8 F3a = 1 << 0;
	inline constexpr uint8 F3b = 1 << 1;
	inline constexpr uint8 F3c = 1 << 2;
	inline constexpr uint8 F3d = 1 << 3;
	inline constexpr uint8 F3e = 1 << 4;
	inline constexpr uint8 F3f = 1 << 5;

	/** The violated F3 constraints (0 = all pass) over a value set and injected inputs. Pure. */
	STEALTHFOUNDATION_API uint8 EvaluateF3(const FStealthServerTuning& V, const FConcealmentF3Inputs& In);

	/** F3(d)'s warp mean speed (BushEntryReachCm + r_c) / BushEnterSwapSec in cm/s (the largest double at a zero swap). */
	STEALTHFOUNDATION_API double WarpMeanSpeedCmPerSec(const FStealthServerTuning& V, const FConcealmentF3Inputs& In);

	/** The constraint letters of a result ("ae" for F3a | F3e; empty for 0). */
	STEALTHFOUNDATION_API FString DescribeF3(uint8 Violated);

	/** The TDC table adapter of one constraint: Reject when Bit is violated at the preset-load inputs. */
	template <uint8 Bit>
	ETuningCheck TableCheck(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext&)
	{
		return (EvaluateF3(Values, FConcealmentF3Inputs::FromValues(Values)) & Bit) != 0 ? ETuningCheck::Reject : ETuningCheck::Pass;
	}
}
