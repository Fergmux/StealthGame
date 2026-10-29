// Story con-001 (production/epics/concealment-bushes/story-001-concealment-tuning-keys-and-timing-validator.md), AC1 and
// AC2 (GDD concealment-bushes.md Tuning Knobs, F3, AC-CON-11 data part; ADR-0017 section 3):
//   - StealthGame.Concealment.TuningRows: the eleven Concealment keys are schema rows equal to the committed GDD fixture
//     (default, hard range, safe range, audience, latch, unit); the per-key range gate (F-1) keeps min and max and clamps
//     min - 0.001 and max + 0.001 for every numeric key; BushVictimMode accepts exactly its three values.
//   - StealthGame.Concealment.TimingChain: the F3 validator (range gate bypassed, injected constants of the AC) passes the
//     defaults and names exactly {b}, {a,e}, {d}, {f} for the four failing data sets; the preset-load rows I-CON-F3a..f pass
//     the schema defaults.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthConcealmentInvariants.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningDerived.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/Testing/StealthConcealmentTuningKnobs.h"

namespace ConcealmentTimingChainTestData
{
	/** The injected constants of AC2 (never the preset-load inputs: the test pins the GDD's worked numbers). */
	FConcealmentF3Inputs AcInputs()
	{
		FConcealmentF3Inputs In;
		In.ConcealCullDelayMs = 250.0;
		In.WalkSpeedCmPerSec = 140.0;
		In.CapsuleRadiusCm = 35.0;
		In.StrikeContactSec = 0.4;
		In.L99Ms = 150.0;
		In.PawnNetUpdateHz = 15.0;
		In.ReopenMs = 33.0;
		return In;
	}

	const FTuningSchemaRow* FindRow(const ANSICHAR* Key)
	{
		for (const FTuningSchemaRow& Row : GetTuningSchema())
		{
			if (FCStringAnsi::Strcmp(Row.Key, Key) == 0)
			{
				return &Row;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentTuningRowsTest, "StealthGame.Concealment.TuningRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentTuningRowsTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentTimingChainTestData;

	// Exactly the eleven keys under owner Concealment.
	int32 OwnerRows = 0;
	for (const FTuningSchemaRow& Row : GetTuningSchema())
	{
		OwnerRows += Row.Owner != nullptr && FCStringAnsi::Strcmp(Row.Owner, StealthConcealmentTuningKnobs::Owner) == 0 ? 1 : 0;
	}
	TestEqual(TEXT("owner Concealment has exactly the eleven GDD keys (a missing or extra key fails)"), OwnerRows, 11);

	const FTuningF1Options Options;
	for (const StealthConcealmentTuningKnobs::FKnob& Knob : StealthConcealmentTuningKnobs::Knobs)
	{
		const FString Key(Knob.Key);
		const FTuningSchemaRow* Row = FindRow(Knob.Key);
		if (!TestNotNull(FString::Printf(TEXT("%s is a schema row"), *Key), Row))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s owner"), *Key), FString(Row->Owner), FString(StealthConcealmentTuningKnobs::Owner));
		TestEqual(FString::Printf(TEXT("%s default"), *Key), Row->Default, static_cast<double>(Knob.Default), 1e-6);
		TestEqual(FString::Printf(TEXT("%s hard min"), *Key), Row->HardMin, static_cast<double>(Knob.HardMin), 1e-6);
		TestEqual(FString::Printf(TEXT("%s hard max"), *Key), Row->HardMax, static_cast<double>(Knob.HardMax), 1e-6);
		TestEqual(FString::Printf(TEXT("%s safe min"), *Key), Row->SafeMin, static_cast<double>(Knob.SafeMin), 1e-6);
		TestEqual(FString::Printf(TEXT("%s safe max"), *Key), Row->SafeMax, static_cast<double>(Knob.SafeMax), 1e-6);
		TestEqual(FString::Printf(TEXT("%s audience (All = public)"), *Key), Row->bPublic, Knob.bPublic);
		TestEqual(FString::Printf(TEXT("%s latch"), *Key), static_cast<int32>(Row->Latch), static_cast<int32>(Knob.Latch));
		TestEqual(FString::Printf(TEXT("%s unit"), *Key), FString(Row->Unit), FString(Knob.Unit));
		TestEqual(FString::Printf(TEXT("%s type"), *Key), static_cast<int32>(Row->TypeTag),
			static_cast<int32>(Knob.bEnum ? StealthTuningTypeTag::Enum : StealthTuningTypeTag::Float));

		ETuningF1Action Action = ETuningF1Action::Kept;
		if (Knob.bEnum)
		{
			// BushVictimMode: exactly UniformHash (0), EarliestCommitted (1) and All (2); anything else takes the default.
			for (const double Value : { 0.0, 1.0, 2.0 })
			{
				const double Effective = FTuningPipelineCore::ApplyF1(*Row, Value, Options, Action);
				TestTrue(FString::Printf(TEXT("%s accepts %g"), *Key, Value), Action == ETuningF1Action::Kept && Effective == Value);
			}
			for (const double Value : { -1.0, 3.0, 1.5, 255.0 })
			{
				const double Effective = FTuningPipelineCore::ApplyF1(*Row, Value, Options, Action);
				TestTrue(FString::Printf(TEXT("%s rejects %g (undeclared value -> UniformHash)"), *Key, Value),
					Action == ETuningF1Action::DefaultedUndeclaredEnum && Effective == 0.0);
			}
			continue;
		}
		// The range gate only (F3 is TimingChain): min and max load, min - 0.001 and max + 0.001 are clamped.
		const double Epsilon = 0.001;
		double Effective = FTuningPipelineCore::ApplyF1(*Row, Row->HardMin, Options, Action);
		TestTrue(FString::Printf(TEXT("%s accepts its hard min %g"), *Key, Row->HardMin), Action == ETuningF1Action::Kept && Effective == Row->HardMin);
		Effective = FTuningPipelineCore::ApplyF1(*Row, Row->HardMax, Options, Action);
		TestTrue(FString::Printf(TEXT("%s accepts its hard max %g"), *Key, Row->HardMax), Action == ETuningF1Action::Kept && Effective == Row->HardMax);
		Effective = FTuningPipelineCore::ApplyF1(*Row, Row->HardMin - Epsilon, Options, Action);
		TestTrue(FString::Printf(TEXT("%s rejects min - 0.001 (clamped)"), *Key), Action == ETuningF1Action::Clamped && Effective == Row->HardMin);
		Effective = FTuningPipelineCore::ApplyF1(*Row, Row->HardMax + Epsilon, Options, Action);
		TestTrue(FString::Printf(TEXT("%s rejects max + 0.001 (clamped)"), *Key), Action == ETuningF1Action::Clamped && Effective == Row->HardMax);
	}

	// Edge: BushEnterSwapSec 0.3 passes the range gate (F3(d) judges it separately).
	if (const FTuningSchemaRow* Swap = FindRow("BushEnterSwapSec"))
	{
		ETuningF1Action Action = ETuningF1Action::Kept;
		FTuningPipelineCore::ApplyF1(*Swap, static_cast<double>(0.3f), Options, Action); // the float key as authored
		TestTrue(TEXT("BushEnterSwapSec 0.3 passes the range gate"), Action == ETuningF1Action::Kept);
		FStealthServerTuning V;
		V.Concealment_Public.BushEnterSwapSec = 0.3f;
		TestEqual(TEXT("... and F3 reports (d) for it, separately"), StealthConcealmentInvariants::DescribeF3(StealthConcealmentInvariants::EvaluateF3(V, AcInputs())), FString(TEXT("d")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentTimingChainTest, "StealthGame.Concealment.TimingChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentTimingChainTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentTimingChainTestData;
	using namespace StealthConcealmentInvariants;
	const FConcealmentF3Inputs In = AcInputs();

	// The GDD's worked numbers at the defaults.
	const FStealthServerTuning Defaults;
	TestEqual(TEXT("T_uncull = 150 + 66.7 + 33 ms ~= 0.25 s"), In.UncullSec(), 0.2497, 0.0005);
	TestEqual(TEXT("(d) warp mean speed (60 + 35) / 0.5 = 190 cm/s"), WarpMeanSpeedCmPerSec(Defaults, In), 190.0, 1e-3);
	TestEqual(TEXT("defaults: (a) 0.9 >= 0.75, (b) 0.45 >= 0.25, (c) 0.35 >= 0.25, (d) 190 <= 210, (e), (f) 0.75 <= 1.2 all pass"),
		DescribeF3(EvaluateF3(Defaults, In)), FString());

	// The four failing data sets (range gate bypassed: values set directly).
	{
		FStealthServerTuning V;
		V.Concealment_Public.BushExitCrossSec = 0.2f;
		TestEqual(TEXT("BushExitCrossSec 0.2 -> {b}"), DescribeF3(EvaluateF3(V, In)), FString(TEXT("b")));
	}
	{
		FStealthServerTuning V;
		V.Concealment_Public.BushEnterSwapSec = 0.9f;
		V.Concealment_Public.BushEnterDurSec = 0.8f;
		TestEqual(TEXT("BushEnterSwapSec 0.9 with BushEnterDurSec 0.8 -> {a,e}"), DescribeF3(EvaluateF3(V, In)), FString(TEXT("ae")));
	}
	{
		FStealthServerTuning V;
		V.Concealment_Public.BushEntryReachCm = 100.0f;
		TestEqual(TEXT("BushEntryReachCm 100 -> {d} (270 cm/s)"), DescribeF3(EvaluateF3(V, In)), FString(TEXT("d")));
	}
	{
		FStealthServerTuning V;
		V.Concealment_Public.BushStrikeEmergeSec = 0.5f;
		FConcealmentF3Inputs Slow = In;
		Slow.StrikeContactSec = 0.8;
		TestEqual(TEXT("BushStrikeEmergeSec 0.5 with StrikeContactSec 0.8 -> {f}"), DescribeF3(EvaluateF3(V, Slow)), FString(TEXT("f")));
	}

	// Edges: Reach 70 with Swap 0.5 is exactly 210 (passes); Swap 0.45 at Reach 60 is 211 cm/s (fails (d)).
	{
		FStealthServerTuning V;
		V.Concealment_Public.BushEntryReachCm = 70.0f;
		TestEqual(TEXT("Reach 70, Swap 0.5: 210 cm/s passes (d)"), EvaluateF3(V, In) & F3d, static_cast<uint8>(0));
	}
	{
		FStealthServerTuning V;
		V.Concealment_Public.BushEnterSwapSec = 0.45f;
		TestTrue(TEXT("Swap 0.45 at Reach 60: 211 cm/s fails (d)"), (EvaluateF3(V, In) & F3d) != 0);
	}

	// Preset load: the six Check rows are in the TDC table and pass the schema defaults.
	const FStealthServerTuning SchemaDefaults = FTuningPipelineCore::BuildSchemaDefaults(GetTuningSchema());
	const FTuningDerived Derived = FTuningDerived::Compute(SchemaDefaults);
	FTuningCheckContext Context;
	Context.bIsPublic = true;
	int32 Rows = 0;
	for (const FTuningInvariant& Row : GetTuningInvariants())
	{
		if (Row.Owner != nullptr && FCStringAnsi::Strcmp(Row.Owner, "Concealment") == 0)
		{
			++Rows;
			TestEqual(FString::Printf(TEXT("%s passes the schema defaults"), *FString(Row.Id)), static_cast<int32>(Row.Check(SchemaDefaults, Derived, Context)), static_cast<int32>(ETuningCheck::Pass));
		}
	}
	TestEqual(TEXT("six Concealment rows I-CON-F3a..f in the preset-load table"), Rows, 6);

	// Negative control through a row: a set that fails (b) is rejected by I-CON-F3b only.
	FStealthServerTuning Bad = SchemaDefaults;
	Bad.Concealment_Public.BushExitCrossSec = 0.2f;
	for (const FTuningInvariant& Row : GetTuningInvariants())
	{
		if (Row.Owner != nullptr && FCStringAnsi::Strcmp(Row.Owner, "Concealment") == 0)
		{
			const bool bIsB = FCStringAnsi::Strcmp(Row.Id, "I-CON-F3b") == 0;
			TestEqual(FString::Printf(TEXT("BushExitCrossSec 0.2: %s"), *FString(Row.Id)), static_cast<int32>(Row.Check(Bad, Derived, Context)),
				static_cast<int32>(bIsB ? ETuningCheck::Reject : ETuningCheck::Pass));
		}
	}
	return true;
}

#endif
