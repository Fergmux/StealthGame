#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

struct FStealthServerTuning;

/**
 * The plain-C++ tuning pipeline core (ADR-0004 section 3). Story tdc-046 introduces the F-1 stage;
 * story tdc-050 extends this header with FTuningResolveInput/Result, FValidationReport,
 * ITuningPresetSource and ETuningFallback. No UWorld, no asset access: headless-testable.
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

/**
 * Inputs of F-1 that are not values. bTestAllowsStrikeLimitOne is the catalog flag of the preset being
 * validated (UStealthTuningCatalog entry; the carrier is this explicit stage input, shared with story
 * tdc-050's resolve input): it lowers NpcKillStrikeLimit's hard minimum from 2 to 1 for that preset only.
 * The catalog check keeps it off Default and HostSelectable presets; Shipping ignores it.
 */
struct FTuningF1Options
{
	bool bTestAllowsStrikeLimitOne = false;
};

/** The F-1 part of the validation report (story tdc-050 folds it into FValidationReport). */
struct FTuningF1Report
{
	TArray<FTuningF1Change> Changes;
};

/** The key whose hard minimum the catalog flag lowers (GDD F-1, user decision 2026-10-25). */
inline constexpr const ANSICHAR* TuningStrikeLimitKey = "NpcKillStrikeLimit";

/** NpcKillStrikeLimit's hard minimum in a bTestAllowsStrikeLimitOne preset (GDD F-1; not a preset value). */
inline constexpr double TuningStrikeLimitTestHardMin = 1.0;

class STEALTHFOUNDATION_API FTuningPipelineCore
{
public:
	/**
	 * GDD F-1 for one key, in this order: (1) a non-finite float takes Row.Default (tested before
	 * clamping: clamp of NaN is NaN, and +/-Infinity take the default, not a bound); (2) a value equal
	 * to the row's declared sentinel is kept; (3) otherwise the value is clamped to [HardMin, HardMax]
	 * (a value exactly on a bound is kept); bools pass unchanged. Pure: no log, no state.
	 */
	static double ApplyF1(const FTuningSchemaRow& Row, double Authored, const FTuningF1Options& Options, ETuningF1Action& OutAction);

	/**
	 * Runs F-1 over every row of Schema on Values in place (through each row's Get/Set), appends one
	 * FTuningF1Change per changed key to OutReport, and logs one LogStealthFoundation warning per
	 * changed key naming the key, its authored and its effective value. F-1 is the only code that
	 * changes a tuning value, and only to the key's own hard range (ADR-0004 guideline 5). Safe-range
	 * excursions are warning-only and belong to story tdc-049. Invariants are never evaluated here.
	 */
	static void RunF1(TConstArrayView<FTuningSchemaRow> Schema, FStealthServerTuning& Values, const FTuningF1Options& Options, FTuningF1Report& OutReport);

	/** Logs the F-1 warning for one change (also used by RunF1); the value text is FormatTuningValue. */
	static void LogF1Change(const FTuningSchemaRow& Row, const FTuningF1Change& Change);

	/**
	 * A value as it appears in warnings: integers without a decimal point ("5"), floats as the shortest
	 * decimal that round-trips through float ("7.5", "3"), non-finite as "NaN", "+Infinity", "-Infinity".
	 */
	static FString FormatTuningValue(uint8 TypeTag, double Value);
};
