// Story nkp-020 (production/epics/npc-kill-penalty-informants/story-020-penalty-tuning-warnings-and-residual-models.md):
// the Penalty CR-11 rows I-NKP-1 and W-NKP-2 to W-NKP-5 (GDD npc-kill-penalty-informants.md Tuning Knobs; TDC GDD
// owner-invariant rows). Registered in the TDC table (StealthTuningInvariants.cpp); see Server/StealthPenaltyInvariants.h.

#include "Server/StealthPenaltyInvariants.h"

#include "Server/StealthServerTuning.h"

// A named namespace, not an anonymous one: the unity build can merge this file with AssassinationInvariants.cpp, whose
// anonymous-namespace helpers have the same names.
namespace PenaltyInvariantsLocal
{
	using StealthPenaltyInvariants::CompareToleranceAbs;

	/** A <= B within the tolerance. */
	bool AtMost(double A, double B) { return A <= B + CompareToleranceAbs; }

	ETuningCheck WarnUnless(bool bHolds) { return bHolds ? ETuningCheck::Pass : ETuningCheck::Warn; }

	const StealthPenaltyInvariants::FRow PenaltyInvariantRows[] = {
		{ "I-NKP-1", &StealthPenaltyInvariants::CheckINkp1 },
		{ "W-NKP-2", &StealthPenaltyInvariants::CheckWNkp2 },
		{ "W-NKP-3", &StealthPenaltyInvariants::CheckWNkp3 },
		{ "W-NKP-4", &StealthPenaltyInvariants::CheckWNkp4 },
		{ "W-NKP-5", &StealthPenaltyInvariants::CheckWNkp5 },
	};
}

FPenaltyInvariantInputs FPenaltyInvariantInputs::FromValues(const FStealthServerTuning& Values)
{
	FPenaltyInvariantInputs Out;
	Out.RespawnStarvationSec = StealthTuningPendingOwnerKeys::RespawnStarvationSec;
	Out.NoPawnFadeSec = static_cast<double>(Values.Camera_Public.NoPawnFadeSec); // registered by story cs-001
	Out.BushStrikeEmergeSec = static_cast<double>(Values.Concealment_Public.BushStrikeEmergeSec); // registered by story con-001
	return Out;
}

namespace StealthPenaltyInvariants
{
	using PenaltyInvariantsLocal::AtMost;
	using PenaltyInvariantsLocal::WarnUnless;
	using PenaltyInvariantsLocal::PenaltyInvariantRows;

	ETuningCheck CheckINkp1(const FStealthServerTuning& V, const FPenaltyInvariantInputs&, const FTuningCheckContext& Context)
	{
		if (V.Penalty_Public.NpcKillStrikeLimit != 1)
		{
			return ETuningCheck::Pass;
		}
		return Context.bIsPublic ? ETuningCheck::Reject : ETuningCheck::Warn;
	}

	ETuningCheck CheckWNkp2(const FStealthServerTuning& V, const FPenaltyInvariantInputs& In, const FTuningCheckContext&)
	{
		const double Starved = static_cast<double>(V.Penalty_Public.InformantRespawnDelaySec) + StarvationWideningCount * In.RespawnStarvationSec;
		return WarnUnless(AtMost(Starved, StarvedBlackScreenBudgetSec));
	}

	ETuningCheck CheckWNkp3(const FStealthServerTuning& V, const FPenaltyInvariantInputs&, const FTuningCheckContext&)
	{
		return WarnUnless(AtMost(static_cast<double>(V.Assassination_Public.AttackAttemptLockoutSec), static_cast<double>(V.Penalty_Server.StrikeOutReleaseGraceSec)));
	}

	ETuningCheck FloorGuard(double DeathFallSec, double NoPawnFadeSec, double InformantRespawnDelaySec)
	{
		return WarnUnless(AtMost(BushStrikeCapSec + DeathFallSec + NoPawnFadeSec, InformantRespawnDelaySec));
	}

	ETuningCheck CheckWNkp4(const FStealthServerTuning& V, const FPenaltyInvariantInputs& In, const FTuningCheckContext&)
	{
		return FloorGuard(V.Assassination_Public.DeathFallSec, In.NoPawnFadeSec, V.Penalty_Public.InformantRespawnDelaySec);
	}

	ETuningCheck CheckWNkp5(const FStealthServerTuning& V, const FPenaltyInvariantInputs& In, const FTuningCheckContext&)
	{
		const double LatestCardSec = static_cast<double>(V.Assassination_Public.StrikeContactSec) + In.BushStrikeEmergeSec
			+ static_cast<double>(V.Penalty_Public.ExileBeatSec) + CardRpcAllowanceSec;
		return WarnUnless(AtMost(LatestCardSec, static_cast<double>(V.Penalty_Public.ExileCardTimeoutSec)));
	}

	TConstArrayView<FRow> GetRows()
	{
		return MakeArrayView(PenaltyInvariantRows);
	}

	FPenaltyInvariantReport Evaluate(const FStealthServerTuning& Values, const FPenaltyInvariantInputs& Inputs, const FTuningCheckContext& Context)
	{
		FPenaltyInvariantReport Report;
		for (const FRow& Row : PenaltyInvariantRows)
		{
			const ETuningCheck Result = Row.Check(Values, Inputs, Context);
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
