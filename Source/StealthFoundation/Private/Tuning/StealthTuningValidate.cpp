// Story tdc-046: the F-1 validation stage (GDD F-1, CR-6 a/b; ADR-0004 section 3). See
// Server/StealthTuningPipelineCore.h.

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
