// Story tdc-044 (production/epics/tuning-data-config/story-044-schema-complete.md), GDD AC-3a (CR-3, CR-13,
// CR-15): the compiled schema is complete, pinned, mirrored and reflection-clean.
//   - the table is non-empty and this is not a bootstrap build (a STEALTH_TUNING_SCHEMA_BOOTSTRAP=1 build can
//     never pass this test, so it can never pass CI; story tdc-042 relies on this);
//   - every row has a default, hard min/max, audience, latch, owner and unit, accessors, and its default lies
//     inside its hard range (or is its declared sentinel);
//   - keys are unique (case-insensitive too) and end with a CR-15.2 suffix equal to the row's Unit; booleans
//     start with b, enums end in Mode;
//   - the metadata mirrors (ClampMin/Max, SafeMin/Max, Latch, Owner, Unit, audience, type, default) equal the
//     compiled rows field by field, and the checked-in StealthTuningSchema.generated.inl equals the in-memory
//     regeneration (the test never writes a file);
//   - the C++ initialisers (a default FStealthServerTuning and the UStealthTuningData class default object)
//     and the compiled Default column equal the GDD Canonical Tunables Default column (committed constants);
//   - reflection over the preset class, the catalog class, both faces and every sub-struct finds only bool,
//     int32, float and enum keys, no Config property and no Config / per-object-config class flag.
// The Default preset ASSET pin moved to story tdc-056 (the asset does not exist yet; readiness fix logged).
// Negative controls (ADR-0008 section 3): a seeded default outside its hard range, an empty table and a stale
// checked-in text are each fed to the same check functions and detected.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/AnyOf.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningCatalog.h"
#include "Server/StealthTuningData.h"
#include "Server/StealthTuningSchema.h"
#include "Server/Testing/StealthNegativeControl.h"
#include "Server/Testing/StealthSclTuningKnobs.h"
#include "Tuning/StealthTuningSchemaGen.h"
#include "UObject/UnrealType.h"
#include "View/StealthClientTuning.h"

namespace SchemaCompleteTestData
{
	/** CR-15.2 unit or kind suffixes (re-read from design/gdd/tuning-data-config.md CR-15.2, 2026-10-25). */
	const ANSICHAR* const Suffixes[] = {
		"Sec", "Ms", "Minutes", "Cm", "CmPerSec", "PerSec", "PerMin", "Hz", "Deg", "Pct", "Mult", "Frac", "Gain", "Sigma", "Exp",
		"Count", "Size", "Limit", "Depth", "Asset",
	};

	/** The GDD Canonical Tunables Default column, committed (design/gdd/tuning-data-config.md, Canonical Tunables). */
	struct FPin
	{
		const ANSICHAR* Key;
		double Default;
	};
	const FPin GddDefaults[] = {
		{ "AssassinationRangeCm", 200.0 },
		{ "DefenceCooldownSec", 60.0 },
		{ "DefenceDurationSec", 4.0 },
		{ "NpcCount", 50.0 },
		{ "NpcKillStrikeLimit", 3.0 },
		{ "ObjectiveInteractSec", 3.0 },
		{ "TargetRoundMinutes", 12.0 },
		{ "TeamSize", 5.0 },
	};

	bool EndsWith(const FString& Key, const ANSICHAR* Tail)
	{
		return Key.EndsWith(ANSI_TO_TCHAR(Tail), ESearchCase::CaseSensitive);
	}

	/** Row completeness, ranges, names and suffixes over any row view. Failures go to Ctx. */
	void CheckRows(TConstArrayView<FTuningSchemaRow> Rows, FStealthCheckContext& Ctx)
	{
		Ctx.TestTrue(TEXT("the schema table is non-empty (an empty bootstrap table never passes)"), Rows.Num() > 0);
		TSet<FString> SeenLower;
		for (const FTuningSchemaRow& Row : Rows)
		{
			if (!Ctx.TestTrue(TEXT("every row has a key"), Row.Key != nullptr && Row.Key[0] != '\0'))
			{
				continue;
			}
			const FString Key(Row.Key);
			Ctx.TestTrue(FString::Printf(TEXT("%s: unique key (case-insensitive)"), *Key), !SeenLower.Contains(Key.ToLower()));
			SeenLower.Add(Key.ToLower());
			Ctx.TestTrue(FString::Printf(TEXT("%s: Get/Set accessors"), *Key), Row.Get != nullptr && Row.Set != nullptr);
			Ctx.TestTrue(FString::Printf(TEXT("%s: type is bool, int32, float or enum"), *Key),
				Row.TypeTag == StealthTuningTypeTag::Bool || Row.TypeTag == StealthTuningTypeTag::Int || Row.TypeTag == StealthTuningTypeTag::Float || Row.TypeTag == StealthTuningTypeTag::Enum);
			Ctx.TestTrue(FString::Printf(TEXT("%s: latch class is Round, Action, Use or Session"), *Key), Row.Latch <= ETuningLatch::Session);
			Ctx.TestTrue(FString::Printf(TEXT("%s: owner"), *Key), Row.Owner != nullptr && Row.Owner[0] != '\0');
			Ctx.TestTrue(FString::Printf(TEXT("%s: hard min <= hard max"), *Key), Row.HardMin <= Row.HardMax);
			const bool bDefaultIsSentinel = Row.bHasSentinel && Row.Default == Row.Sentinel;
			Ctx.TestTrue(FString::Printf(TEXT("%s: default %g inside the hard range [%g, %g]"), *Key, Row.Default, Row.HardMin, Row.HardMax),
				bDefaultIsSentinel || (Row.Default >= Row.HardMin && Row.Default <= Row.HardMax));

			if (Row.TypeTag == StealthTuningTypeTag::Bool)
			{
				Ctx.TestTrue(FString::Printf(TEXT("%s: a boolean key starts with b"), *Key), Key.Len() > 1 && Key[0] == TEXT('b') && FChar::IsUpper(Key[1]));
				continue;
			}
			if (Row.TypeTag == StealthTuningTypeTag::Enum)
			{
				Ctx.TestTrue(FString::Printf(TEXT("%s: an enum key ends in Mode"), *Key), EndsWith(Key, "Mode"));
				continue;
			}
			const bool bUnitListed = Row.Unit != nullptr && Algo::AnyOf(Suffixes, [&Row](const ANSICHAR* S) { return FCStringAnsi::Strcmp(S, Row.Unit) == 0; });
			Ctx.TestTrue(FString::Printf(TEXT("%s: unit %s is a CR-15.2 suffix"), *Key, Row.Unit != nullptr ? *FString(Row.Unit) : TEXT("<none>")), bUnitListed);
			Ctx.TestTrue(FString::Printf(TEXT("%s: the key ends with its unit suffix"), *Key), Row.Unit != nullptr && EndsWith(Key, Row.Unit) && Key.Len() > FCStringAnsi::Strlen(Row.Unit));
		}
	}

	/** The checked-in text must equal the in-memory regeneration. */
	void CheckText(const FString& CheckedIn, const FString& Generated, FStealthCheckContext& Ctx)
	{
		Ctx.TestTrue(TEXT("the checked-in StealthTuningSchema.generated.inl equals the in-memory regeneration (run Stealth.Tuning.SchemaGen)"),
			CheckedIn.Equals(Generated, ESearchCase::CaseSensitive));
	}

	/** Reads a key's value (widened to double) from Container through reflection: OwnerMember then Key. */
	bool ReadReflected(const UStruct* Type, const void* Container, const FString& OwnerMember, const FString& Key, double& OutValue)
	{
		const FStructProperty* Owner = CastField<FStructProperty>(Type->FindPropertyByName(FName(*OwnerMember)));
		if (Owner == nullptr)
		{
			return false;
		}
		const void* OwnerValue = Owner->ContainerPtrToValuePtr<void>(Container);
		const FProperty* KeyProp = Owner->Struct->FindPropertyByName(FName(*Key));
		if (const FBoolProperty* Bool = CastField<FBoolProperty>(KeyProp))
		{
			OutValue = Bool->GetPropertyValue_InContainer(OwnerValue) ? 1.0 : 0.0;
			return true;
		}
		if (const FIntProperty* Int = CastField<FIntProperty>(KeyProp))
		{
			OutValue = Int->GetPropertyValue_InContainer(OwnerValue);
			return true;
		}
		if (const FFloatProperty* Float = CastField<FFloatProperty>(KeyProp))
		{
			OutValue = Float->GetPropertyValue_InContainer(OwnerValue);
			return true;
		}
		return false;
	}

	/** True for an allowed key property: bool, int32, float or enum. */
	bool IsAllowedKeyType(const FProperty* Prop)
	{
		if (Prop->IsA<FBoolProperty>() || Prop->IsA<FIntProperty>() || Prop->IsA<FFloatProperty>() || Prop->IsA<FEnumProperty>())
		{
			return true;
		}
		const FByteProperty* Byte = CastField<FByteProperty>(Prop);
		return Byte != nullptr && Byte->Enum != nullptr;
	}

	/** Walks a face or preset type: every own member is a tuning sub-struct of allowed keys; no Config property anywhere. */
	void CheckReflection(const UStruct* Type, FStealthCheckContext& Ctx)
	{
		const FString TypeName = Type->GetName();
		for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			const FProperty* Member = *It;
			Ctx.TestFalse(FString::Printf(TEXT("%s::%s has no Config flag (CR-13)"), *TypeName, *Member->GetName()), Member->HasAnyPropertyFlags(CPF_Config | CPF_GlobalConfig));
			const FStructProperty* Owner = CastField<FStructProperty>(Member);
			if (!Ctx.TestTrue(FString::Printf(TEXT("%s::%s is a tuning sub-struct"), *TypeName, *Member->GetName()), Owner != nullptr))
			{
				continue;
			}
			for (TFieldIterator<FProperty> KeyIt(Owner->Struct); KeyIt; ++KeyIt)
			{
				const FProperty* Key = *KeyIt;
				Ctx.TestTrue(FString::Printf(TEXT("%s.%s is bool, int32, float or enum (found %s)"), *Owner->Struct->GetName(), *Key->GetName(), *Key->GetCPPType()), IsAllowedKeyType(Key));
				Ctx.TestFalse(FString::Printf(TEXT("%s.%s has no Config flag (CR-13)"), *Owner->Struct->GetName(), *Key->GetName()), Key->HasAnyPropertyFlags(CPF_Config | CPF_GlobalConfig));
			}
		}
	}

	/** No Config / per-object-config class flag on a tuning class, and no Config property on its own members. */
	void CheckClassFlags(const UClass* Class, FStealthCheckContext& Ctx)
	{
		Ctx.TestFalse(FString::Printf(TEXT("%s has no Config / PerObjectConfig / DefaultConfig / GlobalUserConfig / ProjectUserConfig class flag"), *Class->GetName()),
			Class->HasAnyClassFlags(CLASS_Config | CLASS_PerObjectConfig | CLASS_DefaultConfig | CLASS_GlobalUserConfig | CLASS_ProjectUserConfig));
		for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			Ctx.TestFalse(FString::Printf(TEXT("%s::%s has no Config flag"), *Class->GetName(), *It->GetName()), It->HasAnyPropertyFlags(CPF_Config | CPF_GlobalConfig));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTuningSchemaCompleteTest, "StealthGame.Tuning.SchemaComplete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningSchemaCompleteTest::RunTest(const FString& Parameters)
{
	using namespace SchemaCompleteTestData;
	using namespace StealthTuningSchemaGen;

	const auto Forward = [this](const FStealthCheckContext& Ctx)
	{
		for (const FString& Failure : Ctx.GetFailures())
		{
			AddError(Failure);
		}
	};

	const TConstArrayView<FTuningSchemaRow> Compiled = GetTuningSchema();
	TestFalse(TEXT("this build did not compile the empty bootstrap table (STEALTH_TUNING_SCHEMA_BOOTSTRAP=1)"), IsTuningSchemaBootstrapBuild());

	// Rows: complete, defaults inside hard ranges, unique, CR-15 suffixes.
	{
		FStealthCheckContext Ctx;
		CheckRows(Compiled, Ctx);
		Forward(Ctx);
	}

	// Metadata mirrors equal the compiled rows; the checked-in text equals the regeneration.
	TArray<FGenRow> Reflected;
	TArray<FString> Errors;
	if (!TestTrue(TEXT("the generator reflects the schema"), ReflectRows(Reflected, Errors)))
	{
		for (const FString& Error : Errors)
		{
			AddError(Error);
		}
		return false;
	}
	if (TestEqual(TEXT("compiled and reflected row counts"), Compiled.Num(), Reflected.Num()))
	{
		for (int32 Index = 0; Index < Reflected.Num(); ++Index)
		{
			const FGenRow& Mirror = Reflected[Index];
			const FTuningSchemaRow& Row = Compiled[Index];
			const FString& Key = Mirror.Key;
			TestEqual(FString::Printf(TEXT("row %d key"), Index), FString(Row.Key), Key);
			TestEqual(FString::Printf(TEXT("%s type"), *Key), static_cast<int32>(Row.TypeTag), static_cast<int32>(Mirror.TypeTag));
			TestTrue(FString::Printf(TEXT("%s default mirror"), *Key), Row.Default == Mirror.Default);
			TestTrue(FString::Printf(TEXT("%s ClampMin mirror"), *Key), Row.HardMin == Mirror.HardMin);
			TestTrue(FString::Printf(TEXT("%s ClampMax mirror"), *Key), Row.HardMax == Mirror.HardMax);
			TestTrue(FString::Printf(TEXT("%s SafeMin mirror"), *Key), Row.SafeMin == Mirror.SafeMin);
			TestTrue(FString::Printf(TEXT("%s SafeMax mirror"), *Key), Row.SafeMax == Mirror.SafeMax);
			TestTrue(FString::Printf(TEXT("%s audience mirror (_Public struct)"), *Key), Row.bPublic == Mirror.bPublic);
			TestEqual(FString::Printf(TEXT("%s Latch mirror"), *Key), static_cast<int32>(Row.Latch), static_cast<int32>(Mirror.Latch));
			TestEqual(FString::Printf(TEXT("%s Owner mirror"), *Key), FString(Row.Owner), Mirror.Owner);
			TestEqual(FString::Printf(TEXT("%s Unit mirror"), *Key), FString(Row.Unit), Mirror.Unit);
			TestTrue(FString::Printf(TEXT("%s sentinel mirror"), *Key), Row.bHasSentinel == Mirror.bHasSentinel && (!Row.bHasSentinel || Row.Sentinel == Mirror.Sentinel));
		}
	}
	TestEqual(TEXT("compiled fingerprint equals the reflected one"), GetTuningSchemaFingerprint(), Fingerprint(Reflected));
	FString CheckedIn;
	const FString Generated = RenderTable(Reflected);
	if (TestTrue(TEXT("the checked-in table exists"), LoadCheckedInTable(CheckedIn)))
	{
		FStealthCheckContext Ctx;
		CheckText(CheckedIn, Generated, Ctx);
		Forward(Ctx);
	}

	// Default pin: compiled Default column, C++ initialisers of both faces' owner (server face) and of the
	// preset class default object, each against the committed GDD Default column; every schema key is pinned.
	const FStealthServerTuning Initialisers;
	const UStealthTuningData* PresetCdo = GetDefault<UStealthTuningData>();
	// The pins: the TDC Canonical Tunables plus every owner table registered so far (SCL Tuning Knobs, story SCL-003).
	TArray<FPin> Pins(GddDefaults, UE_ARRAY_COUNT(GddDefaults));
	for (const StealthSclTuningKnobs::FKnob& Knob : StealthSclTuningKnobs::Knobs)
	{
		Pins.Add({ Knob.Key, static_cast<double>(Knob.Default) });
	}
	TestEqual(TEXT("every schema key has a GDD pin and every pin a schema key"), Pins.Num(), Compiled.Num());
	for (const FPin& Pin : Pins)
	{
		const FString Key(Pin.Key);
		const FTuningSchemaRow* Row = Compiled.FindByPredicate([&Pin](const FTuningSchemaRow& R) { return FCStringAnsi::Strcmp(R.Key, Pin.Key) == 0; });
		const FGenRow* Mirror = Reflected.FindByPredicate([&Key](const FGenRow& R) { return R.Key == Key; });
		if (!TestNotNull(FString::Printf(TEXT("%s is a schema row"), *Key), Row) || !TestNotNull(FString::Printf(TEXT("%s is reflected"), *Key), Mirror))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s compiled default equals the GDD Default column"), *Key), Row->Default, Pin.Default);
		TestEqual(FString::Printf(TEXT("%s C++ initialiser (server face) equals the GDD Default column"), *Key), Row->Get(Initialisers), Pin.Default);
		double CdoValue = 0.0;
		if (TestTrue(FString::Printf(TEXT("%s is readable on the UStealthTuningData CDO"), *Key), ReadReflected(UStealthTuningData::StaticClass(), PresetCdo, Mirror->OwnerMember, Key, CdoValue)))
		{
			TestEqual(FString::Printf(TEXT("%s C++ initialiser (preset class) equals the GDD Default column"), *Key), CdoValue, Pin.Default);
		}
	}

	// Reflection hygiene: allowed types only, no Config property, no Config class flag.
	{
		FStealthCheckContext Ctx;
		CheckReflection(FStealthServerTuning::StaticStruct(), Ctx);
		CheckReflection(FStealthClientTuning::StaticStruct(), Ctx);
		CheckReflection(UStealthTuningData::StaticClass(), Ctx);
		CheckClassFlags(UStealthTuningData::StaticClass(), Ctx);
		CheckClassFlags(UStealthTuningCatalog::StaticClass(), Ctx);
		Forward(Ctx);
	}

	// Negative controls: each seeded violation is detected by the same check.
	{
		TArray<FTuningSchemaRow> Seeded(Compiled.GetData(), Compiled.Num());
		if (Seeded.Num() > 0)
		{
			Seeded[0].Default = Seeded[0].HardMax + 1.0;
		}
		FStealthNegativeControl Control(TEXT("DefaultOutsideHardRange"));
		FStealthNegativeControl::Report(*this, Control.Run([&Seeded](FStealthCheckContext& Ctx) { CheckRows(Seeded, Ctx); }));
	}
	{
		FStealthNegativeControl Control(TEXT("EmptyBootstrapTable"));
		FStealthNegativeControl::Report(*this, Control.Run([](FStealthCheckContext& Ctx) { CheckRows(TConstArrayView<FTuningSchemaRow>(), Ctx); }));
	}
	{
		FStealthNegativeControl Control(TEXT("StaleCheckedInTable"));
		const FString Stale = Generated + TEXT("// drift");
		FStealthNegativeControl::Report(*this, Control.Run([&Stale, &Generated](FStealthCheckContext& Ctx) { CheckText(Stale, Generated, Ctx); }));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
