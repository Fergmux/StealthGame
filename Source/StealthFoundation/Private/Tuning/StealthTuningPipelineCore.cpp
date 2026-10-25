// Stories tdc-050 / tdc-051 / tdc-053: Resolve() and the fallback ladder (GDD CR-6, CR-7; ADR-0004
// section 3). See Server/StealthTuningPipelineCore.h. F-1 and the invariant evaluation are in
// StealthTuningValidate.cpp.

#include "Server/StealthTuningPipelineCore.h"

#include "StealthFoundationModule.h"
#include "Server/StealthTuningDerived.h"
#include "Server/StealthTuningHasher.h"
#include "Server/StealthTuningVersion.h"
#include "Server/StealthValidatedTuning.h"

const FTuningRungReport* FValidationReport::AcceptedRung() const
{
	return Rungs.FindByPredicate([](const FTuningRungReport& Rung) { return Rung.Outcome == ETuningRungOutcome::Accepted; });
}

bool FValidationReport::HasFinding(FAnsiStringView Id, ETuningCheck Result) const
{
	for (const FTuningRungReport& Rung : Rungs)
	{
		for (const FTuningInvariantFinding& Finding : Rung.Findings)
		{
			if (Finding.Result == Result && Finding.Id != nullptr && FAnsiStringView(Finding.Id).Equals(Id, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
	}
	return false;
}

FName FTuningPipelineCore::SchemaDefaultsPresetId()
{
	return FName(TEXT("SchemaDefaults"));
}

FName FTuningPipelineCore::DefaultPresetId()
{
	return FName(TEXT("Default"));
}

FStealthServerTuning FTuningPipelineCore::BuildSchemaDefaults(TConstArrayView<FTuningSchemaRow> Schema)
{
	FStealthServerTuning Values;
	for (const FTuningSchemaRow& Row : Schema)
	{
		if (Row.Set != nullptr)
		{
			Row.Set(Values, Row.Default);
		}
	}
	return Values;
}

namespace
{
	/** One rung's attempt: validates Loaded (when present) and records the rung; true when accepted. */
	bool TryRung(TConstArrayView<FTuningSchemaRow> Schema, TConstArrayView<FTuningInvariant> Invariants, const FStealthServerTuning* SessionFrozen,
		FName PresetId, ETuningFallback Level, const TOptional<FStealthServerTuning>& Loaded, bool bIsPublic, const FTuningF1Options& Options,
		FValidationReport& Report, FStealthServerTuning& OutValues)
	{
		FTuningRungReport& Rung = Report.Rungs.AddDefaulted_GetRef();
		Rung.PresetId = PresetId;
		Rung.Level = Level;
		if (!Loaded.IsSet())
		{
			Rung.Outcome = ETuningRungOutcome::Missing;
			const FString PresetText = PresetId.ToString();
			UE_LOGF(LogStealthFoundation, Warning, "Tuning preset %ls is missing or corrupted (no value set loaded); trying the next rung", *PresetText);
			return false;
		}

		FStealthServerTuning Values = Loaded.GetValue();
		if (SessionFrozen != nullptr)
		{
			// Session-latched keys (CR-11) are frozen at session open and carried unchanged into every round.
			for (const FTuningSchemaRow& Row : Schema)
			{
				if (Row.Latch == ETuningLatch::Session && Row.Get != nullptr && Row.Set != nullptr)
				{
					Row.Set(Values, Row.Get(*SessionFrozen));
				}
			}
		}
		FTuningPipelineCore::RunF1(Schema, Values, Options, Rung.F1);
		const FTuningDerived Derived = FTuningDerived::Compute(Values);
		FTuningCheckContext Context;
		Context.bIsPublic = bIsPublic;
		Context.PresetId = PresetId;
		Context.Schema = Schema;
		const ETuningCheck Worst = FTuningPipelineCore::EvaluateInvariants(Invariants, Values, Derived, Context, Rung.Findings);
		if (Worst == ETuningCheck::Reject)
		{
			// Rejected as a whole: nothing of this preset is applied (never partly repaired, CR-6).
			Rung.Outcome = ETuningRungOutcome::Rejected;
			const FString PresetText = PresetId.ToString();
			UE_LOGF(LogStealthFoundation, Warning, "Tuning preset %ls rejected as a whole by an invariant; trying the next rung", *PresetText);
			return false;
		}
		Rung.Outcome = ETuningRungOutcome::Accepted;
		OutValues = Values;
		return true;
	}
}

FTuningResolveResult FTuningPipelineCore::Resolve(const FTuningResolveInput& Input) const
{
	TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();
	TConstArrayView<FTuningInvariant> Invariants = GetTuningInvariants();
#if WITH_STEALTH_TEST_SEAMS
	if (Input.Schema.Num() > 0)
	{
		Schema = Input.Schema;
	}
	if (Input.Invariants.Num() > 0)
	{
		Invariants = Input.Invariants;
	}
#endif

	FTuningResolveResult Result;
	const FName DefaultId = DefaultPresetId();
	const FName Requested = Input.RequestedPreset.IsNone() ? DefaultId : Input.RequestedPreset;
	FStealthServerTuning Active;
	bool bAccepted = false;

	// Rungs 0 and 1: the requested preset, then Default (requesting Default skips the repeated rung).
	TArray<TPair<FName, ETuningFallback>, TInlineAllocator<2>> PresetRungs;
	if (Requested != DefaultId)
	{
		PresetRungs.Add({ Requested, ETuningFallback::Requested });
	}
	PresetRungs.Add({ DefaultId, ETuningFallback::Default });

	for (const TPair<FName, ETuningFallback>& PresetRung : PresetRungs)
	{
		bool bIsPublic = false;
		TOptional<FStealthServerTuning> Loaded;
		FTuningF1Options Options;
		if (Input.Source != nullptr)
		{
			Loaded = Input.Source->Load(PresetRung.Key, bIsPublic);
			Options = Input.Source->GetF1Options(PresetRung.Key);
		}
		if (TryRung(Schema, Invariants, Input.SessionFrozen, PresetRung.Key, PresetRung.Value, Loaded, bIsPublic, Options, Result.Report, Active))
		{
			Result.FallbackLevel = PresetRung.Value;
			Result.PresetId = PresetRung.Key;
			bAccepted = true;
			break;
		}
	}

	// Rung 2: the schema defaults (the generated table's Default column, not any asset), validated in
	// the strict scope it stands in for (Default is public). It has no catalog entry, so no F-1 flag.
	if (!bAccepted)
	{
		const FName SchemaId = SchemaDefaultsPresetId();
		const TOptional<FStealthServerTuning> Defaults(BuildSchemaDefaults(Schema));
		if (TryRung(Schema, Invariants, Input.SessionFrozen, SchemaId, ETuningFallback::SchemaDefaults, Defaults, true, FTuningF1Options(), Result.Report, Active))
		{
			Result.FallbackLevel = ETuningFallback::SchemaDefaults;
			Result.PresetId = SchemaId;
			bAccepted = true;
		}
	}

	if (!bAccepted)
	{
		// Rung 3: Failed. The round does not start; Round returns to Lobby (CR-6).
		const FString RequestedText = Requested.ToString();
		UE_LOGF(LogStealthFoundation, Warning, "TuningLoadFailed: every rung of the fallback ladder failed (requested preset %ls, Default, schema defaults)", *RequestedText);
		Result.PresetId = NAME_None;
		return Result;
	}

	if (Result.FallbackLevel != ETuningFallback::Requested)
	{
		const FString RequestedText = Requested.ToString();
		const FString ActiveText = Result.PresetId.ToString();
		UE_LOGF(LogStealthFoundation, Warning, "Tuning fallback: requested preset %ls not used; %ls active at FallbackLevel %d", *RequestedText, *ActiveText,
			static_cast<int32>(Result.FallbackLevel));
	}

	const uint64 TuningHash = FTuningHasher::Hash(Schema, Active, TuningSchemaVersion, false);
	const uint64 ClientHash = FTuningHasher::Hash(Schema, Active, TuningSchemaVersion, true);
	Result.Validated = TSharedPtr<const FValidatedTuning>(
		MakeShared<FValidatedTuning>(Active, TuningHash, ClientHash, TuningSchemaVersion, Result.PresetId, 0, Result.FallbackLevel, Result.Report));
	return Result;
}
