#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningInvariants.h"

struct FStealthServerTuning;

/**
 * The Assassination & Defence Reversal cross-field invariants (GDD assassination-defence-reversal.md Tuning Knobs,
 * "Cross-field invariants"; TDC GDD owner-invariant rows; ADR-0004 section 3; story asn-001).
 *
 * Every check is a pure function over the clamped (post F-1) value set and the externally owned inputs; it returns
 * Pass, Warn or Reject and never writes a value (CR-6: reject or warn, never clamp or repair). The rows are registered in
 * the one TDC invariant table (Private/Tuning/StealthTuningInvariants.cpp) through an adapter that reads the inputs with
 * FAssassinationInvariantInputs::FromValues; tests call Evaluate with injected inputs.
 *
 *   Reject: I-ASN-1 R - r_c - g <= X_max             I-ASN-2 T_c + 0.4 <= Counter <= Lockout
 *           I-ASN-3 T_c + BushStrikeEmerge <= 1.2      I-ASN-4 NetLead <= T_c / 2
 *           I-ASN-5 DeathFall >= 1.1                   I-ASN-8 BearingLimit <= 60 + WarpYawLimit
 *   Warn:   I-ASN-5w Fall + Linger + Grace < CrowdReplacementMinDelay (strict)
 *           I-ASN-6 Lockout >= T_c + 1.1               I-ASN-7 X_max / T_c <= 600 cm/s
 *           W-ASN-9 OpenSpotTolerance == PawnRefAgreement (forged-ref radius parity)
 *           W-ASN-10 Lockout > T_c + BushStrikeEmerge (strict)
 *           W-ASN-11 Fall + Linger + Grace < InformantRespawnDelay (strict)
 *
 * Comparisons use StealthAssassinationInvariants::CompareToleranceAbs: the keys are floats authored in decimals, so a
 * widened sum such as 0.4f + 1.1 sits a few 1e-9 off its decimal value; inclusive bounds accept within the tolerance and
 * strict bounds need a margin above it (story asn-001 decision, logged).
 *
 * Usage (test):
 *   FAssassinationInvariantInputs Inputs = FAssassinationInvariantInputs::FromValues(Values);
 *   Inputs.BushStrikeEmergeSec = 0.5;
 *   const FAssassinationInvariantReport Report = StealthAssassinationInvariants::Evaluate(Values, Inputs);
 *   // Report.Rejected == { "I-ASN-3" }, Report.Warned == { "I-ASN-6" }
 */

/** The values the Assassination invariants read that Assassination does not own. */
struct FAssassinationInvariantInputs
{
	/** SCL capsule radius r_c (schema constant SclCapsuleRadiusCm, provisional 35). */
	double CapsuleRadiusCm = 0.0;
	/** Concealment's bush-strike emergence time (pending owner key, 0.35). */
	double BushStrikeEmergeSec = 0.0;
	/** Crowd's minimum replacement delay after a kill (schema key CrowdReplacementMinDelaySec). */
	double CrowdReplacementMinDelaySec = 0.0;
	/** Penalty's informant respawn delay (pending owner key, 15). */
	double InformantRespawnDelaySec = 0.0;
	/** IIF's PawnRef agreement radius (pending owner key, 75). */
	double PawnRefAgreementCm = 0.0;

	/** The inputs a preset load sees: schema keys from Values, schema constants and pending owner keys otherwise. */
	STEALTHFOUNDATION_API static FAssassinationInvariantInputs FromValues(const FStealthServerTuning& Values);
};

/** The findings of one evaluation: the Ids of the rows that rejected and, separately, the rows that warned. */
struct FAssassinationInvariantReport
{
	TArray<FString> Rejected;
	TArray<FString> Warned;
};

namespace StealthAssassinationInvariants
{
	/** Absolute tolerance of every invariant comparison (seconds, cm, degrees or cm/s alike; far below authored steps). */
	inline constexpr double CompareToleranceAbs = 1e-4;

	/** One Assassination check over the value set and the externally owned inputs. */
	using FCheck = ETuningCheck (*)(const FStealthServerTuning&, const FAssassinationInvariantInputs&);

	/** One row: the exact GDD Id and its check. */
	struct FRow
	{
		const ANSICHAR* Id = nullptr;
		FCheck Check = nullptr;
	};

	STEALTHFOUNDATION_API ETuningCheck CheckIAsn1(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckIAsn2(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckIAsn3(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckIAsn4(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckIAsn5(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckIAsn5w(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckIAsn6(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckIAsn7(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckIAsn8(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckWAsn9(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckWAsn10(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckWAsn11(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In);

	/** Every Assassination row in GDD order (the order the TDC table registers them). */
	STEALTHFOUNDATION_API TConstArrayView<FRow> GetRows();

	/** Runs every row over Values with Inputs. Pure. */
	STEALTHFOUNDATION_API FAssassinationInvariantReport Evaluate(const FStealthServerTuning& Values, const FAssassinationInvariantInputs& Inputs);

	/** The TDC table adapter: runs Check with FAssassinationInvariantInputs::FromValues(Values). */
	template <FCheck Check>
	ETuningCheck TableCheck(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext&)
	{
		return Check(Values, FAssassinationInvariantInputs::FromValues(Values));
	}
}
