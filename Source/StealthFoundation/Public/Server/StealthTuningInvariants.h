#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

struct FStealthServerTuning;
struct FTuningDerived;
class AGameNetworkManager;

/**
 * The cross-field tuning invariants (GDD Formulas "Invariants", CR-6 c; ADR-0004 section 3; stories
 * tdc-048 and tdc-055). Every invariant, TDC-owned (I-1 to I-5) or owner-registered (CR-15.6), is one
 * row of one table in Private/Tuning/StealthTuningInvariants.cpp. A row's Check is a pure function over
 * the clamped (post F-1) const value set, the F-3 indicators and a context; it returns Pass, Warn or
 * Reject and never writes a value (only F-1 changes a value, ADR-0004 guideline 5). A Reject anywhere
 * rejects the whole preset; a Warn logs one warning naming the row's Id and keeps the preset.
 */

/** Result of one invariant row. Ordered by severity: the worst result across rows wins. */
enum class ETuningCheck : uint8
{
	Pass,
	Warn,
	Reject,
};

/** Name of the non-selectable bot-test preset whose rows may be scoped out (W-RMF-4 pattern). */
inline FName TuningSoloPresetId() { return FName(TEXT("Solo")); }

/**
 * What a Check may know besides the values (ADR-0004 Key Interfaces).
 * bIsPublic is true for Default, any HostSelectable preset and the schema-defaults rung (the strict
 * scope of I-3, I-4, I-NKP-1). Schema is the schema view the preset was validated against (the
 * resolve input's substitutable view, story tdc-050); I-5 recognises range pairs from it. An empty
 * view means the generated schema (GetTuningSchema()).
 */
struct FTuningCheckContext
{
	bool bIsPublic = false;
	FName PresetId;
	TConstArrayView<FTuningSchemaRow> Schema;

	/** Schema, or the generated table when Schema is empty. */
	TConstArrayView<FTuningSchemaRow> SchemaOrGenerated() const { return Schema.Num() > 0 ? Schema : GetTuningSchema(); }
};

/** The pure check signature: const value set, const indicators, const context; no output parameter. */
using FTuningInvariantCheck = ETuningCheck (*)(const FStealthServerTuning&, const FTuningDerived&, const FTuningCheckContext&);

/**
 * One invariant row (ADR-0004 Key Interfaces { Id, Owner, Check }, plus three optional trailing
 * fields that a three-field row leaves at their defaults):
 *   RejectClause / WarnClause  a mixed row (a reject clause and a warn clause, for example I-OC2 or
 *                              I-HUD-4) names each clause; the report names the clause that produced
 *                              the row's result (story tdc-055). Single-clause rows leave both null.
 *   bOffline                   a constraint that needs a measured or layout-time quantity (O-CNB-6,
 *                              I-SCL-10, the LSC layout predicate). Offline rows are never evaluated
 *                              at preset load: GetTuningInvariants() leaves them out and
 *                              FTuningPipelineCore::EvaluateInvariants skips them in an injected table.
 */
struct FTuningInvariant
{
	/** The exact GDD ID ("I-1", "I-ASN-4", "W-NKP-3"); the report and the warning name it (AC-30). */
	const ANSICHAR* Id = nullptr;
	/** Owner system id (the GDD that defines the constraint's meaning). */
	const ANSICHAR* Owner = nullptr;
	/** The pure check. */
	FTuningInvariantCheck Check = nullptr;
	/** Mixed rows only: the reject clause's text. */
	const ANSICHAR* RejectClause = nullptr;
	/** Mixed rows only: the warn clause's text. */
	const ANSICHAR* WarnClause = nullptr;
	/** True for an offline row (enforced by its owner's CI or load-time check, never at preset load). */
	bool bOffline = false;
};

/** The preset-load invariant table: every non-offline row (I-1 to I-5, then owner rows as they register). */
STEALTHFOUNDATION_API TConstArrayView<FTuningInvariant> GetTuningInvariants();

/** The offline rows, listed so the registration is visible; never evaluated at preset load. Empty today. */
STEALTHFOUNDATION_API TConstArrayView<FTuningInvariant> GetTuningOfflineInvariants();

/**
 * The deck-floor factors of I-3 (Character Pool CR-5). They are Character Pool owner keys
 * (DeckCopiesFloorCount, DeckActiveAppearancesMinCount) that the schema does not hold yet; until the
 * Character Pool epic registers them as schema rows, I-3 reads these provisional values, equal to the
 * keys' registered defaults (4 and 6, so the floor is 24). When the keys land, I-3 reads them from the
 * value set and these constants are deleted (tech debt TD-TDC-I3).
 */
namespace StealthTuningPendingOwnerKeys
{
	inline constexpr int32 DeckCopiesFloorCount = 4;
	inline constexpr int32 DeckActiveAppearancesMinCount = 6;
	/**
	 * Session's SessionSoftTimeoutSec (Session & Travel owner key, default 3), read by I-SCL-12 until the Session epic
	 * registers it as a schema row (story SCL-003; the TD-TDC-I3 pattern).
	 */
	inline constexpr double SessionSoftTimeoutSec = 3.0;
}

/**
 * I-SCL-15 (ADR-0010 section 6; story SCL-003): the engine's movement-network values are read from the
 * AGameNetworkManager class default object (GlobalConfig, Config/DefaultGame.ini), process-wide and before any tuning
 * applies, so they are pinned in config and checked against the SCL keys. True when MAXCLIENTUPDATEINTERVAL equals
 * InputStarvationSec, MaxMoveDeltaTime <= MAXCLIENTUPDATEINTERVAL, MaxClientForcedUpdateDuration >=
 * InputStarvationSec + T_stop,max + 0.25 s (T_stop,max = max(WalkStopSec / WalkSpeedScaleMinMult, RunStopSec)) and both
 * movement-time discrepancy flags are on. The I-SCL-15 invariant row calls it on the CDO at preset load (boot).
 */
namespace StealthTuningEngineNetPins
{
	/** The AGameNetworkManager values I-SCL-15 compares (plain data, so a test can drift one without an actor). */
	struct FValues
	{
		float MaxClientUpdateInterval = 0.0f;
		float MaxClientForcedUpdateDuration = 0.0f;
		float MaxMoveDeltaTime = 0.0f;
		bool bMovementTimeDiscrepancyDetection = false;
		bool bMovementTimeDiscrepancyResolution = false;
	};

	/** Reads the compared values from a manager (normally the class default object). */
	STEALTHFOUNDATION_API FValues Read(const AGameNetworkManager& Manager);

	/** The I-SCL-15 predicate over Engine and the tuning value set. */
	STEALTHFOUNDATION_API bool Check(const FValues& Engine, const FStealthServerTuning& Values);
}

/**
 * I-5 range-pair recognition (GDD I-5, "...MinSec/...MaxSec and similar"): two schema rows form a
 * range pair when their keys are <Prefix>Min<Unit> and <Prefix>Max<Unit> with the same non-empty
 * Prefix and the rows' own Unit (the CR-15.2 suffix). A one-sided bound such as CrowdMaxIdleSec, or a
 * Min key with no Max partner (FinalCandidatesMinCount), is not a pair. No pair exists among the eight
 * canonical keys yet.
 */
struct FTuningRangePair
{
	const FTuningSchemaRow* Min = nullptr;
	const FTuningSchemaRow* Max = nullptr;
};

/** Every range pair of Schema, in schema order of the Min row. */
STEALTHFOUNDATION_API TArray<FTuningRangePair> FindTuningRangePairs(TConstArrayView<FTuningSchemaRow> Schema);
