// Stories tdc-048 / tdc-055: the invariant table (TuningInvariants.cpp of ADR-0004 section 3). See
// Server/StealthTuningInvariants.h. One row per GDD invariant; each Check is pure over the clamped
// const set and returns Pass, Warn or Reject. A new cross-field constraint arrives through a TDC change
// request (CR-15.6) and is added here as a row, never as a check inside the owner's system.

#include "Server/StealthTuningInvariants.h"

#include "Server/StealthAssassinationInvariants.h"
#include "Server/StealthObjectivesInvariants.h"
#include "Server/StealthConcealmentInvariants.h"
#include "Server/StealthPenaltyInvariants.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningConstants.h"
#include "Server/StealthTuningDerived.h"
#include "Misc/StringBuilder.h"
#include "GameFramework/GameNetworkManager.h"

namespace
{
	/** I-1: DefenceCooldownSec >= 8 * DefenceDurationSec (float comparison; 8 * is exact). Reject. */
	ETuningCheck CheckI1(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext&)
	{
		const float Duration = Values.Assassination_Public.DefenceDurationSec;
		const float Cooldown = Values.Assassination_Public.DefenceCooldownSec;
		return Cooldown >= 8.0f * Duration ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-2: NpcCount >= 2 * TeamSize (CrowdRatio >= 1). Reject. */
	ETuningCheck CheckI2(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext&)
	{
		return Values.Crowd_Server.NpcCount >= 2 * Values.Round_Public.TeamSize ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/**
	 * I-3: 2 * TeamSize + NpcCount >= DeckCopiesFloorCount * DeckActiveAppearancesMinCount (24 at the
	 * defaults). Reject in a public preset (Default, HostSelectable), warn in a non-selectable one.
	 */
	ETuningCheck CheckI3(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext& Context)
	{
		const int32 Floor = StealthTuningPendingOwnerKeys::DeckCopiesFloorCount * StealthTuningPendingOwnerKeys::DeckActiveAppearancesMinCount;
		if (2 * Values.Round_Public.TeamSize + Values.Crowd_Server.NpcCount >= Floor)
		{
			return ETuningCheck::Pass;
		}
		return Context.bIsPublic ? ETuningCheck::Reject : ETuningCheck::Warn;
	}

	/**
	 * I-4: 100 * BlindGuessSuccess < BlindGuessWarnPct (schema constant 50). Reject in a public preset
	 * (HostSelectable or Default), warn in a non-selectable one.
	 */
	ETuningCheck CheckI4(const FStealthServerTuning&, const FTuningDerived& Derived, const FTuningCheckContext& Context)
	{
		if (100.0 * Derived.BlindGuessSuccess < StealthTuningConstants::BlindGuessWarnPct)
		{
			return ETuningCheck::Pass;
		}
		return Context.bIsPublic ? ETuningCheck::Reject : ETuningCheck::Warn;
	}

	/** I-5: Min <= Max for every range pair of the context's schema view. Reject. */
	ETuningCheck CheckI5(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext& Context)
	{
		for (const FTuningRangePair& Pair : FindTuningRangePairs(Context.SchemaOrGenerated()))
		{
			if (Pair.Min->Get != nullptr && Pair.Max->Get != nullptr && Pair.Min->Get(Values) > Pair.Max->Get(Values))
			{
				return ETuningCheck::Reject;
			}
		}
		return ETuningCheck::Pass;
	}


	// ---- Shared Character & Locomotion rows (GDD shared-character-locomotion.md, Invariants sent to TDC, CR-15.6;
	// story SCL-003). Every SCL row rejects: each is a hard cross-field constraint a pawn rule relies on (decision logged
	// 2026-10-26: the GDD names I-SCL-4/5/16 reject and states the others as "must"). One value set for every pawn.

	/** T_stop,max = max(WalkStopSec / WalkSpeedScaleMinMult, RunStopSec): the longest stop to rest (I-SCL-12/15/16). */
	double LongestStopSec(const FLocomotionTuning_Public& L)
	{
		return FMath::Max(static_cast<double>(L.WalkStopSec) / FMath::Max(static_cast<double>(L.WalkSpeedScaleMinMult), UE_DOUBLE_SMALL_NUMBER), static_cast<double>(L.RunStopSec));
	}

	/** I-SCL-1: WalkSpeed < RunThreshold - RunExitHysteresis and RunThreshold < RunSpeed. */
	ETuningCheck CheckISCL1(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const FLocomotionTuning_Public& L = V.Locomotion_Public;
		const FLocomotionTuning_Server& S = V.Locomotion_Server;
		return L.WalkSpeedCmPerSec < S.RunSpeedThresholdCmPerSec - S.RunExitHysteresisCmPerSec && S.RunSpeedThresholdCmPerSec < L.RunSpeedCmPerSec
			? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/**
	 * I-SCL-2: Min < Vault - Tol, and the vault, hurdle and climb bands never touch: Vault + Tol < Hurdle - Tol and
	 * Hurdle + Tol < Climb - Tol (the hurdle band was added with the 1.2 m height, spike SCL-045).
	 */
	ETuningCheck CheckISCL2(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const FLocomotionTuning_Public& L = V.Locomotion_Public;
		const float Tol = L.TraversalHeightToleranceCm;
		return L.TraversalMinHeightCm < L.TraversalVaultHeightCm - Tol
			&& L.TraversalVaultHeightCm + Tol < L.TraversalHurdleHeightCm - Tol
			&& L.TraversalHurdleHeightCm + Tol < L.TraversalClimbHeightCm - Tol
			? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-3: T_B(1+eta) < T_A(1-eta) and T_C(1+eta) < T_B(1-eta), eta = AnimTierHysteresisPct / 100. */
	ETuningCheck CheckISCL3(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const FLocomotionTuning_Public& L = V.Locomotion_Public;
		const double Eta = L.AnimTierHysteresisPct / 100.0;
		return L.AnimTierBMinScreenPct * (1.0 + Eta) < L.AnimTierAMinScreenPct * (1.0 - Eta) && L.AnimTierCMinScreenPct * (1.0 + Eta) < L.AnimTierBMinScreenPct * (1.0 - Eta)
			? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-4: WalkStopSec >= 0.5 (no sudden stop from a steady walk). */
	ETuningCheck CheckISCL4(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		return V.Locomotion_Public.WalkStopSec >= 0.5f ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-5: WalkSpeedScaleMinMult <= 0.8, the minimum of the fixed pace set {0.8, 0.9, 1.0}. */
	ETuningCheck CheckISCL5(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		return V.Locomotion_Public.WalkSpeedScaleMinMult <= 0.8f ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-6: WalkTurnRate <= 0.573 * WalkSpeed * WalkSpeedScaleMinMult (turn radius >= 100 cm at the slowest pace). */
	ETuningCheck CheckISCL6(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const FLocomotionTuning_Public& L = V.Locomotion_Public;
		return L.WalkTurnRateDegPerSec <= 0.573 * L.WalkSpeedCmPerSec * L.WalkSpeedScaleMinMult ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-7: StarvationCatchUpMaxMult * WalkSpeed < RunThreshold - RunExitHysteresis (a catching-up walker never runs). */
	ETuningCheck CheckISCL7(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const FLocomotionTuning_Server& S = V.Locomotion_Server;
		return S.StarvationCatchUpMaxMult * V.Locomotion_Public.WalkSpeedCmPerSec < S.RunSpeedThresholdCmPerSec - S.RunExitHysteresisCmPerSec
			? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-8: PawnSmoothSnapThresholdCm >= 3 * RunSpeed / PawnNetUpdateHz. */
	ETuningCheck CheckISCL8(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const double Hz = FMath::Max(static_cast<double>(V.Locomotion_Server.PawnNetUpdateHz), UE_DOUBLE_SMALL_NUMBER);
		return V.Locomotion_Public.PawnSmoothSnapThresholdCm >= 3.0 * V.Locomotion_Public.RunSpeedCmPerSec / Hz ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-9: PawnSmoothLocationSec >= 1 / PawnNetUpdateHz. */
	ETuningCheck CheckISCL9(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const double Hz = FMath::Max(static_cast<double>(V.Locomotion_Server.PawnNetUpdateHz), UE_DOUBLE_SMALL_NUMBER);
		return V.Locomotion_Public.PawnSmoothLocationSec >= 1.0 / Hz ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-12: InputStarvationSec >= 0.25 and InputStarvationSec + 0.05 + T_stop,max < SessionSoftTimeoutSec. */
	ETuningCheck CheckISCL12(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const double Starvation = V.Locomotion_Server.InputStarvationSec;
		return Starvation >= 0.25 && Starvation + 0.05 + LongestStopSec(V.Locomotion_Public) < StealthTuningPendingOwnerKeys::SessionSoftTimeoutSec
			? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-13: FootstepCadenceFullPerSec >= FootstepCadenceSlowPerSec. */
	ETuningCheck CheckISCL13(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		return V.Locomotion_Public.FootstepCadenceFullPerSec >= V.Locomotion_Public.FootstepCadenceSlowPerSec ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/**
	 * I-SCL-15 (ADR-0010 section 6, the boot check): the engine movement-network values the AGameNetworkManager class default
	 * object read from Config/DefaultGame.ini must equal / satisfy the keys (StealthTuningEngineNetPins::Check). Evaluated at
	 * preset load (server boot), so a mismatch rejects the preset and fails boot.
	 */
	ETuningCheck CheckISCL15(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		return StealthTuningEngineNetPins::Check(StealthTuningEngineNetPins::Read(*GetDefault<AGameNetworkManager>()), V) ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-SCL-16: StopMinSec <= min(WalkStopSec, RunStopSec) (the floor never lengthens a stop from a steady pace). */
	ETuningCheck CheckISCL16(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const FLocomotionTuning_Public& L = V.Locomotion_Public;
		return L.StopMinSec <= FMath::Min(L.WalkStopSec, L.RunStopSec) ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	// ---- Crowd & NPC Behaviour rows (TDC GDD Formulas I-CNB-1 to I-CNB-6, all reject; story cnb-003). The LSC keys
	// I-CNB-3/4 read are pending owner keys (StealthTuningPendingOwnerKeys) until the Level Socket Contract registers them.

	/** I-CNB-1: CrowdIdleMinSec >= SessionSoftTimeoutSec + 1 (a long pause outlasts a soft stall). */
	ETuningCheck CheckICNB1(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		return V.Crowd_Server.CrowdIdleMinSec >= StealthTuningPendingOwnerKeys::SessionSoftTimeoutSec + 1.0 ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-CNB-2: CrowdMaxIdleSec >= max(CrowdIdleMaxSec, CrowdConversationMaxSec). */
	ETuningCheck CheckICNB2(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const FCrowdTuning_Server& C = V.Crowd_Server;
		return C.CrowdMaxIdleSec >= FMath::Max(C.CrowdIdleMaxSec, C.CrowdConversationMaxSec) ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-CNB-3: (60 / CrowdAmbientCycleIntervalSec) * (1 - EmergenceExitCancelMaxPct / 100) >= AmbientEmergencePerSocketMinPerMin. */
	ETuningCheck CheckICNB3(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const double PerSocketPerMin = 60.0 / FMath::Max(static_cast<double>(V.Crowd_Server.CrowdAmbientCycleIntervalSec), UE_DOUBLE_SMALL_NUMBER);
		const double AfterCancels = PerSocketPerMin * (1.0 - StealthTuningPendingOwnerKeys::EmergenceExitCancelMaxPct / 100.0);
		return AfterCancels >= StealthTuningPendingOwnerKeys::AmbientEmergencePerSocketMinPerMin ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-CNB-4: 2 * 60 / CrowdAmbientCycleIntervalSec <= 60 / EmergenceSocketCooldownSec (exit + entry fit the cooldown). */
	ETuningCheck CheckICNB4(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const double Events = 2.0 * 60.0 / FMath::Max(static_cast<double>(V.Crowd_Server.CrowdAmbientCycleIntervalSec), UE_DOUBLE_SMALL_NUMBER);
		return Events <= 60.0 / StealthTuningPendingOwnerKeys::EmergenceSocketCooldownSec ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/**
	 * I-CNB-5: F2 chain-gap compensation validity, (Lambda - r_N,j) * CrowdTellChainGapSec / 60 <= 0.5 for every tell j of
	 * T1-T5, T12, T13 (r_N,j = 60 / interval per minute, Lambda their sum).
	 */
	ETuningCheck CheckICNB5(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const FCrowdTuning_Server& C = V.Crowd_Server;
		const float Intervals[] = { C.CrowdRunIntervalSec, C.CrowdVaultIntervalSec, C.CrowdClimbIntervalSec, C.CrowdBraceIntervalSec,
			C.CrowdDecoyTendIntervalSec, C.CrowdBushIntervalSec, C.CrowdReversalIntervalSec, C.CrowdHeadHoldIntervalSec };
		double Lambda = 0.0;
		double MinRate = TNumericLimits<double>::Max();
		for (const float Interval : Intervals)
		{
			const double Rate = 60.0 / FMath::Max(static_cast<double>(Interval), UE_DOUBLE_SMALL_NUMBER);
			Lambda += Rate;
			MinRate = FMath::Min(MinRate, Rate);
		}
		// The worst tell is the rarest one (largest Lambda - r_N,j).
		return (Lambda - MinRate) * static_cast<double>(C.CrowdTellChainGapSec) / 60.0 <= 0.5 ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-CNB-6: CrowdDecoyEarlyReleaseMaxSec < ObjectiveInteractSec (an early release never lasts as long as a real tend). */
	ETuningCheck CheckICNB6(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		return V.Crowd_Server.CrowdDecoyEarlyReleaseMaxSec < V.Objectives_Public.ObjectiveInteractSec ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** Offline rows never run at preset load (the harness, R3 or the clip authoring check enforces them). */
	ETuningCheck CheckOffline(const FStealthServerTuning&, const FTuningDerived&, const FTuningCheckContext&)
	{
		return ETuningCheck::Pass;
	}

	/** The preset-load rows (none offline), TDC rows first. Owner epics append their rows here. */
	const FTuningInvariant LoadRows[] = {
		{ "I-1", "TDC", &CheckI1 },
		{ "I-2", "TDC", &CheckI2 },
		{ "I-3", "TDC", &CheckI3 },
		{ "I-4", "TDC", &CheckI4 },
		{ "I-5", "TDC", &CheckI5 },
		{ "I-SCL-1", "SCL", &CheckISCL1 },
		{ "I-SCL-2", "SCL", &CheckISCL2 },
		{ "I-SCL-3", "SCL", &CheckISCL3 },
		{ "I-SCL-4", "SCL", &CheckISCL4 },
		{ "I-SCL-5", "SCL", &CheckISCL5 },
		{ "I-SCL-6", "SCL", &CheckISCL6 },
		{ "I-SCL-7", "SCL", &CheckISCL7 },
		{ "I-SCL-8", "SCL", &CheckISCL8 },
		{ "I-SCL-9", "SCL", &CheckISCL9 },
		{ "I-SCL-12", "SCL", &CheckISCL12 },
		{ "I-SCL-13", "SCL", &CheckISCL13 },
		{ "I-SCL-15", "SCL", &CheckISCL15 },
		{ "I-SCL-16", "SCL", &CheckISCL16 },
		{ "I-CNB-1", "Crowd", &CheckICNB1 },
		{ "I-CNB-2", "Crowd", &CheckICNB2 },
		{ "I-CNB-3", "Crowd", &CheckICNB3 },
		{ "I-CNB-4", "Crowd", &CheckICNB4 },
		{ "I-CNB-5", "Crowd", &CheckICNB5 },
		{ "I-CNB-6", "Crowd", &CheckICNB6 },
		// Assassination & Defence Reversal rows (story asn-001; checks in AssassinationInvariants.cpp).
		{ "I-ASN-1", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckIAsn1> },
		{ "I-ASN-2", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckIAsn2> },
		{ "I-ASN-3", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckIAsn3> },
		{ "I-ASN-4", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckIAsn4> },
		{ "I-ASN-5", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckIAsn5> },
		{ "I-ASN-5w", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckIAsn5w> },
		{ "I-ASN-6", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckIAsn6> },
		{ "I-ASN-7", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckIAsn7> },
		{ "I-ASN-8", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckIAsn8> },
		{ "W-ASN-9", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckWAsn9> },
		{ "W-ASN-10", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckWAsn10> },
		{ "W-ASN-11", "Assassination", &StealthAssassinationInvariants::TableCheck<&StealthAssassinationInvariants::CheckWAsn11> },
		// Penalty CR-11 rows (story nkp-020; checks in PenaltyInvariants.cpp).
		{ "I-NKP-1", "Penalty", &StealthPenaltyInvariants::TableCheck<&StealthPenaltyInvariants::CheckINkp1> },
		{ "W-NKP-2", "Penalty", &StealthPenaltyInvariants::TableCheck<&StealthPenaltyInvariants::CheckWNkp2> },
		{ "W-NKP-3", "Penalty", &StealthPenaltyInvariants::TableCheck<&StealthPenaltyInvariants::CheckWNkp3> },
		{ "W-NKP-4", "Penalty", &StealthPenaltyInvariants::TableCheck<&StealthPenaltyInvariants::CheckWNkp4> },
		{ "W-NKP-5", "Penalty", &StealthPenaltyInvariants::TableCheck<&StealthPenaltyInvariants::CheckWNkp5> },
		// Objectives Chain rows (story oc-001; checks in ObjectivesInvariants.cpp). I-OC2 is a mixed row.
		{ "I-OC1", "Objectives", &StealthObjectivesInvariants::TableCheck<&StealthObjectivesInvariants::CheckIOc1> },
		{ "I-OC2", "Objectives", &StealthObjectivesInvariants::TableCheck<&StealthObjectivesInvariants::CheckIOc2>,
			StealthObjectivesInvariants::IOc2RejectClause, StealthObjectivesInvariants::IOc2WarnClause },
		{ "I-OC3", "Objectives", &StealthObjectivesInvariants::TableCheck<&StealthObjectivesInvariants::CheckIOc3> },
		// Concealment F3 timing chain (story con-001; ConcealmentInvariants.cpp): one Reject row per constraint.
		{ "I-CON-F3a", "Concealment", &StealthConcealmentInvariants::TableCheck<StealthConcealmentInvariants::F3a> },
		{ "I-CON-F3b", "Concealment", &StealthConcealmentInvariants::TableCheck<StealthConcealmentInvariants::F3b> },
		{ "I-CON-F3c", "Concealment", &StealthConcealmentInvariants::TableCheck<StealthConcealmentInvariants::F3c> },
		{ "I-CON-F3d", "Concealment", &StealthConcealmentInvariants::TableCheck<StealthConcealmentInvariants::F3d> },
		{ "I-CON-F3e", "Concealment", &StealthConcealmentInvariants::TableCheck<StealthConcealmentInvariants::F3e> },
		{ "I-CON-F3f", "Concealment", &StealthConcealmentInvariants::TableCheck<StealthConcealmentInvariants::F3f> },
	};

	/**
	 * Offline rows (listed so the registration is visible; never evaluated at preset load): I-SCL-10 the listen host's p99
	 * frame time (harness frame-time capture), I-SCL-11 the F8 animation budget with the measured per-tier cost (spike R3),
	 * I-SCL-14 TraversalReachCm <= 1.1 x the authored clip approach length (traversal clip authoring check).
	 */
	const FTuningInvariant OfflineRows[] = {
		{ "I-SCL-10", "SCL", &CheckOffline, nullptr, nullptr, true },
		{ "I-SCL-11", "SCL", &CheckOffline, nullptr, nullptr, true },
		{ "I-SCL-14", "SCL", &CheckOffline, nullptr, nullptr, true },
	};

	/** True when Key ends with Tail (byte-wise). */
	bool EndsWith(FAnsiStringView Key, FAnsiStringView Tail)
	{
		return Key.Len() >= Tail.Len() && Key.Right(Tail.Len()).Equals(Tail, ESearchCase::CaseSensitive);
	}
}

TConstArrayView<FTuningInvariant> GetTuningInvariants()
{
	return MakeArrayView(LoadRows);
}

TConstArrayView<FTuningInvariant> GetTuningOfflineInvariants()
{
	// The SCL offline rows (story SCL-003); O-CNB-6, O-CNB-7 and the LSC layout predicate arrive with their owner epics
	// and are enforced by those owners' CI or load-time checks.
	return MakeArrayView(OfflineRows);
}

TArray<FTuningRangePair> FindTuningRangePairs(TConstArrayView<FTuningSchemaRow> Schema)
{
	TArray<FTuningRangePair> Pairs;
	for (const FTuningSchemaRow& MinRow : Schema)
	{
		if (MinRow.Key == nullptr || MinRow.Unit == nullptr)
		{
			continue;
		}
		const FAnsiStringView Key(MinRow.Key);
		const FAnsiStringView Unit(MinRow.Unit);
		TAnsiStringBuilder<64> MinTail;
		MinTail << "Min" << Unit;
		if (!EndsWith(Key, MinTail.ToView()) || Key.Len() == MinTail.Len())
		{
			continue;
		}
		TAnsiStringBuilder<256> MaxKey;
		MaxKey << Key.LeftChop(MinTail.Len()) << "Max" << Unit;
		for (const FTuningSchemaRow& MaxRow : Schema)
		{
			if (MaxRow.Key != nullptr && MaxRow.Unit != nullptr && FAnsiStringView(MaxRow.Key).Equals(MaxKey.ToView(), ESearchCase::CaseSensitive) && FAnsiStringView(MaxRow.Unit).Equals(Unit, ESearchCase::CaseSensitive))
			{
				Pairs.Add({ &MinRow, &MaxRow });
				break;
			}
		}
	}
	return Pairs;
}

StealthTuningEngineNetPins::FValues StealthTuningEngineNetPins::Read(const AGameNetworkManager& Manager)
{
	FValues Out;
	Out.MaxClientUpdateInterval = Manager.MAXCLIENTUPDATEINTERVAL;
	Out.MaxClientForcedUpdateDuration = Manager.MaxClientForcedUpdateDuration;
	Out.MaxMoveDeltaTime = Manager.MaxMoveDeltaTime;
	Out.bMovementTimeDiscrepancyDetection = Manager.bMovementTimeDiscrepancyDetection;
	Out.bMovementTimeDiscrepancyResolution = Manager.bMovementTimeDiscrepancyResolution;
	return Out;
}

bool StealthTuningEngineNetPins::Check(const FValues& Engine, const FStealthServerTuning& Values)
{
	const double Starvation = Values.Locomotion_Server.InputStarvationSec;
	const double MaxInterval = Engine.MaxClientUpdateInterval;
	const double LongestStop = FMath::Max(static_cast<double>(Values.Locomotion_Public.WalkStopSec) / FMath::Max(static_cast<double>(Values.Locomotion_Public.WalkSpeedScaleMinMult), UE_DOUBLE_SMALL_NUMBER),
		static_cast<double>(Values.Locomotion_Public.RunStopSec));
	return FMath::IsNearlyEqual(MaxInterval, Starvation, 1e-4)
		&& Engine.MaxMoveDeltaTime <= MaxInterval + 1e-4
		&& Engine.MaxClientForcedUpdateDuration + 1e-4 >= Starvation + LongestStop + 0.25
		&& Engine.bMovementTimeDiscrepancyDetection
		&& Engine.bMovementTimeDiscrepancyResolution;
}
