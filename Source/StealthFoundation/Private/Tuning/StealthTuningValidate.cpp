// Story tdc-046: the F-1 validation stage (GDD F-1, CR-6 a/b; ADR-0004 section 3); story tdc-049: the
// safe-range warning in the same pass (CR-15.5); stories tdc-048/055: invariant evaluation (CR-6 c).
// See Server/StealthTuningPipelineCore.h.

#include "Server/StealthTuningPipelineCore.h"

#include "StealthFoundationModule.h"
#include "Server/StealthServerTuning.h"

#include <cmath>

double FTuningPipelineCore::ApplyF1(const FTuningSchemaRow& Row, double Authored, const FTuningF1Options& Options, ETuningF1Action& OutAction)
{
	OutAction = ETuningF1Action::Kept;
	if (Row.TypeTag == StealthTuningTypeTag::Bool)
	{
		return Authored;
	}
	// (1) Finite test before clamping.
	if (!std::isfinite(Authored))
	{
		OutAction = ETuningF1Action::DefaultedNonFinite;
		return Row.Default;
	}
	// (1b) An enum key accepts exactly its declared values (0..HardMax, whole numbers); anything else takes the default
	// (story con-001: the first enum key, BushVictimMode). Never clamped to a neighbouring value.
	if (Row.TypeTag == StealthTuningTypeTag::Enum)
	{
		if (Authored < Row.HardMin || Authored > Row.HardMax || Authored != FMath::RoundToDouble(Authored))
		{
			OutAction = ETuningF1Action::DefaultedUndeclaredEnum;
			return Row.Default;
		}
		return Authored;
	}
	// (2) A declared sentinel is valid by itself.
	if (Row.bHasSentinel && Authored == Row.Sentinel)
	{
		return Authored;
	}
	// (3) Clamp to the hard range (the catalog flag lowers NpcKillStrikeLimit's minimum to 1, never in Shipping).
	double HardMin = Row.HardMin;
#if !UE_BUILD_SHIPPING
	if (Options.bTestAllowsStrikeLimitOne && FCStringAnsi::Strcmp(Row.Key, TuningStrikeLimitKey) == 0)
	{
		HardMin = FMath::Min(HardMin, TuningStrikeLimitTestHardMin);
	}
#else
	(void)Options;
#endif
	const double Effective = FMath::Clamp(Authored, HardMin, Row.HardMax);
	if (Effective != Authored)
	{
		OutAction = ETuningF1Action::Clamped;
	}
	return Effective;
}

void FTuningPipelineCore::RunF1(TConstArrayView<FTuningSchemaRow> Schema, FStealthServerTuning& Values, const FTuningF1Options& Options, FTuningF1Report& OutReport)
{
	for (const FTuningSchemaRow& Row : Schema)
	{
		if (Row.Get == nullptr || Row.Set == nullptr)
		{
			continue;
		}
		const double Authored = Row.Get(Values);
		ETuningF1Action Action = ETuningF1Action::Kept;
		const double Effective = ApplyF1(Row, Authored, Options, Action);
		if (Action == ETuningF1Action::Kept)
		{
			// Story tdc-049: kept inside the hard range but outside the safe range: one warning, value kept.
			// A key F-1 changed is judged only by its clamp warning (one warning per key), even when its
			// effective value (a hard bound or the default) also lies outside the safe range.
			if (IsOutsideSafeRange(Row, Authored))
			{
				FTuningSafeRangeNote& Note = OutReport.SafeRange.AddDefaulted_GetRef();
				Note.Key = Row.Key;
				Note.Value = Authored;
				Note.SafeMin = Row.SafeMin;
				Note.SafeMax = Row.SafeMax;
				LogSafeRange(Row, Note);
			}
			continue;
		}
		Row.Set(Values, Effective);
		FTuningF1Change& Change = OutReport.Changes.AddDefaulted_GetRef();
		Change.Key = Row.Key;
		Change.Authored = Authored;
		Change.Effective = Effective;
		Change.Action = Action;
		LogF1Change(Row, Change);
	}
}

bool FTuningPipelineCore::IsOutsideSafeRange(const FTuningSchemaRow& Row, double Value)
{
	if (Row.TypeTag == StealthTuningTypeTag::Bool || (Row.bHasSentinel && Value == Row.Sentinel))
	{
		return false;
	}
	return Value < Row.SafeMin || Value > Row.SafeMax;
}

void FTuningPipelineCore::LogSafeRange(const FTuningSchemaRow& Row, const FTuningSafeRangeNote& Note)
{
	const FString Key(Row.Key);
	const FString Value = FormatTuningValue(Row.TypeTag, Note.Value);
	const FString SafeMin = FormatTuningValue(Row.TypeTag, Note.SafeMin);
	const FString SafeMax = FormatTuningValue(Row.TypeTag, Note.SafeMax);
	UE_LOGF(LogStealthFoundation, Warning, "Tuning safe range: key %ls value %ls kept, outside the safe range [%ls, %ls] (CR-15.5)", *Key, *Value, *SafeMin, *SafeMax);
}

ETuningCheck FTuningPipelineCore::EvaluateInvariants(TConstArrayView<FTuningInvariant> Rows, const FStealthServerTuning& Values, const FTuningDerived& Derived,
	const FTuningCheckContext& Context, TArray<FTuningInvariantFinding>& OutFindings)
{
	ETuningCheck Worst = ETuningCheck::Pass;
	const FString PresetText = Context.PresetId.ToString();
	for (const FTuningInvariant& Row : Rows)
	{
		if (Row.bOffline || Row.Check == nullptr)
		{
			continue;
		}
		const ETuningCheck Result = Row.Check(Values, Derived, Context);
		if (Result == ETuningCheck::Pass)
		{
			continue;
		}
		FTuningInvariantFinding& Finding = OutFindings.AddDefaulted_GetRef();
		Finding.Id = Row.Id;
		Finding.Clause = Result == ETuningCheck::Reject ? Row.RejectClause : Row.WarnClause;
		Finding.Result = Result;
		Worst = FMath::Max(Worst, Result);

		const FString Id(Row.Id);
		const TCHAR* const Severity = Result == ETuningCheck::Reject ? TEXT("reject") : TEXT("warn");
		if (Finding.Clause != nullptr)
		{
			const FString Clause(Finding.Clause);
			UE_LOGF(LogStealthFoundation, Warning, "Tuning invariant %ls %ls (clause: %ls) in preset %ls", *Id, Severity, *Clause, *PresetText);
		}
		else
		{
			UE_LOGF(LogStealthFoundation, Warning, "Tuning invariant %ls %ls in preset %ls", *Id, Severity, *PresetText);
		}
	}
	return Worst;
}

void FTuningPipelineCore::LogF1Change(const FTuningSchemaRow& Row, const FTuningF1Change& Change)
{
	const TCHAR* Why = Change.Action == ETuningF1Action::DefaultedNonFinite ? TEXT("non-finite, schema default")
		: Change.Action == ETuningF1Action::DefaultedUndeclaredEnum ? TEXT("undeclared enum value, schema default")
		: TEXT("clamped to the hard range");
	const FString Key(Row.Key);
	const FString Authored = FormatTuningValue(Row.TypeTag, Change.Authored);
	const FString Effective = FormatTuningValue(Row.TypeTag, Change.Effective);
	UE_LOGF(LogStealthFoundation, Warning, "Tuning F-1: key %ls authored %ls effective %ls (%ls)", *Key, *Authored, *Effective, Why);
}

FString FTuningPipelineCore::FormatTuningValue(uint8 TypeTag, double Value)
{
	if (std::isnan(Value))
	{
		return TEXT("NaN");
	}
	if (std::isinf(Value))
	{
		return Value > 0.0 ? TEXT("+Infinity") : TEXT("-Infinity");
	}
	if (TypeTag != StealthTuningTypeTag::Float)
	{
		return FString::Printf(TEXT("%lld"), static_cast<long long>(std::llround(Value)));
	}
	const float AsFloat = static_cast<float>(Value);
	double Scale = 1.0;
	for (int32 Decimals = 0; Decimals <= 9; ++Decimals, Scale *= 10.0)
	{
		const double Rounded = FMath::RoundToDouble(static_cast<double>(AsFloat) * Scale) / Scale;
		if (static_cast<float>(Rounded) == AsFloat)
		{
			FString Text = FString::Printf(TEXT("%.9f"), Rounded);
			while (Text.EndsWith(TEXT("0")))
			{
				Text.LeftChopInline(1);
			}
			if (Text.EndsWith(TEXT(".")))
			{
				Text.LeftChopInline(1);
			}
			return Text;
		}
	}
	return FString::Printf(TEXT("%.9g"), static_cast<double>(AsFloat));
}
