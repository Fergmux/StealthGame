#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningSchema.h"

class FValidatedTuning;
struct FTuningDerived;

/**
 * The plain-C++ tuning pipeline core (ADR-0004 section 3). Story tdc-046 introduced the F-1 stage,
 * tdc-049 the safe-range warning, tdc-048/055 the invariant evaluation, tdc-050/051/053 Resolve() and
 * the fallback ladder. No UWorld, no asset access: headless-testable.
 */

/** What F-1 did to one key. */
enum class ETuningF1Action : uint8
{
	/** Value kept (inside the hard range, exactly on a bound, a declared sentinel, or a bool). */
	Kept,
	/** Clamped to [HardMin, HardMax]. */
	Clamped,
	/** A non-finite float (NaN, +/-Infinity) took the schema default. */
	DefaultedNonFinite,
	/** An enum value outside its declared set took the schema default (no enum key exists yet). */
	DefaultedUndeclaredEnum,
};

/** One key F-1 changed (Kept keys are not recorded). */
struct FTuningF1Change
{
	/** The key (points into the schema row, which outlives the report). */
	const ANSICHAR* Key = nullptr;
	double Authored = 0.0;
	double Effective = 0.0;
	ETuningF1Action Action = ETuningF1Action::Kept;
};

/** One key kept inside its hard range but outside its safe range (CR-15.5, AC-26): a warning, never a clamp. */
struct FTuningSafeRangeNote
{
	/** The key (points into the schema row). */
	const ANSICHAR* Key = nullptr;
	double Value = 0.0;
	double SafeMin = 0.0;
	double SafeMax = 0.0;
};

/**
 * Inputs of F-1 that are not values. bTestAllowsStrikeLimitOne is the catalog flag of the preset being
 * validated (UStealthTuningCatalog entry; Resolve() asks the preset source for it per rung,
 * ITuningPresetSource::GetF1Options): it lowers NpcKillStrikeLimit's hard minimum from 2 to 1 for that
 * preset only. The catalog check keeps it off Default and HostSelectable presets; Shipping ignores it.
 */
struct FTuningF1Options
{
	bool bTestAllowsStrikeLimitOne = false;
};

/** The F-1 part of the validation report. */
struct FTuningF1Report
{
	/** Keys F-1 changed (clamps, defaults applied), in schema order. */
	TArray<FTuningF1Change> Changes;
	/** Keys kept outside their safe range (warning only), in schema order. */
	TArray<FTuningSafeRangeNote> SafeRange;
};

/** The key whose hard minimum the catalog flag lowers (GDD F-1, user decision 2026-10-25). */
inline constexpr const ANSICHAR* TuningStrikeLimitKey = "NpcKillStrikeLimit";

/** NpcKillStrikeLimit's hard minimum in a bTestAllowsStrikeLimitOne preset (GDD F-1; not a preset value). */
inline constexpr double TuningStrikeLimitTestHardMin = 1.0;

/** The rung that produced the active set (ADR-0004 Key Interfaces). */
enum class ETuningFallback : uint8
{
	Requested = 0,
	Default = 1,
	SchemaDefaults = 2,
};

/** Why the pipeline failed (ADR-0004 Key Interfaces, FOnTuningFailed). */
enum class ETuningFailReason : uint8
{
	/** Every rung of the fallback ladder failed (CR-6; logged as TuningLoadFailed). */
	LoadFailed,
	/** The server self-check found a snapshot that does not hash to the validated set (CR-9, later story). */
	SnapshotFault,
};

/** What happened to one rung of the ladder. */
enum class ETuningRungOutcome : uint8
{
	/** The rung's preset passed every invariant (warnings allowed) and is active. */
	Accepted,
	/** The preset loaded but at least one invariant returned Reject: rejected as a whole. */
	Rejected,
	/** The source produced no value set for the preset (missing or corrupted, see ITuningPresetSource). */
	Missing,
};

/** One Warn or Reject row result (Pass rows are not recorded). */
struct FTuningInvariantFinding
{
	/** The row's exact GDD ID. */
	const ANSICHAR* Id = nullptr;
	/** Mixed rows: the clause that produced the result (RejectClause or WarnClause); null otherwise. */
	const ANSICHAR* Clause = nullptr;
	ETuningCheck Result = ETuningCheck::Pass;
};

/** The validation record of one rung. */
struct FTuningRungReport
{
	/** The preset tried (SchemaDefaults for the schema-defaults rung). */
	FName PresetId;
	ETuningFallback Level = ETuningFallback::Requested;
	ETuningRungOutcome Outcome = ETuningRungOutcome::Missing;
	/** F-1 clamps, defaults applied and safe-range notes (empty for a Missing rung). */
	FTuningF1Report F1;
	/** Warn and Reject invariant results, in table order (empty for a Missing rung). */
	TArray<FTuningInvariantFinding> Findings;
};

/**
 * The ValidationReport (CR-6, ADR-0004 section 3): every rung tried, in ladder order, with its clamps,
 * defaults applied, invariant failures by Id and warnings. Round copies it into the Event Log header (CR-7).
 */
struct FValidationReport
{
	TArray<FTuningRungReport> Rungs;

	/** The accepted rung, or nullptr when the pipeline failed. */
	STEALTHFOUNDATION_API const FTuningRungReport* AcceptedRung() const;

	/** True when any rung recorded a finding with this Id and result. */
	STEALTHFOUNDATION_API bool HasFinding(FAnsiStringView Id, ETuningCheck Result) const;
};

/**
 * The injectable preset loader (ADR-0004 section 3; the AC-6b/6c test seam). Production: the store's
 * preloaded catalog (story tdc-058). Tests: a local class that can return a missing or corrupted preset.
 *
 * "Corrupted" (defined here once; stories tdc-051 and tdc-053 use this definition): the source cannot
 * produce a value set for the preset, so Load returns no value. That covers a missing catalog entry, a
 * missing or unreadable asset, an asset of the wrong class and any other load or validity failure of
 * the asset itself. A preset whose values are out of range is NOT corrupted: F-1 clamps it. A preset
 * that loads and then fails an invariant is rejected, not corrupted; both take the next rung.
 */
class ITuningPresetSource
{
public:
	virtual ~ITuningPresetSource() = default;

	/**
	 * The preset's values copied out of its asset, or no value when it is missing or corrupted.
	 * bOutIsPublic: true for Default and any HostSelectable preset (the strict invariant scope).
	 */
	virtual TOptional<FStealthServerTuning> Load(FName PresetId, bool& bOutIsPublic) const = 0;

	/** The preset's F-1 options (its catalog entry's bTestAllowsStrikeLimitOne). Default: none. */
	virtual FTuningF1Options GetF1Options(FName PresetId) const { return FTuningF1Options(); }
};

/** Input of Resolve() (ADR-0004 Key Interfaces plus the story tdc-050 schema seam). */
struct FTuningResolveInput
{
	/** The stored preset name (Default when absent). */
	FName RequestedPreset;
	/** The preset loader; required. */
	const ITuningPresetSource* Source = nullptr;
	/** Session-latched keys frozen at session open (CR-11); copied over every rung's values. Null: none. */
	const FStealthServerTuning* SessionFrozen = nullptr;
#if WITH_STEALTH_TEST_SEAMS
	/**
	 * Test seam (story tdc-050 amendment; used by tdc-053): a substituted schema view. Empty means the
	 * generated table. Every row needs its Get/Set accessors. Compiled out of Shipping.
	 */
	TConstArrayView<FTuningSchemaRow> Schema;
	/** Test seam (story tdc-055): a substituted invariant table. Empty means GetTuningInvariants(). */
	TConstArrayView<FTuningInvariant> Invariants;
#endif
};

/** Output of Resolve() (ADR-0004 Key Interfaces). Failed is a null Validated (no separate flag to disagree with it). */
struct FTuningResolveResult
{
	/** The accepted rung's level; meaningless when failed. */
	ETuningFallback FallbackLevel = ETuningFallback::Requested;
	/** The accepted rung's preset (SchemaDefaults for level 2); NAME_None when failed. */
	FName PresetId;
	FValidationReport Report;
	/** The frozen set, or null when every rung failed. */
	TSharedPtr<const FValidatedTuning> Validated;

	/** True when every rung failed (the pipeline's Failed state; Round returns to Lobby). */
	bool IsFailed() const { return !Validated.IsValid(); }
};

class STEALTHFOUNDATION_API FTuningPipelineCore
{
public:
	/** PresetId of the schema-defaults rung (CR-6). */
	static FName SchemaDefaultsPresetId();

	/** PresetId of the mandatory Default preset (CR-2). */
	static FName DefaultPresetId();

	/**
	 * GDD F-1 for one key, in this order: (1) a non-finite float takes Row.Default (tested before
	 * clamping: clamp of NaN is NaN, and +/-Infinity take the default, not a bound); (2) a value equal
	 * to the row's declared sentinel is kept; (3) otherwise the value is clamped to [HardMin, HardMax]
	 * (a value exactly on a bound is kept); bools pass unchanged. Pure: no log, no state.
	 */
	static double ApplyF1(const FTuningSchemaRow& Row, double Authored, const FTuningF1Options& Options, ETuningF1Action& OutAction);

	/**
	 * True when Value, which F-1 kept, lies outside [SafeMin, SafeMax] (CR-15.5). Bools, a declared
	 * sentinel and a value exactly on a safe bound are never outside. Pure.
	 */
	static bool IsOutsideSafeRange(const FTuningSchemaRow& Row, double Value);

	/**
	 * Runs F-1 over every row of Schema on Values in place (through each row's Get/Set), appends one
	 * FTuningF1Change per changed key to OutReport, and logs one LogStealthFoundation warning per
	 * changed key naming the key, its authored and its effective value. In the same pass a key F-1 kept
	 * but that lies outside its safe range logs one safe-range warning naming the key and its value and
	 * is recorded in OutReport.SafeRange; its value is never changed (story tdc-049). A key F-1 changed
	 * gets only the clamp warning, never a second safe-range warning for its effective value (exactly
	 * one warning per key). F-1 is the only code that changes a tuning value, and only to the key's own
	 * hard range (ADR-0004 guideline 5). Invariants are never evaluated here.
	 */
	static void RunF1(TConstArrayView<FTuningSchemaRow> Schema, FStealthServerTuning& Values, const FTuningF1Options& Options, FTuningF1Report& OutReport);

	/** Logs the F-1 warning for one change (also used by RunF1); the value text is FormatTuningValue. */
	static void LogF1Change(const FTuningSchemaRow& Row, const FTuningF1Change& Change);

	/** Logs the safe-range warning for one note (also used by RunF1). */
	static void LogSafeRange(const FTuningSchemaRow& Row, const FTuningSafeRangeNote& Note);

	/**
	 * Evaluates every non-offline row of Rows on the clamped const set and returns the worst result
	 * (Pass < Warn < Reject). Each Warn or Reject row appends one finding to OutFindings (with the
	 * mixed row's clause) and logs one LogStealthFoundation warning naming the row's Id and the preset.
	 * Offline rows are skipped. Never writes a value (CR-6).
	 */
	static ETuningCheck EvaluateInvariants(TConstArrayView<FTuningInvariant> Rows, const FStealthServerTuning& Values, const FTuningDerived& Derived,
		const FTuningCheckContext& Context, TArray<FTuningInvariantFinding>& OutFindings);

	/** The schema-defaults value set: every row of Schema set to its Default column (not any asset). */
	static FStealthServerTuning BuildSchemaDefaults(TConstArrayView<FTuningSchemaRow> Schema);

	/**
	 * The fallback ladder (CR-6, ADR-0004 section 3): the requested preset, then Default, then the
	 * schema defaults, then Failed. Requesting Default skips the repeated rung. Per rung: load through
	 * the source (no value: Missing), copy the Session-latched keys from SessionFrozen, run F-1 with the
	 * preset's options, compute F-3, evaluate the invariants with bIsPublic (the source's flag; true for
	 * the schema-defaults rung) and the PresetId; any Reject rejects the whole preset and the next rung
	 * starts from that rung's own values (nothing of a rejected preset is applied). The first rung that
	 * does not reject is frozen into an FValidatedTuning with TuningHash and ClientHash (F-2) and
	 * Revision 0. When every rung fails the result is Failed (null Validated) and TuningLoadFailed is
	 * logged once.
	 */
	FTuningResolveResult Resolve(const FTuningResolveInput& Input) const;

	/**
	 * A value as it appears in warnings: integers without a decimal point ("5"), floats as the shortest
	 * decimal that round-trips through float ("7.5", "3"), non-finite as "NaN", "+Infinity", "-Infinity".
	 */
	static FString FormatTuningValue(uint8 TypeTag, double Value);
};
