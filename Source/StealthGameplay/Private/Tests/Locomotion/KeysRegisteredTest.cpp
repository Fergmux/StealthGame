// Story SCL-003 (production/epics/shared-character-locomotion/story-003-tuning-keys-registered.md), AC-SCL-35
// KeysRegistered: GIVEN the TDC schema, WHEN the CR-15 lint runs over the SCL Tuning Knobs table (parsed fixture data,
// Server/Testing/StealthSclTuningKnobs.h), THEN every key exists with its type, default, hard and safe range, audience,
// latch, owner and unit; the retired InformantMaxSpeedCmPerSec and FootstepStrideWalkCm do not exist; and every SCL
// invariant enforced at preset load (I-SCL-1 to 9, 12, 13, 15, 16) passes the defaults and rejects one violating case.
// I-SCL-10, 11 and 14 are registered as offline rows (harness frame time, spike R3 cost, clip authoring). I-SCL-15 is
// also the boot test StealthGame.Locomotion.GameNetworkManagerPin.
// Edge cases (negative controls): a table key with no schema row, and a key with the wrong audience, each fail the lint.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningDerived.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/Testing/StealthSclTuningKnobs.h"

namespace KeysRegisteredTestData
{
	using StealthSclTuningKnobs::FKnob;

	/** Float columns are compared as the float the C++ initialiser holds, widened (the schema stores float defaults widened). */
	bool SameFloat(double Schema, float Fixture)
	{
		return FMath::IsNearlyEqual(Schema, static_cast<double>(Fixture), 1e-6);
	}

	/** The CR-15 lint over a fixture table against schema rows: one error line per mismatch. */
	TArray<FString> Lint(TConstArrayView<FKnob> Table, TConstArrayView<FTuningSchemaRow> Schema)
	{
		TArray<FString> Errors;
		for (const FKnob& Knob : Table)
		{
			const FTuningSchemaRow* Row = nullptr;
			for (const FTuningSchemaRow& Candidate : Schema)
			{
				if (FCStringAnsi::Strcmp(Candidate.Key, Knob.Key) == 0)
				{
					Row = &Candidate;
					break;
				}
			}
			const FString Key(Knob.Key);
			if (Row == nullptr)
			{
				Errors.Add(FString::Printf(TEXT("%s: no schema row"), *Key));
				continue;
			}
			const uint8 WantType = Knob.bInt ? StealthTuningTypeTag::Int : StealthTuningTypeTag::Float;
			if (Row->TypeTag != WantType) { Errors.Add(FString::Printf(TEXT("%s: type tag %d, want %d"), *Key, Row->TypeTag, WantType)); }
			if (!SameFloat(Row->Default, Knob.Default)) { Errors.Add(FString::Printf(TEXT("%s: default %g, want %g"), *Key, Row->Default, Knob.Default)); }
			if (!SameFloat(Row->HardMin, Knob.HardMin) || !SameFloat(Row->HardMax, Knob.HardMax)) { Errors.Add(FString::Printf(TEXT("%s: hard range [%g, %g], want [%g, %g]"), *Key, Row->HardMin, Row->HardMax, Knob.HardMin, Knob.HardMax)); }
			if (!SameFloat(Row->SafeMin, Knob.SafeMin) || !SameFloat(Row->SafeMax, Knob.SafeMax)) { Errors.Add(FString::Printf(TEXT("%s: safe range [%g, %g], want [%g, %g]"), *Key, Row->SafeMin, Row->SafeMax, Knob.SafeMin, Knob.SafeMax)); }
			if (Row->bPublic != Knob.bPublic) { Errors.Add(FString::Printf(TEXT("%s: audience %s, want %s"), *Key, Row->bPublic ? TEXT("Client") : TEXT("ServerOnly"), Knob.bPublic ? TEXT("Client") : TEXT("ServerOnly"))); }
			if (Row->Latch != Knob.Latch) { Errors.Add(FString::Printf(TEXT("%s: latch %d, want %d"), *Key, static_cast<int32>(Row->Latch), static_cast<int32>(Knob.Latch))); }
			if (Row->Owner == nullptr || FCStringAnsi::Strcmp(Row->Owner, StealthSclTuningKnobs::Owner) != 0) { Errors.Add(FString::Printf(TEXT("%s: owner is not SCL"), *Key)); }
			if (Row->Unit == nullptr || FCStringAnsi::Strcmp(Row->Unit, Knob.Unit) != 0) { Errors.Add(FString::Printf(TEXT("%s: unit differs"), *Key)); }
		}
		// No SCL row exists that the table does not list (one key per Tuning Knobs entry, nothing extra).
		for (const FTuningSchemaRow& Row : Schema)
		{
			if (Row.Owner == nullptr || FCStringAnsi::Strcmp(Row.Owner, StealthSclTuningKnobs::Owner) != 0)
			{
				continue;
			}
			bool bListed = false;
			for (const FKnob& Knob : Table)
			{
				bListed |= FCStringAnsi::Strcmp(Knob.Key, Row.Key) == 0;
			}
			if (!bListed)
			{
				Errors.Add(FString::Printf(TEXT("%s: SCL schema row not in the Tuning Knobs table"), ANSI_TO_TCHAR(Row.Key)));
			}
		}
		return Errors;
	}

	const FTuningInvariant* FindInvariant(TConstArrayView<FTuningInvariant> Rows, const ANSICHAR* Id)
	{
		for (const FTuningInvariant& Row : Rows)
		{
			if (FCStringAnsi::Strcmp(Row.Id, Id) == 0)
			{
				return &Row;
			}
		}
		return nullptr;
	}

	/** One invariant with the mutation that must make it reject. */
	struct FInvariantCase
	{
		const ANSICHAR* Id;
		void (*Break)(FStealthServerTuning&);
	};

	const FInvariantCase Cases[] = {
		{ "I-SCL-1", [](FStealthServerTuning& V) { V.Locomotion_Server.RunSpeedThresholdCmPerSec = 160.0f; } },
		{ "I-SCL-2", [](FStealthServerTuning& V) { V.Locomotion_Public.TraversalMinHeightCm = 50.0f; V.Locomotion_Public.TraversalVaultHeightCm = 60.0f; } },
		{ "I-SCL-3", [](FStealthServerTuning& V) { V.Locomotion_Public.AnimTierAMinScreenPct = 10.0f; V.Locomotion_Public.AnimTierBMinScreenPct = 10.0f; } },
		{ "I-SCL-4", [](FStealthServerTuning& V) { V.Locomotion_Public.WalkStopSec = 0.45f; } },
		{ "I-SCL-5", [](FStealthServerTuning& V) { V.Locomotion_Public.WalkSpeedScaleMinMult = 0.85f; } },
		{ "I-SCL-6", [](FStealthServerTuning& V) { V.Locomotion_Public.WalkSpeedCmPerSec = 125.0f; } },
		{ "I-SCL-7", [](FStealthServerTuning& V) { V.Locomotion_Server.StarvationCatchUpMaxMult = 1.5f; V.Locomotion_Public.WalkSpeedCmPerSec = 200.0f; } },
		{ "I-SCL-8", [](FStealthServerTuning& V) { V.Locomotion_Public.PawnSmoothSnapThresholdCm = 128.0f; V.Locomotion_Public.RunSpeedCmPerSec = 650.0f; V.Locomotion_Server.PawnNetUpdateHz = 12.0f; } },
		{ "I-SCL-9", [](FStealthServerTuning& V) { V.Locomotion_Public.PawnSmoothLocationSec = 0.07f; V.Locomotion_Server.PawnNetUpdateHz = 13.0f; } },
		{ "I-SCL-12", [](FStealthServerTuning& V) { V.Locomotion_Server.InputStarvationSec = 1.0f; V.Locomotion_Public.WalkStopSec = 1.2f; V.Locomotion_Public.WalkSpeedScaleMinMult = 0.6f; } },
		{ "I-SCL-13", [](FStealthServerTuning& V) { V.Locomotion_Public.FootstepCadenceFullPerSec = 1.8f; V.Locomotion_Public.FootstepCadenceSlowPerSec = 1.9f; } },
		{ "I-SCL-15", [](FStealthServerTuning& V) { V.Locomotion_Server.InputStarvationSec = 0.4f; } },
		{ "I-SCL-16", [](FStealthServerTuning& V) { V.Locomotion_Public.StopMinSec = 0.7f; } },
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSclKeysRegisteredTest, "StealthGame.Locomotion.KeysRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSclKeysRegisteredTest::RunTest(const FString& Parameters)
{
	using namespace KeysRegisteredTestData;
	const TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();

	// Every Tuning Knobs key is registered with the table's values.
	const TArray<FString> Errors = Lint(StealthSclTuningKnobs::Knobs, Schema);
	for (const FString& Error : Errors)
	{
		AddError(FString::Printf(TEXT("CR-15 lint: %s"), *Error));
	}
	TestEqual(TEXT("the SCL table has 55 keys (47 Client, 8 ServerOnly)"), static_cast<int32>(UE_ARRAY_COUNT(StealthSclTuningKnobs::Knobs)), 55);

	// Retired keys do not exist.
	for (const ANSICHAR* Retired : StealthSclTuningKnobs::RetiredKeys)
	{
		bool bFound = false;
		for (const FTuningSchemaRow& Row : Schema)
		{
			bFound |= FCStringAnsi::Strcmp(Row.Key, Retired) == 0;
		}
		TestFalse(FString::Printf(TEXT("retired key %s is not in the schema"), ANSI_TO_TCHAR(Retired)), bFound);
	}

	// Negative controls: the lint catches a table key without schema support and a wrong audience.
	{
		TArray<FKnob> Extra(StealthSclTuningKnobs::Knobs, UE_ARRAY_COUNT(StealthSclTuningKnobs::Knobs));
		Extra.Add({ "SclUnregisteredProbeSec", false, 1.0f, 0.0f, 2.0f, 0.5f, 1.5f, true, ETuningLatch::Round, "Sec" });
		const TArray<FString> ExtraErrors = Lint(Extra, Schema);
		TestTrue(TEXT("negative control: a table key with no schema row fails the lint"), ExtraErrors.ContainsByPredicate([](const FString& E) { return E.StartsWith(TEXT("SclUnregisteredProbeSec: no schema row")); }));

		TArray<FKnob> Flipped(StealthSclTuningKnobs::Knobs, UE_ARRAY_COUNT(StealthSclTuningKnobs::Knobs));
		Flipped[0].bPublic = !Flipped[0].bPublic; // a Client key listed as ServerOnly
		Flipped.Last().bPublic = !Flipped.Last().bPublic; // a ServerOnly key listed as Client
		const TArray<FString> FlippedErrors = Lint(Flipped, Schema);
		TestEqual(TEXT("negative control: both audience flips fail the lint"), FlippedErrors.FilterByPredicate([](const FString& E) { return E.Contains(TEXT("audience")); }).Num(), 2);
	}

	// Invariants: the defaults pass every SCL row; each row rejects its violating case (and only the mutated row is asked).
	const TConstArrayView<FTuningInvariant> Rows = GetTuningInvariants();
	const FStealthServerTuning Defaults;
	const FTuningCheckContext Context{ true, FName(TEXT("Default")), Schema };
	for (const FInvariantCase& Case : Cases)
	{
		const FTuningInvariant* Row = FindInvariant(Rows, Case.Id);
		if (!TestNotNull(FString::Printf(TEXT("%s is a preset-load invariant row"), ANSI_TO_TCHAR(Case.Id)), Row))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s owner is SCL"), ANSI_TO_TCHAR(Case.Id)), FString(Row->Owner), FString(TEXT("SCL")));
		TestEqual(FString::Printf(TEXT("%s passes the defaults"), ANSI_TO_TCHAR(Case.Id)), static_cast<int32>(Row->Check(Defaults, FTuningDerived::Compute(Defaults), Context)), static_cast<int32>(ETuningCheck::Pass));
		FStealthServerTuning Broken = Defaults;
		Case.Break(Broken);
		TestEqual(FString::Printf(TEXT("%s rejects its violating case"), ANSI_TO_TCHAR(Case.Id)), static_cast<int32>(Row->Check(Broken, FTuningDerived::Compute(Broken), Context)), static_cast<int32>(ETuningCheck::Reject));
	}

	// The whole table accepts the defaults (no SCL row makes the default preset fail).
	TArray<FTuningInvariantFinding> Findings;
	TestEqual(TEXT("every preset-load invariant passes the schema defaults"),
		static_cast<int32>(FTuningPipelineCore::EvaluateInvariants(Rows, Defaults, FTuningDerived::Compute(Defaults), Context, Findings)), static_cast<int32>(ETuningCheck::Pass));

	// Offline rows are registered, never evaluated at preset load.
	for (const ANSICHAR* Id : { "I-SCL-10", "I-SCL-11", "I-SCL-14" })
	{
		const FTuningInvariant* Offline = FindInvariant(GetTuningOfflineInvariants(), Id);
		TestTrue(FString::Printf(TEXT("%s is an offline row"), ANSI_TO_TCHAR(Id)), Offline != nullptr && Offline->bOffline);
		TestNull(FString::Printf(TEXT("%s is not a preset-load row"), ANSI_TO_TCHAR(Id)), FindInvariant(Rows, Id));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
