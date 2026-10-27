// Story asn-001 (production/epics/assassination-defence-reversal/story-001-assassination-tuning-keys-and-invariants.md):
// the Assassination cross-field invariants I-ASN-1 to I-ASN-8, I-ASN-5w, W-ASN-9, W-ASN-10 and W-ASN-11 (GDD
// assassination-defence-reversal.md Tuning Knobs; TDC GDD owner-invariant rows). Registered in the TDC table
// (StealthTuningInvariants.cpp); see Server/StealthAssassinationInvariants.h.

#include "Server/StealthAssassinationInvariants.h"

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningConstants.h"

namespace
{
	using StealthAssassinationInvariants::CompareToleranceAbs;

	/** A <= B within the tolerance. */
	bool AtMost(double A, double B) { return A <= B + CompareToleranceAbs; }
	/** A < B by more than the tolerance (a strict GDD inequality). */
	bool StrictlyBelow(double A, double B) { return A < B - CompareToleranceAbs; }

	/** Fall + linger + grace: the longest a killed body exists after its death write (I-ASN-5w, W-ASN-11). */
	double BodyLifetimeSec(const FAssassinationTuning_Public& A)
	{
		return static_cast<double>(A.DeathFallSec) + static_cast<double>(A.BodyLingerSec) + static_cast<double>(A.BodyRemovalGraceSec);
	}

	ETuningCheck RejectUnless(bool bHolds) { return bHolds ? ETuningCheck::Pass : ETuningCheck::Reject; }
	ETuningCheck WarnUnless(bool bHolds) { return bHolds ? ETuningCheck::Pass : ETuningCheck::Warn; }

	const StealthAssassinationInvariants::FRow Rows[] = {
		{ "I-ASN-1", &StealthAssassinationInvariants::CheckIAsn1 },
		{ "I-ASN-2", &StealthAssassinationInvariants::CheckIAsn2 },
		{ "I-ASN-3", &StealthAssassinationInvariants::CheckIAsn3 },
		{ "I-ASN-4", &StealthAssassinationInvariants::CheckIAsn4 },
		{ "I-ASN-5", &StealthAssassinationInvariants::CheckIAsn5 },
		{ "I-ASN-5w", &StealthAssassinationInvariants::CheckIAsn5w },
		{ "I-ASN-6", &StealthAssassinationInvariants::CheckIAsn6 },
		{ "I-ASN-7", &StealthAssassinationInvariants::CheckIAsn7 },
		{ "I-ASN-8", &StealthAssassinationInvariants::CheckIAsn8 },
		{ "W-ASN-9", &StealthAssassinationInvariants::CheckWAsn9 },
		{ "W-ASN-10", &StealthAssassinationInvariants::CheckWAsn10 },
		{ "W-ASN-11", &StealthAssassinationInvariants::CheckWAsn11 },
	};
}

FAssassinationInvariantInputs FAssassinationInvariantInputs::FromValues(const FStealthServerTuning& Values)
{
	FAssassinationInvariantInputs Out;
	Out.CapsuleRadiusCm = StealthTuningConstants::SclCapsuleRadiusCm;
	Out.BushStrikeEmergeSec = StealthTuningPendingOwnerKeys::BushStrikeEmergeSec;
	Out.CrowdReplacementMinDelaySec = Values.Crowd_Server.CrowdReplacementMinDelaySec;
	Out.InformantRespawnDelaySec = StealthTuningPendingOwnerKeys::InformantRespawnDelaySec;
	Out.PawnRefAgreementCm = StealthTuningPendingOwnerKeys::PawnRefAgreementCm;
	return Out;
}

namespace StealthAssassinationInvariants
{
	ETuningCheck CheckIAsn1(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In)
	{
		const FAssassinationTuning_Public& A = V.Assassination_Public;
		return RejectUnless(AtMost(static_cast<double>(A.AssassinationRangeCm) - In.CapsuleRadiusCm - A.StrikeContactGapCm, A.StrikeWarpLimitCm));
	}

	ETuningCheck CheckIAsn2(const FStealthServerTuning& V, const FAssassinationInvariantInputs&)
	{
		const FAssassinationTuning_Public& A = V.Assassination_Public;
		return RejectUnless(AtMost(static_cast<double>(A.StrikeContactSec) + 0.4, A.CounterDurationSec) && AtMost(A.CounterDurationSec, A.AttackAttemptLockoutSec));
	}

	ETuningCheck CheckIAsn3(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In)
	{
		return RejectUnless(AtMost(static_cast<double>(V.Assassination_Public.StrikeContactSec) + In.BushStrikeEmergeSec, 1.2));
	}

	ETuningCheck CheckIAsn4(const FStealthServerTuning& V, const FAssassinationInvariantInputs&)
	{
		return RejectUnless(AtMost(V.Assassination_Server.StrikeNetLeadSec, static_cast<double>(V.Assassination_Public.StrikeContactSec) / 2.0));
	}

	ETuningCheck CheckIAsn5(const FStealthServerTuning& V, const FAssassinationInvariantInputs&)
	{
		return RejectUnless(AtMost(1.1, V.Assassination_Public.DeathFallSec));
	}

	ETuningCheck CheckIAsn5w(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In)
	{
		return WarnUnless(StrictlyBelow(BodyLifetimeSec(V.Assassination_Public), In.CrowdReplacementMinDelaySec));
	}

	ETuningCheck CheckIAsn6(const FStealthServerTuning& V, const FAssassinationInvariantInputs&)
	{
		const FAssassinationTuning_Public& A = V.Assassination_Public;
		return WarnUnless(AtMost(static_cast<double>(A.StrikeContactSec) + 1.1, A.AttackAttemptLockoutSec));
	}

	ETuningCheck CheckIAsn7(const FStealthServerTuning& V, const FAssassinationInvariantInputs&)
	{
		const FAssassinationTuning_Public& A = V.Assassination_Public;
		const double ContactSec = FMath::Max(static_cast<double>(A.StrikeContactSec), UE_DOUBLE_SMALL_NUMBER);
		return WarnUnless(AtMost(static_cast<double>(A.StrikeWarpLimitCm) / ContactSec, 600.0));
	}

	ETuningCheck CheckIAsn8(const FStealthServerTuning& V, const FAssassinationInvariantInputs&)
	{
		const FAssassinationTuning_Public& A = V.Assassination_Public;
		return RejectUnless(AtMost(A.StrikeBearingLimitDeg, 60.0 + A.StrikeWarpYawLimitDeg));
	}

	ETuningCheck CheckWAsn9(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In)
	{
		return WarnUnless(FMath::Abs(static_cast<double>(V.Assassination_Server.OpenSpotToleranceCm) - In.PawnRefAgreementCm) <= CompareToleranceAbs);
	}

	ETuningCheck CheckWAsn10(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In)
	{
		const FAssassinationTuning_Public& A = V.Assassination_Public;
		return WarnUnless(StrictlyBelow(static_cast<double>(A.StrikeContactSec) + In.BushStrikeEmergeSec, A.AttackAttemptLockoutSec));
	}

	ETuningCheck CheckWAsn11(const FStealthServerTuning& V, const FAssassinationInvariantInputs& In)
	{
		return WarnUnless(StrictlyBelow(BodyLifetimeSec(V.Assassination_Public), In.InformantRespawnDelaySec));
	}

	TConstArrayView<FRow> GetRows()
	{
		return MakeArrayView(Rows);
	}

	FAssassinationInvariantReport Evaluate(const FStealthServerTuning& Values, const FAssassinationInvariantInputs& Inputs)
	{
		FAssassinationInvariantReport Report;
		for (const FRow& Row : Rows)
		{
			const ETuningCheck Result = Row.Check(Values, Inputs);
			if (Result == ETuningCheck::Reject)
			{
				Report.Rejected.Add(FString(Row.Id));
			}
			else if (Result == ETuningCheck::Warn)
			{
				Report.Warned.Add(FString(Row.Id));
			}
		}
		return Report;
	}
}
