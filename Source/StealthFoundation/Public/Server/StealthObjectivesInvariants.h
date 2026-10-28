#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningInvariants.h"

struct FStealthServerTuning;

/**
 * The Objectives Chain cross-field rows I-OC1 to I-OC3 (GDD objectives-chain.md Tuning Knobs, OC-6, OC-14, F4; ADR-0020
 * section 8; story oc-001). Each check is a pure function over the clamped (post F-1) value set; it returns Pass, Warn or
 * Reject and never writes a value (TDC CR-6: an invariant never repairs a value, ADR-0004 guideline 5). The rows are
 * registered in the one TDC table (Private/Tuning/StealthTuningInvariants.cpp) with owner "Objectives". The Crowd keys they
 * read (CrowdAmbientTouchMaxSec, CrowdDecoyEarlyReleaseMinSec) are Crowd's schema rows, read from the value set, never
 * copied. Validation runs at preset load only, never per frame.
 *
 *   I-OC1  ObjectiveInteractSec >= CrowdAmbientTouchMaxSec + 0.5                          Warn (a tend outlasts a touch)
 *   I-OC2  W_eff = (RivalSignalDelayMaxSec - RivalSignalDelayMinSec) + RivalSignalReleaseQuantumSec (F4):
 *          Reject when Max < Min + 30 or W_eff < 100 s; Warn when W_eff < 163 s            mixed row (reject and warn clause)
 *   I-OC3  TendCueDelaySec + 0.25 >= CrowdDecoyEarlyReleaseMinSec AND
 *          TendCueDelaySec <= ObjectiveInteractSec - 1.0                                   Reject
 *
 * Boundaries are inclusive in the GDD's direction (I-OC1 ">=", the TDC owner-invariant mechanism of story tdc-055); every
 * comparison uses CompareToleranceAbs because the keys are floats authored in decimals (0.55f + 0.25 must equal 0.8f).
 *
 * Usage (test):
 *   const ETuningCheck R = StealthObjectivesInvariants::CheckIOc2(Values);
 *   const double W = StealthObjectivesInvariants::EffectiveWindowSec(Values);         // 165 at the defaults
 *   const double LR = StealthObjectivesInvariants::SignalLikelihoodRatio(W, 0.0033);  // 2.38 <= 2.4
 */
namespace StealthObjectivesInvariants
{
	/** Absolute tolerance of every comparison (seconds; far below authored steps). */
	inline constexpr double CompareToleranceAbs = 1e-4;
	/** I-OC1: margin a tend must outlast the longest ambient touch by. */
	inline constexpr double InteractOverTouchMarginSec = 0.5;
	/** I-OC2: W_eff below this rejects (F4; TDC CR-6 fallback, never clamped). */
	inline constexpr double WindowRejectBelowSec = 100.0;
	/** I-OC2: W_eff below this warns (F4: W_eff >= 0.539 / lambda_c,min). */
	inline constexpr double WindowWarnBelowSec = 163.0;
	/** I-OC2: the delay window must be at least this wide (Max >= Min + 30). */
	inline constexpr double DelaySpreadMinSec = 30.0;
	/** I-OC3: reaction allowance added to the cue delay before comparing with the earliest decoy release. */
	inline constexpr double CueReactionAllowanceSec = 0.25;
	/** I-OC3: the cue must come at least this long before the hold completes. */
	inline constexpr double CueBeforeCompleteSec = 1.0;
	/** F4 design bound: the largest evidence a matched release may add. */
	inline constexpr double SignalLikelihoodRatioMax = 2.4;
	/** F4: the slow-round background rate of rival releases (per second), the binding case. */
	inline constexpr double ReleaseRateMinPerSec = 0.0033;

	/** The I-OC2 clause texts (the report names the clause that produced the row's result, story tdc-055). */
	inline constexpr const ANSICHAR* IOc2RejectClause = "W_eff < 100 s or RivalSignalDelayMaxSec < RivalSignalDelayMinSec + 30";
	inline constexpr const ANSICHAR* IOc2WarnClause = "W_eff < 163 s";

	/** F4 W_eff = (Max - Min) + Q in seconds, from the value set. Pure. */
	STEALTHFOUNDATION_API double EffectiveWindowSec(const FStealthServerTuning& V);

	/** F4 LR_sig = 1 / (1 - e^(-lambda * W_eff)). Pure; a W_eff or lambda <= 0 returns the largest double. */
	STEALTHFOUNDATION_API double SignalLikelihoodRatio(double EffectiveWindowSec, double ReleaseRatePerSec);

	/** I-OC1 (Warn). */
	STEALTHFOUNDATION_API ETuningCheck CheckIOc1(const FStealthServerTuning& V);
	/** I-OC2 (Reject or Warn, see the namespace comment). */
	STEALTHFOUNDATION_API ETuningCheck CheckIOc2(const FStealthServerTuning& V);
	/** I-OC3 (Reject). */
	STEALTHFOUNDATION_API ETuningCheck CheckIOc3(const FStealthServerTuning& V);

	/** The TDC table adapter (the rows need neither the F-3 indicators nor the context). */
	template <ETuningCheck (*Check)(const FStealthServerTuning&)>
	ETuningCheck TableCheck(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext&)
	{
		return Check(Values);
	}
}
