// Story cnb-003 (production/epics/crowd-npc-behaviour/story-003-crowd-tuning-keys-and-profile-data.md): the Crowd
// tuning keys as data (GDD Tuning Knobs, CR-15, OQ13, F2; ADR-0016 sections 3, 4.5, 9; ADR-0004).
//   KeysRegistered   every Crowd row of the committed knob table (GDD rows + OQ13 constants) is a ServerOnly schema row
//                    with its type, default, hard and safe range, latch, owner and unit, and no Crowd schema row is
//                    unlisted; an out-of-range value never reaches the frozen set (F-1 clamps it to the bound).
//   Invariants       I-CNB-1 to I-CNB-6 pass the defaults and each rejects its violating case (TDC GDD Formulas).
//   ServerOnlyHashed the Baseline / Tell Matrix intervals are in TuningHash, not in ClientHash, not on the client face;
//                    the Crowd round hash moves with a rate and with an asset key.
//   AssetKeys        a null, missing or wrong-class asset key fails validation (the default assets exist from stories
//                    cnb-010 / cnb-012 on; this story checks the class-and-path mechanism).
// Negative controls: a table key with no schema row and a flipped audience fail the lint.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/CrowdAssetKeys.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningDerived.h"
#include "Server/StealthTuningHasher.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/StealthTuningVersion.h"
#include "Server/Testing/StealthCrowdTuningKnobs.h"
#include "UObject/UnrealType.h"
#include "View/StealthClientTuning.h"

namespace CrowdTuningSmokeTestData
{
	using StealthCrowdTuningKnobs::FKnob;

	bool SameFloat(double Schema, float Fixture)
	{
		return FMath::IsNearlyEqual(Schema, static_cast<double>(Fixture), 1e-6);
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

	/** The CR-15 lint of a knob table against schema rows: one line per mismatch, plus unlisted Crowd rows. */
	TArray<FString> Lint(TConstArrayView<FKnob> Table, TConstArrayView<FTuningSchemaRow> Schema)
	{
		TArray<FString> Errors;
		for (const FKnob& Knob : Table)
		{
			const FString Key(Knob.Key);
			const FTuningSchemaRow* Row = FindRow(Schema, Knob.Key);
			if (Row == nullptr)
			{
				Errors.Add(FString::Printf(TEXT("%s: no schema row"), *Key));
				continue;
			}
			const uint8 WantType = Knob.bInt ? StealthTuningTypeTag::Int : StealthTuningTypeTag::Float;
			if (Row->TypeTag != WantType) { Errors.Add(FString::Printf(TEXT("%s: type tag %d, want %d"), *Key, Row->TypeTag, WantType)); }
			if (!SameFloat(Row->Default, Knob.Default)) { Errors.Add(FString::Printf(TEXT("%s: default %g, want %g"), *Key, Row->Default, Knob.Default)); }
			if (!SameFloat(Row->HardMin, Knob.HardMin) || !SameFloat(Row->HardMax, Knob.HardMax)) { Errors.Add(FString::Printf(TEXT("%s: hard range"), *Key)); }
			if (!SameFloat(Row->SafeMin, Knob.SafeMin) || !SameFloat(Row->SafeMax, Knob.SafeMax)) { Errors.Add(FString::Printf(TEXT("%s: safe range"), *Key)); }
			if (Row->bPublic != Knob.bPublic) { Errors.Add(FString::Printf(TEXT("%s: audience"), *Key)); }
			if (Row->Latch != Knob.Latch) { Errors.Add(FString::Printf(TEXT("%s: latch %d, want %d"), *Key, static_cast<int32>(Row->Latch), static_cast<int32>(Knob.Latch))); }
			if (Row->Owner == nullptr || FCStringAnsi::Strcmp(Row->Owner, StealthCrowdTuningKnobs::Owner) != 0) { Errors.Add(FString::Printf(TEXT("%s: owner is not Crowd"), *Key)); }
			if (Row->Unit == nullptr || FCStringAnsi::Strcmp(Row->Unit, Knob.Unit) != 0) { Errors.Add(FString::Printf(TEXT("%s: unit differs"), *Key)); }
		}
		for (const FTuningSchemaRow& Row : Schema)
		{
			if (Row.Owner == nullptr || FCStringAnsi::Strcmp(Row.Owner, StealthCrowdTuningKnobs::Owner) != 0)
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
				Errors.Add(FString::Printf(TEXT("%s: Crowd schema row not in the knob table"), ANSI_TO_TCHAR(Row.Key)));
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

	struct FInvariantCase
	{
		const ANSICHAR* Id;
		void (*Break)(FStealthServerTuning&);
	};

	/** One violating case per I-CNB row (the other keys at their defaults). */
	const FInvariantCase Cases[] = {
		{ "I-CNB-1", [](FStealthServerTuning& V) { V.Crowd_Server.CrowdIdleMinSec = 3.5f; } },                // < SessionSoftTimeoutSec 3 + 1
		{ "I-CNB-2", [](FStealthServerTuning& V) { V.Crowd_Server.CrowdConversationMaxSec = 25.0f; } },      // > CrowdMaxIdleSec 20
		{ "I-CNB-3", [](FStealthServerTuning& V) { V.Crowd_Server.CrowdAmbientCycleIntervalSec = 95.0f; } }, // 0.63 * 0.75 < 0.5
		{ "I-CNB-4", [](FStealthServerTuning& V) { V.Crowd_Server.CrowdAmbientCycleIntervalSec = 30.0f; } }, // 4 events > 3 per minute
		{ "I-CNB-5", [](FStealthServerTuning& V) { V.Crowd_Server.CrowdTellChainGapSec = 50.0f; } },         // (0.68) * 50 / 60 > 0.5
		{ "I-CNB-6", [](FStealthServerTuning& V) { V.Crowd_Server.CrowdDecoyEarlyReleaseMaxSec = 3.0f; } },  // not < ObjectiveInteractSec 3
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTuningKeysRegisteredTest, "StealthGame.Crowd.Tuning.KeysRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTuningKeysRegisteredTest::RunTest(const FString& Parameters)
{
	using namespace CrowdTuningSmokeTestData;
	const TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();
	if (!TestTrue(TEXT("the generated schema has rows (not a bootstrap build)"), Schema.Num() > 0))
	{
		return false;
	}
	const TConstArrayView<FKnob> Table(StealthCrowdTuningKnobs::Knobs, UE_ARRAY_COUNT(StealthCrowdTuningKnobs::Knobs));
	const TArray<FString> Errors = Lint(Table, Schema);
	for (const FString& Error : Errors)
	{
		AddError(Error);
	}
	AddInfo(FString::Printf(TEXT("CROWDKEYS %d Crowd rows linted (GDD Tuning Knobs + OQ13 constants)"), Table.Num()));
	TestTrue(TEXT("the knob table holds the GDD rows and the OQ13 constants"), Table.Num() >= 100);

	// Negative controls: an unregistered key and a flipped audience fail the lint.
	{
		TArray<FKnob> Extra(Table.GetData(), Table.Num());
		Extra.Add({ "CrowdUnregisteredProbeSec", false, 1.0f, 0.0f, 2.0f, 0.5f, 1.5f, false, ETuningLatch::Use, "Sec" });
		TestTrue(TEXT("negative control: a key with no schema row fails"), Lint(Extra, Schema).ContainsByPredicate([](const FString& E) { return E.StartsWith(TEXT("CrowdUnregisteredProbeSec: no schema row")); }));
		TArray<FKnob> Flipped(Table.GetData(), Table.Num());
		Flipped.Last().bPublic = true;
		TestTrue(TEXT("negative control: a Client audience fails"), Lint(Flipped, Schema).ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("audience")); }));
	}

	// Out of range never reaches the frozen set: F-1 clamps to the key's own bound and records the change.
	const FTuningSchemaRow* Yield = FindRow(Schema, "CrowdYieldPct");
	if (TestNotNull(TEXT("CrowdYieldPct row"), Yield))
	{
		ETuningF1Action Action = ETuningF1Action::Kept;
		const double Effective = FTuningPipelineCore::ApplyF1(*Yield, 150.0, FTuningF1Options(), Action);
		TestEqual(TEXT("CrowdYieldPct 150 is clamped"), static_cast<int32>(Action), static_cast<int32>(ETuningF1Action::Clamped));
		TestEqual(TEXT("CrowdYieldPct 150 becomes 100"), Effective, 100.0);
	}
	const FTuningSchemaRow* Starts = FindRow(Schema, "CrowdPathStartsPerFrameLimit");
	if (TestNotNull(TEXT("CrowdPathStartsPerFrameLimit row"), Starts))
	{
		ETuningF1Action Action = ETuningF1Action::Kept;
		TestEqual(TEXT("CrowdPathStartsPerFrameLimit 0 is clamped to 1"), FTuningPipelineCore::ApplyF1(*Starts, 0.0, FTuningF1Options(), Action), 1.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTuningInvariantsTest, "StealthGame.Crowd.Tuning.Invariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTuningInvariantsTest::RunTest(const FString& Parameters)
{
	using namespace CrowdTuningSmokeTestData;
	const TConstArrayView<FTuningInvariant> Rows = GetTuningInvariants();
	const FStealthServerTuning Defaults;
	const FTuningCheckContext Context{ true, FName(TEXT("Default")), GetTuningSchema() };
	for (const FInvariantCase& Case : Cases)
	{
		const FTuningInvariant* Row = FindInvariant(Rows, Case.Id);
		if (!TestNotNull(FString::Printf(TEXT("%s is a preset-load row"), ANSI_TO_TCHAR(Case.Id)), Row))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s owner"), ANSI_TO_TCHAR(Case.Id)), FString(Row->Owner), FString(TEXT("Crowd")));
		TestEqual(FString::Printf(TEXT("%s passes the defaults"), ANSI_TO_TCHAR(Case.Id)), static_cast<int32>(Row->Check(Defaults, FTuningDerived::Compute(Defaults), Context)), static_cast<int32>(ETuningCheck::Pass));
		FStealthServerTuning Broken = Defaults;
		Case.Break(Broken);
		TestEqual(FString::Printf(TEXT("%s rejects its violating case"), ANSI_TO_TCHAR(Case.Id)), static_cast<int32>(Row->Check(Broken, FTuningDerived::Compute(Broken), Context)), static_cast<int32>(ETuningCheck::Reject));
	}
	// The I-5 range pairs cover the Crowd Min/Max keys: IdleMin above IdleMax is rejected by I-5.
	{
		FStealthServerTuning Swapped = Defaults;
		Swapped.Crowd_Server.CrowdIdleMinSec = 17.0f;
		TArray<FTuningInvariantFinding> Findings;
		TestEqual(TEXT("CrowdIdleMinSec > CrowdIdleMaxSec is rejected (I-5)"), static_cast<int32>(FTuningPipelineCore::EvaluateInvariants(Rows, Swapped, FTuningDerived::Compute(Swapped), Context, Findings)), static_cast<int32>(ETuningCheck::Reject));
	}
	TArray<FTuningInvariantFinding> Findings;
	TestEqual(TEXT("every preset-load invariant passes the schema defaults"), static_cast<int32>(FTuningPipelineCore::EvaluateInvariants(Rows, Defaults, FTuningDerived::Compute(Defaults), Context, Findings)), static_cast<int32>(ETuningCheck::Pass));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTuningServerOnlyHashedTest, "StealthGame.Crowd.Tuning.ServerOnlyHashed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTuningServerOnlyHashedTest::RunTest(const FString& Parameters)
{
	const FStealthServerTuning Defaults;
	FStealthServerTuning Changed = Defaults;
	Changed.Crowd_Server.CrowdRunIntervalSec = 420.0f;
	TestNotEqual(TEXT("a changed Crowd rate changes TuningHash"), FTuningHasher::Hash(Defaults, TuningSchemaVersion, false), FTuningHasher::Hash(Changed, TuningSchemaVersion, false));
	TestEqual(TEXT("a changed Crowd rate leaves ClientHash unchanged (ServerOnly)"), FTuningHasher::Hash(Defaults, TuningSchemaVersion, true), FTuningHasher::Hash(Changed, TuningSchemaVersion, true));

	// The client face names no Crowd key (structural audience).
	bool bClientHasCrowd = false;
	for (TFieldIterator<FProperty> It(FStealthClientTuning::StaticStruct(), EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		bClientHasCrowd |= It->GetName().Contains(TEXT("Crowd"));
	}
	TestFalse(TEXT("FStealthClientTuning has no Crowd member"), bClientHasCrowd);
	for (const FTuningSchemaRow& Row : GetTuningSchema())
	{
		if (Row.Owner != nullptr && FCStringAnsi::Strcmp(Row.Owner, "Crowd") == 0 && Row.bPublic)
		{
			AddError(FString::Printf(TEXT("%s is a Client key"), ANSI_TO_TCHAR(Row.Key)));
		}
	}

	// The Crowd round hash: moves with a rate and with an asset key.
	const FCrowdAssetKeys Keys;
	FCrowdAssetKeys OtherKeys;
	OtherKeys.CrowdMotionProfileAsset = FSoftObjectPath(TEXT("/Game/StealthGame/AI/DA_CrowdMotionProfile_Other.DA_CrowdMotionProfile_Other"));
	const uint64 Base = StealthCrowdTuning::ComputeRoundTuningHash(Defaults, Keys, TuningSchemaVersion);
	TestEqual(TEXT("the round hash is deterministic"), Base, StealthCrowdTuning::ComputeRoundTuningHash(Defaults, Keys, TuningSchemaVersion));
	TestNotEqual(TEXT("the round hash moves with a Crowd rate"), Base, StealthCrowdTuning::ComputeRoundTuningHash(Changed, Keys, TuningSchemaVersion));
	TestNotEqual(TEXT("the round hash moves with an asset key"), Base, StealthCrowdTuning::ComputeRoundTuningHash(Defaults, OtherKeys, TuningSchemaVersion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTuningAssetKeysTest, "StealthGame.Crowd.Tuning.AssetKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTuningAssetKeysTest::RunTest(const FString& Parameters)
{
	// Null and missing paths.
	TestEqual(TEXT("a null key is Missing"), static_cast<int32>(StealthCrowdTuning::CheckAsset(FSoftObjectPath(), StealthCrowdTuning::MotionProfileClassPath(), false)), static_cast<int32>(ECrowdAssetCheck::Missing));
	TestEqual(TEXT("a path with no asset is Missing"), static_cast<int32>(StealthCrowdTuning::CheckAsset(FSoftObjectPath(TEXT("/Game/StealthGame/AI/DA_NoSuchProfile.DA_NoSuchProfile")), StealthCrowdTuning::MotionProfileClassPath(), false)), static_cast<int32>(ECrowdAssetCheck::Missing));
	// Wrong class: an engine static mesh under the motion profile and StateTree keys.
	const FSoftObjectPath Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestEqual(TEXT("a static mesh is the wrong class for the motion profile key"), static_cast<int32>(StealthCrowdTuning::CheckAsset(Cube, StealthCrowdTuning::MotionProfileClassPath(), false)), static_cast<int32>(ECrowdAssetCheck::WrongClass));
	TestEqual(TEXT("a static mesh is the wrong class for a StateTree key"), static_cast<int32>(StealthCrowdTuning::CheckAsset(Cube, StealthCrowdTuning::StateTreeClassPath(), true)), static_cast<int32>(ECrowdAssetCheck::WrongClass));
	FCrowdAssetKeys Broken;
	Broken.CrowdFullStateTreeAsset = Cube;
	TArray<FString> Problems;
	TestFalse(TEXT("ValidateAssetKeys fails with a wrong-class key"), StealthCrowdTuning::ValidateAssetKeys(Broken, &Problems));
	TestTrue(TEXT("the problem names the key"), Problems.ContainsByPredicate([](const FString& P) { return P.StartsWith(TEXT("CrowdFullStateTreeAsset")); }));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
