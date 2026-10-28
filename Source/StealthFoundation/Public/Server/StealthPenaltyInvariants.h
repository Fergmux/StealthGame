#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningInvariants.h"

struct FStealthServerTuning;

/**
 * The NPC-Kill Penalty & Informants cross-field rows (GDD npc-kill-penalty-informants.md Tuning Knobs and "Penalty CR-11";
 * TDC GDD owner-invariant rows I-NKP-1 and W-NKP-2 to W-NKP-5; ADR-0004 section 3; story nkp-020). Every check is a pure
 * function over the clamped (post F-1) value set, the externally owned inputs and the preset context; it returns Pass,
 * Warn or Reject and never writes a value (CR-6). The rows are registered in the one TDC table
 * (Private/Tuning/StealthTuningInvariants.cpp) through an adapter that reads the inputs with
 * FPenaltyInvariantInputs::FromValues. Validation runs at preset load only, never per frame.
 *
 *   I-NKP-1  NpcKillStrikeLimit != 1: Reject in a public preset (Default, HostSelectable, the schema-defaults rung),
 *            Warn in a non-selectable test preset (only reachable with bTestAllowsStrikeLimitOne)
 *   W-NKP-2  InformantRespawnDelaySec + 2 * RespawnStarvationSec <= 40 (F3 starved black-screen budget)
 *   W-NKP-3  StrikeOutReleaseGraceSec >= AttackAttemptLockoutSec
 *   W-NKP-4  1.2 + DeathFallSec + NoPawnFadeSec <= InformantRespawnDelaySec (F2 bush cap; a guard that cannot fire
 *            inside the hard ranges: 4.7 <= 5, so tests call FloorGuard directly with an injected out-of-range D)
 *   W-NKP-5  ExileCardTimeoutSec >= StrikeContactSec + BushStrikeEmergeSec + ExileBeatSec + 0.3
 *
 * Comparisons use CompareToleranceAbs (the keys are floats authored in decimals; see StealthAssassinationInvariants.h).
 *
 * Usage (test):
 *   FPenaltyInvariantInputs Inputs = FPenaltyInvariantInputs::FromValues(Values);
 *   Inputs.NoPawnFadeSec = 1.5;
 *   const FPenaltyInvariantReport Report = StealthPenaltyInvariants::Evaluate(Values, Inputs, Context);
 *   // StealthPenaltyInvariants::FloorGuard(2.0, 1.5, 4.0) == ETuningCheck::Warn
 */

/** The values the Penalty rows read that Penalty does not own and the schema does not hold yet. */
struct FPenaltyInvariantInputs
{
	/** The Level Socket Contract's RespawnStarvationSec (pending owner key, 10; W-NKP-2). */
	double RespawnStarvationSec = 0.0;
	/** Camera's NoPawnFadeSec (pending owner key, 0.5; W-NKP-4). */
	double NoPawnFadeSec = 0.0;
	/** Concealment's bush-strike emergence time (pending owner key, 0.35; W-NKP-5). */
	double BushStrikeEmergeSec = 0.0;

	/** The inputs a preset load sees: the pending owner keys' registered defaults. */
	STEALTHFOUNDATION_API static FPenaltyInvariantInputs FromValues(const FStealthServerTuning& Values);
};

/** The findings of one evaluation: the Ids of the rows that rejected and, separately, the rows that warned. */
struct FPenaltyInvariantReport
{
	TArray<FString> Rejected;
	TArray<FString> Warned;
};

namespace StealthPenaltyInvariants
{
	/** Absolute tolerance of every Penalty comparison (seconds; far below authored steps). */
	inline constexpr double CompareToleranceAbs = 1e-4;
	/** W-NKP-2: the starved black-screen budget in seconds (F3 p95). */
	inline constexpr double StarvedBlackScreenBudgetSec = 40.0;
	/** W-NKP-4: the F2 bush-strike cap on the strike-to-death time in seconds (I-ASN-3). */
	inline constexpr double BushStrikeCapSec = 1.2;
	/** W-NKP-5: the normal RPC delay allowance added to the latest card entrance in seconds. */
	inline constexpr double CardRpcAllowanceSec = 0.3;
	/** W-NKP-2: the number of RespawnStarvationSec widenings a starved respawn waits through (LSC R5b.5). */
	inline constexpr double StarvationWideningCount = 2.0;

	/** One Penalty check over the value set, the externally owned inputs and the preset context. */
	using FCheck = ETuningCheck (*)(const FStealthServerTuning&, const FPenaltyInvariantInputs&, const FTuningCheckContext&);

	/** One row: the exact GDD Id and its check. */
	struct FRow
	{
		const ANSICHAR* Id = nullptr;
		FCheck Check = nullptr;
	};

	STEALTHFOUNDATION_API ETuningCheck CheckINkp1(const FStealthServerTuning& V, const FPenaltyInvariantInputs& In, const FTuningCheckContext& Context);
	STEALTHFOUNDATION_API ETuningCheck CheckWNkp2(const FStealthServerTuning& V, const FPenaltyInvariantInputs& In, const FTuningCheckContext& Context);
	STEALTHFOUNDATION_API ETuningCheck CheckWNkp3(const FStealthServerTuning& V, const FPenaltyInvariantInputs& In, const FTuningCheckContext& Context);
	STEALTHFOUNDATION_API ETuningCheck CheckWNkp4(const FStealthServerTuning& V, const FPenaltyInvariantInputs& In, const FTuningCheckContext& Context);
	STEALTHFOUNDATION_API ETuningCheck CheckWNkp5(const FStealthServerTuning& V, const FPenaltyInvariantInputs& In, const FTuningCheckContext& Context);

	/**
	 * The W-NKP-4 predicate over plain numbers: Pass when 1.2 + DeathFallSec + NoPawnFadeSec <= InformantRespawnDelaySec, Warn
	 * otherwise. The clamped hard ranges keep the sum at most 4.7 against a delay of at least 5, so a preset can never
	 * reach the Warn branch; a test calls this directly with an injected out-of-range delay (D = 4.0).
	 */
	STEALTHFOUNDATION_API ETuningCheck FloorGuard(double DeathFallSec, double NoPawnFadeSec, double InformantRespawnDelaySec);

	/** Every Penalty row in GDD order (the order the TDC table registers them). */
	STEALTHFOUNDATION_API TConstArrayView<FRow> GetRows();

	/** Runs every row over Values with Inputs and Context. Pure. */
	STEALTHFOUNDATION_API FPenaltyInvariantReport Evaluate(const FStealthServerTuning& Values, const FPenaltyInvariantInputs& Inputs, const FTuningCheckContext& Context);

	/** The TDC table adapter: runs Check with FPenaltyInvariantInputs::FromValues(Values). */
	template <FCheck Check>
	ETuningCheck TableCheck(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext& Context)
	{
		return Check(Values, FPenaltyInvariantInputs::FromValues(Values), Context);
	}
}
