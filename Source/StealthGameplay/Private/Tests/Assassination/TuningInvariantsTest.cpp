// Story asn-001 (production/epics/assassination-defence-reversal/story-001-assassination-tuning-keys-and-invariants.md),
// GDD assassination-defence-reversal.md Tuning Knobs and AC-ASN-31.
//   AC1  the generated schema holds exactly the 16 Assassination keys of the GDD table (fixture
//        Server/Testing/StealthAssassinationTuningKnobs.h): every field equal, 13 All and 3 ServerOnly, range hard 100-250,
//        no key owned elsewhere registered under Assassination.
//   AC2  presets (a), (b), (c), (d), (f), (i) are rejected naming exactly I-ASN-1, 2, 3, 4, 5, 8; only (c) warns (I-ASN-6).
//   AC3  presets (e), (g), (h), (j) are accepted with exactly I-ASN-5w, I-ASN-5w, I-ASN-7, I-ASN-6; the default preset has
//        no finding, also through the production TDC table.
// Externally owned keys are injected at their owner GDD defaults (BushStrikeEmergeSec 0.35, CrowdReplacementMinDelaySec
// 10, InformantRespawnDelaySec 15, PawnRefAgreementCm 75) unless the preset overrides one. Every override lies inside its
// hard range, so F-1 changes nothing: a rejected preset is never clamped or repaired.
// Safe-range lint (Implementation Notes): every key at its safe minimum and maximum, others at defaults, rejects nothing
// and warns only inside the documented bands. The former pinned conflict (StrikeWarpYawLimitDeg safe minimum 20 rejected by
// I-ASN-8 at StrikeBearingLimitDeg 90) is resolved by decision D55 (safe minimum raised to 30 = 90 - 60), so no key at a
// safe bound is rejected. The lint fails if any finding appears or disappears.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthAssassinationInvariants.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningDerived.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/Testing/StealthAssassinationTuningKnobs.h"

namespace AssassinationTuningInvariantsTestData
{
	using StealthAssassinationTuningKnobs::FKnob;

	/** The externally owned keys at their owner GDD defaults (the AC's injected values). */
	FAssassinationInvariantInputs GddInputs()
	{
		FAssassinationInvariantInputs In;
		In.CapsuleRadiusCm = 35.0;
		In.BushStrikeEmergeSec = 0.35;
		In.CrowdReplacementMinDelaySec = 10.0;
		In.InformantRespawnDelaySec = 15.0;
		In.PawnRefAgreementCm = 75.0;
		return In;
	}

	const FTuningSchemaRow* FindRow(TConstArrayView<FTuningSchemaRow> Schema, const ANSICHAR* Key)
	{
		for (const FTuningSchemaRow& Row : Schema)
		{
			if (FCStringAnsi::Strcmp(Row.Key, Key) == 0)
			{
				return &Row;
			}
		}
		return nullptr;
	}

	/** One named preset: overrides on the schema defaults, an optional injected-input change, and the expected findings. */
	struct FPreset
	{
		const TCHAR* Name;
		TFunction<void(FStealthServerTuning&)> Override;
		TFunction<void(FAssassinationInvariantInputs&)> Inject;
		TArray<FString> ExpectRejected;
		TArray<FString> ExpectWarned;
	};

	FString Join(const TArray<FString>& Ids) { return Ids.IsEmpty() ? FString(TEXT("(none)")) : FString::Join(Ids, TEXT(", ")); }

	/** The preset's value set after F-1, plus the number of keys F-1 had to change (0 means never clamped). */
	FStealthServerTuning Build(const FPreset& Preset, int32& OutF1Changes)
	{
		const TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();
		FStealthServerTuning Values = FTuningPipelineCore::BuildSchemaDefaults(Schema);
		if (Preset.Override)
		{
			Preset.Override(Values);
		}
		OutF1Changes = 0;
		const FTuningF1Options Options;
		for (const FTuningSchemaRow& Row : Schema)
		{
			if (Row.Get == nullptr || Row.Set == nullptr)
			{
				continue;
			}
			ETuningF1Action Action = ETuningF1Action();
			const double Authored = Row.Get(Values);
			const double Effective = FTuningPipelineCore::ApplyF1(Row, Authored, Options, Action);
			if (Effective != Authored)
			{
				++OutF1Changes;
				Row.Set(Values, Effective);
			}
		}
		return Values;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationTuningInvariantsTest, "StealthGame.Assassination.TuningInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationTuningInvariantsTest::RunTest(const FString& Parameters)
{
	using namespace AssassinationTuningInvariantsTestData;
	const TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();

	// ---- AC1: the schema rows of owner Assassination equal the GDD table, field for field ----
	{
		int32 AllCount = 0;
		int32 ServerOnlyCount = 0;
		for (const FKnob& Knob : StealthAssassinationTuningKnobs::Knobs)
		{
			const FString Key(Knob.Key);
			const FTuningSchemaRow* Row = FindRow(Schema, Knob.Key);
			if (!TestNotNull(FString::Printf(TEXT("%s has a schema row"), *Key), Row))
			{
				continue;
			}
			// Exact float equality: the schema stores the float initialiser widened, so a 1-ulp default error fails.
			TestEqual(FString::Printf(TEXT("%s type is float"), *Key), Row->TypeTag, StealthTuningTypeTag::Float);
			TestTrue(FString::Printf(TEXT("%s default %g (GDD %g)"), *Key, Row->Default, Knob.Default), Row->Default == static_cast<double>(Knob.Default));
			TestTrue(FString::Printf(TEXT("%s hard range [%g, %g]"), *Key, Row->HardMin, Row->HardMax), Row->HardMin == static_cast<double>(Knob.HardMin) && Row->HardMax == static_cast<double>(Knob.HardMax));
			TestTrue(FString::Printf(TEXT("%s safe range [%g, %g]"), *Key, Row->SafeMin, Row->SafeMax), Row->SafeMin == static_cast<double>(Knob.SafeMin) && Row->SafeMax == static_cast<double>(Knob.SafeMax));
			TestEqual(FString::Printf(TEXT("%s audience"), *Key), Row->bPublic, Knob.bPublic);
			TestEqual(FString::Printf(TEXT("%s latch"), *Key), static_cast<int32>(Row->Latch), static_cast<int32>(Knob.Latch));
			TestTrue(FString::Printf(TEXT("%s owner is Assassination"), *Key), Row->Owner != nullptr && FCStringAnsi::Strcmp(Row->Owner, StealthAssassinationTuningKnobs::Owner) == 0);
			TestTrue(FString::Printf(TEXT("%s unit %hs"), *Key, Knob.Unit), Row->Unit != nullptr && FCStringAnsi::Strcmp(Row->Unit, Knob.Unit) == 0);
			(Knob.bPublic ? AllCount : ServerOnlyCount) += 1;
		}
		TestEqual(TEXT("the GDD table has 16 Assassination keys"), static_cast<int32>(UE_ARRAY_COUNT(StealthAssassinationTuningKnobs::Knobs)), 16);
		TestEqual(TEXT("13 keys are All"), AllCount, 13);
		TestEqual(TEXT("3 keys are ServerOnly (StrikeVerticalReachCm, OpenSpotToleranceCm, StrikeNetLeadSec)"), ServerOnlyCount, 3);

		// No extra Assassination row, and no externally owned key registered under Assassination.
		int32 OwnerRows = 0;
		for (const FTuningSchemaRow& Row : Schema)
		{
			if (Row.Owner == nullptr || FCStringAnsi::Strcmp(Row.Owner, StealthAssassinationTuningKnobs::Owner) != 0)
			{
				continue;
			}
			++OwnerRows;
			bool bListed = false;
			for (const FKnob& Knob : StealthAssassinationTuningKnobs::Knobs)
			{
				bListed |= FCStringAnsi::Strcmp(Knob.Key, Row.Key) == 0;
			}
			TestTrue(FString::Printf(TEXT("Assassination schema row %hs is in the GDD table"), Row.Key), bListed);
		}
		TestEqual(TEXT("the schema has exactly 16 Assassination rows"), OwnerRows, 16);
		for (const ANSICHAR* Foreign : StealthAssassinationTuningKnobs::ForeignKeys)
		{
			const FTuningSchemaRow* Row = FindRow(Schema, Foreign);
			TestTrue(FString::Printf(TEXT("%hs is not registered under Assassination"), Foreign), Row == nullptr || FCStringAnsi::Strcmp(Row->Owner, StealthAssassinationTuningKnobs::Owner) != 0);
		}
		const FTuningSchemaRow* Range = FindRow(Schema, "AssassinationRangeCm");
		TestTrue(TEXT("AssassinationRangeCm hard range is 100-250"), Range != nullptr && Range->HardMin == 100.0 && Range->HardMax == 250.0);
	}

	// ---- AC2 and AC3: the presets of AC-ASN-31 ----
	TArray<FPreset> Presets;
	Presets.Add({ TEXT("(a) range 250, warp 150, gap 5"), [](FStealthServerTuning& V) { V.Assassination_Public.AssassinationRangeCm = 250.0f; V.Assassination_Public.StrikeWarpLimitCm = 150.0f; V.Assassination_Public.StrikeContactGapCm = 5.0f; }, nullptr, { TEXT("I-ASN-1") }, {} });
	Presets.Add({ TEXT("(b) counter 2.0, lockout 1.75"), [](FStealthServerTuning& V) { V.Assassination_Public.CounterDurationSec = 2.0f; }, nullptr, { TEXT("I-ASN-2") }, {} });
	Presets.Add({ TEXT("(c) contact 0.8, BushStrikeEmergeSec 0.5"), [](FStealthServerTuning& V) { V.Assassination_Public.StrikeContactSec = 0.8f; }, [](FAssassinationInvariantInputs& In) { In.BushStrikeEmergeSec = 0.5; }, { TEXT("I-ASN-3") }, { TEXT("I-ASN-6") } });
	Presets.Add({ TEXT("(d) net lead 0.3, contact 0.4"), [](FStealthServerTuning& V) { V.Assassination_Server.StrikeNetLeadSec = 0.3f; }, nullptr, { TEXT("I-ASN-4") }, {} });
	Presets.Add({ TEXT("(f) death fall 1.0"), [](FStealthServerTuning& V) { V.Assassination_Public.DeathFallSec = 1.0f; }, nullptr, { TEXT("I-ASN-5") }, {} });
	Presets.Add({ TEXT("(i) bearing 100, warp yaw 30"), [](FStealthServerTuning& V) { V.Assassination_Public.StrikeBearingLimitDeg = 100.0f; }, nullptr, { TEXT("I-ASN-8") }, {} });
	Presets.Add({ TEXT("(e) fall 1.6, linger 8, grace 2.5"), [](FStealthServerTuning& V) { V.Assassination_Public.DeathFallSec = 1.6f; V.Assassination_Public.BodyLingerSec = 8.0f; V.Assassination_Public.BodyRemovalGraceSec = 2.5f; }, nullptr, {}, { TEXT("I-ASN-5w") } });
	Presets.Add({ TEXT("(g) fall 1.5, linger 6.5, grace 2.0 (sum exactly 10.0, strict)"), [](FStealthServerTuning& V) { V.Assassination_Public.DeathFallSec = 1.5f; V.Assassination_Public.BodyLingerSec = 6.5f; V.Assassination_Public.BodyRemovalGraceSec = 2.0f; }, nullptr, {}, { TEXT("I-ASN-5w") } });
	Presets.Add({ TEXT("(h) contact 0.30, warp 240 (800 cm/s)"), [](FStealthServerTuning& V) { V.Assassination_Public.StrikeContactSec = 0.30f; V.Assassination_Public.StrikeWarpLimitCm = 240.0f; }, nullptr, {}, { TEXT("I-ASN-7") } });
	Presets.Add({ TEXT("(j) lockout 1.2, contact 0.4, counter 1.2"), [](FStealthServerTuning& V) { V.Assassination_Public.AttackAttemptLockoutSec = 1.2f; V.Assassination_Public.CounterDurationSec = 1.2f; }, nullptr, {}, { TEXT("I-ASN-6") } });
	Presets.Add({ TEXT("default preset"), nullptr, nullptr, {}, {} });

	for (const FPreset& Preset : Presets)
	{
		int32 F1Changes = 0;
		const FStealthServerTuning Values = Build(Preset, F1Changes);
		FAssassinationInvariantInputs Inputs = GddInputs();
		if (Preset.Inject)
		{
			Preset.Inject(Inputs);
		}
		const FStealthServerTuning Before = Values;
		const FAssassinationInvariantReport Report = StealthAssassinationInvariants::Evaluate(Values, Inputs);
		TestEqual(FString::Printf(TEXT("%s: F-1 changes nothing (inside every hard range)"), Preset.Name), F1Changes, 0);
		TestTrue(FString::Printf(TEXT("%s: rejections %s, want %s"), Preset.Name, *Join(Report.Rejected), *Join(Preset.ExpectRejected)), Report.Rejected == Preset.ExpectRejected);
		TestTrue(FString::Printf(TEXT("%s: warnings %s, want %s"), Preset.Name, *Join(Report.Warned), *Join(Preset.ExpectWarned)), Report.Warned == Preset.ExpectWarned);
		bool bUnchanged = true;
		for (const FTuningSchemaRow& Row : Schema)
		{
			bUnchanged &= Row.Get == nullptr || Row.Get(Before) == Row.Get(Values);
		}
		TestTrue(FString::Printf(TEXT("%s: evaluation never writes a value"), Preset.Name), bUnchanged);
	}

	// ---- The production TDC table: every row registered under owner Assassination, default preset clean ----
	{
		const TConstArrayView<FTuningInvariant> Table = GetTuningInvariants();
		for (const StealthAssassinationInvariants::FRow& Row : StealthAssassinationInvariants::GetRows())
		{
			int32 Found = 0;
			for (const FTuningInvariant& Entry : Table)
			{
				if (FCStringAnsi::Strcmp(Entry.Id, Row.Id) == 0)
				{
					++Found;
					TestTrue(FString::Printf(TEXT("%hs owner is Assassination"), Row.Id), FCStringAnsi::Strcmp(Entry.Owner, "Assassination") == 0);
				}
			}
			TestEqual(FString::Printf(TEXT("%hs is registered once in the TDC preset-load table"), Row.Id), Found, 1);
		}
		const FStealthServerTuning Defaults = FTuningPipelineCore::BuildSchemaDefaults(Schema);
		TArray<FTuningInvariantFinding> Findings;
		FTuningCheckContext Context;
		Context.bIsPublic = true;
		Context.PresetId = FTuningPipelineCore::DefaultPresetId();
		FTuningPipelineCore::EvaluateInvariants(Table, Defaults, FTuningDerived::Compute(Defaults), Context, Findings);
		for (const FTuningInvariantFinding& Finding : Findings)
		{
			TestFalse(FString::Printf(TEXT("default preset: no Assassination finding (got %hs)"), Finding.Id), FCStringAnsi::Strncmp(Finding.Id, "I-ASN", 5) == 0 || FCStringAnsi::Strncmp(Finding.Id, "W-ASN", 5) == 0);
		}
		// The table adapter reads the same inputs a test injects at the GDD defaults.
		const FAssassinationInvariantInputs Live = FAssassinationInvariantInputs::FromValues(Defaults);
		const FAssassinationInvariantInputs Gdd = GddInputs();
		TestTrue(TEXT("live inputs equal the GDD defaults"), Live.CapsuleRadiusCm == Gdd.CapsuleRadiusCm && Live.BushStrikeEmergeSec == Gdd.BushStrikeEmergeSec
			&& Live.CrowdReplacementMinDelaySec == Gdd.CrowdReplacementMinDelaySec && Live.InformantRespawnDelaySec == Gdd.InformantRespawnDelaySec && Live.PawnRefAgreementCm == Gdd.PawnRefAgreementCm);
	}

	// ---- Safe-range lint: each key at each safe bound, others at defaults ----
	{
		// Documented findings inside the safe ranges: the I-ASN-7 band (GDD Tuning Knobs, C-W18), the I-ASN-5w tidiness
		// warning at the BodyLingerSec safe maximum (the GDD row names it), W-ASN-9 for any OpenSpotToleranceCm other than
		// PawnRefAgreementCm. No rejection: D55 raised the StrikeWarpYawLimitDeg safe minimum to 30 (I-ASN-8 at bearing 90).
		const TMap<FString, FString> Expected = {
			{ TEXT("StrikeContactSec@SafeMin"), TEXT("warn I-ASN-7") },
			{ TEXT("BodyLingerSec@SafeMax"), TEXT("warn I-ASN-5w") },
			{ TEXT("OpenSpotToleranceCm@SafeMin"), TEXT("warn W-ASN-9") },
			{ TEXT("OpenSpotToleranceCm@SafeMax"), TEXT("warn W-ASN-9") },
		};
		TMap<FString, FString> Found;
		for (const FKnob& Knob : StealthAssassinationTuningKnobs::Knobs)
		{
			const FTuningSchemaRow* Row = FindRow(Schema, Knob.Key);
			if (Row == nullptr || Row->Set == nullptr)
			{
				continue;
			}
			for (int32 Bound = 0; Bound < 2; ++Bound)
			{
				FStealthServerTuning Values = FTuningPipelineCore::BuildSchemaDefaults(Schema);
				Row->Set(Values, Bound == 0 ? Row->SafeMin : Row->SafeMax);
				const FAssassinationInvariantReport Report = StealthAssassinationInvariants::Evaluate(Values, GddInputs());
				TArray<FString> Parts;
				for (const FString& Id : Report.Rejected) { Parts.Add(TEXT("reject ") + Id); }
				for (const FString& Id : Report.Warned) { Parts.Add(TEXT("warn ") + Id); }
				if (!Parts.IsEmpty())
				{
					Found.Add(FString::Printf(TEXT("%hs@%s"), Knob.Key, Bound == 0 ? TEXT("SafeMin") : TEXT("SafeMax")), FString::Join(Parts, TEXT("; ")));
				}
			}
		}
		for (const TPair<FString, FString>& Pair : Found)
		{
			const FString* Want = Expected.Find(Pair.Key);
			TestTrue(FString::Printf(TEXT("safe-range lint: %s -> %s is documented"), *Pair.Key, *Pair.Value), Want != nullptr && *Want == Pair.Value);
		}
		for (const TPair<FString, FString>& Pair : Expected)
		{
			TestTrue(FString::Printf(TEXT("safe-range lint: documented finding %s -> %s still occurs (remove it when the GDD changes)"), *Pair.Key, *Pair.Value), Found.Contains(Pair.Key));
		}
	}

	// ---- D55: the warp-yaw safe minimum is the smallest value I-ASN-8 accepts at the bearing safe maximum ----
	{
		const FTuningSchemaRow* Yaw = FindRow(Schema, "StrikeWarpYawLimitDeg");
		const FTuningSchemaRow* Bearing = FindRow(Schema, "StrikeBearingLimitDeg");
		if (TestTrue(TEXT("D55: both I-ASN-8 rows are in the schema"), Yaw != nullptr && Bearing != nullptr))
		{
			TestEqual(TEXT("D55: StrikeWarpYawLimitDeg safe minimum = StrikeBearingLimitDeg safe maximum - 60"), static_cast<double>(Yaw->SafeMin), static_cast<double>(Bearing->SafeMax) - 60.0);
		}
	}

	// ---- Negative control: an injected value the AC names moves the verdict (the inputs are really read) ----
	{
		FStealthServerTuning Values = FTuningPipelineCore::BuildSchemaDefaults(Schema);
		FAssassinationInvariantInputs Inputs = GddInputs();
		Inputs.CrowdReplacementMinDelaySec = 9.2; // 1.2 + 6 + 2 = 9.2: equal, so the strict I-ASN-5w warns
		const FAssassinationInvariantReport Report = StealthAssassinationInvariants::Evaluate(Values, Inputs);
		TestTrue(TEXT("negative control: an injected CrowdReplacementMinDelaySec reaches I-ASN-5w"), Report.Warned.Contains(TEXT("I-ASN-5w")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
