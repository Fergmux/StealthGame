// Story tdc-039 (production/epics/tuning-data-config/story-039-preset-asset-classes.md): the preset and
// catalog classes (ADR-0004 section 1, CR-2, CR-13). Reflection finds no Config/DefaultConfig/
// GlobalUserConfig/PerObjectConfig class flag and no CPF_Config property on UStealthTuningData,
// UStealthTuningCatalog or any tuning struct reachable from them (walked, not listed, so new sub-structs
// are picked up); GetPrimaryAssetId() reports StealthTuningPreset / StealthTuningCatalog; and the catalog
// check rejects bTestAllowsStrikeLimitOne on Default and on any HostSelectable entry, accepts it on a
// non-selectable non-Default entry, and requires Default.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningCatalog.h"
#include "Server/StealthTuningData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "View/StealthClientTuning.h"

namespace PresetClassesHygieneTestData
{
	constexpr EClassFlags ForbiddenClassFlags = CLASS_Config | CLASS_DefaultConfig | CLASS_GlobalUserConfig | CLASS_PerObjectConfig;

	/** Collects every property with CPF_Config under Struct (recursing into struct, array and soft-pointer-free members). */
	void FindConfigProperties(const UStruct* Struct, TSet<const UStruct*>& Visited, TArray<FString>& OutFindings, int32& OutStructCount)
	{
		if (Struct == nullptr || Visited.Contains(Struct))
		{
			return;
		}
		Visited.Add(Struct);
		++OutStructCount;
		for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Config | CPF_GlobalConfig))
			{
				OutFindings.Add(FString::Printf(TEXT("%s::%s carries a Config flag"), *Struct->GetName(), *It->GetName()));
			}
			const FProperty* Inner = *It;
			if (const FArrayProperty* Array = CastField<FArrayProperty>(Inner))
			{
				Inner = Array->Inner;
			}
			if (const FStructProperty* StructProp = CastField<FStructProperty>(Inner))
			{
				FindConfigProperties(StructProp->Struct, Visited, OutFindings, OutStructCount);
			}
		}
	}

	FStealthTuningCatalogEntry Entry(const TCHAR* Id, bool bHostSelectable, bool bStrikeLimitOne)
	{
		FStealthTuningCatalogEntry Out;
		Out.PresetId = FName(Id);
		Out.bHostSelectable = bHostSelectable;
		Out.bTestAllowsStrikeLimitOne = bStrikeLimitOne;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTuningPresetClassesHygieneTest, "StealthGame.Tuning.PresetClassesHygiene",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningPresetClassesHygieneTest::RunTest(const FString& Parameters)
{
	using namespace PresetClassesHygieneTestData;

	// No Config back door on the classes or any tuning struct reachable from them or the faces.
	for (const UClass* Class : { UStealthTuningData::StaticClass(), UStealthTuningCatalog::StaticClass() })
	{
		TestFalse(FString::Printf(TEXT("%s has no Config/DefaultConfig/GlobalUserConfig/PerObjectConfig flag"), *Class->GetName()), Class->HasAnyClassFlags(ForbiddenClassFlags));
		TestTrue(FString::Printf(TEXT("%s is a UPrimaryDataAsset"), *Class->GetName()), Class->IsChildOf(UPrimaryDataAsset::StaticClass()));
	}
	{
		TSet<const UStruct*> Visited;
		TArray<FString> Findings;
		int32 StructCount = 0;
		FindConfigProperties(UStealthTuningData::StaticClass(), Visited, Findings, StructCount);
		FindConfigProperties(UStealthTuningCatalog::StaticClass(), Visited, Findings, StructCount);
		FindConfigProperties(FStealthServerTuning::StaticStruct(), Visited, Findings, StructCount);
		FindConfigProperties(FStealthClientTuning::StaticStruct(), Visited, Findings, StructCount);
		for (const FString& Finding : Findings)
		{
			AddError(Finding);
		}
		// Preset + catalog + entry + the two faces + six sub-structs at least (the walk reaches the sub-structs).
		TestTrue(FString::Printf(TEXT("the walk reached the tuning sub-structs (%d structs)"), StructCount), StructCount >= 9);
	}

	// Primary asset ids.
	{
		UStealthTuningData* Preset = NewObject<UStealthTuningData>(GetTransientPackage(), TEXT("DA_TuningHygieneFixture"));
		UStealthTuningCatalog* Catalog = NewObject<UStealthTuningCatalog>(GetTransientPackage(), TEXT("DA_TuningCatalogHygieneFixture"));
		TestEqual(TEXT("preset primary asset type"), Preset->GetPrimaryAssetId().PrimaryAssetType.GetName(), FName(TEXT("StealthTuningPreset")));
		TestEqual(TEXT("preset primary asset name"), Preset->GetPrimaryAssetId().PrimaryAssetName, FName(TEXT("DA_TuningHygieneFixture")));
		TestEqual(TEXT("catalog primary asset type"), Catalog->GetPrimaryAssetId().PrimaryAssetType.GetName(), FName(TEXT("StealthTuningCatalog")));
		TestEqual(TEXT("catalog primary asset name"), Catalog->GetPrimaryAssetId().PrimaryAssetName, FName(TEXT("DA_TuningCatalogHygieneFixture")));
		Preset->MarkAsGarbage();
		Catalog->MarkAsGarbage();
	}

	// The catalog check.
	{
		TArray<FString> Problems;
		const FStealthTuningCatalogEntry Clean[] = { Entry(TEXT("Default"), false, false), Entry(TEXT("Playtest"), true, false), Entry(TEXT("Solo"), false, false) };
		TestTrue(TEXT("a clean catalog passes"), UStealthTuningCatalog::CheckEntries(Clean, Problems));
		TestEqual(TEXT("a clean catalog reports nothing"), Problems.Num(), 0);

		const FStealthTuningCatalogEntry FlagOnTest[] = { Entry(TEXT("Default"), false, false), Entry(TEXT("StrikeOne"), false, true) };
		Problems.Reset();
		TestTrue(TEXT("the flag on a non-selectable non-Default entry is accepted"), UStealthTuningCatalog::CheckEntries(FlagOnTest, Problems));

		const FStealthTuningCatalogEntry FlagOnDefault[] = { Entry(TEXT("Default"), false, true) };
		Problems.Reset();
		TestFalse(TEXT("the flag on Default is rejected"), UStealthTuningCatalog::CheckEntries(FlagOnDefault, Problems));
		TestEqual(TEXT("the flag on Default: one problem"), Problems.Num(), 1);

		const FStealthTuningCatalogEntry FlagOnSelectable[] = { Entry(TEXT("Default"), false, false), Entry(TEXT("Playtest"), true, true) };
		Problems.Reset();
		TestFalse(TEXT("the flag on a HostSelectable entry is rejected"), UStealthTuningCatalog::CheckEntries(FlagOnSelectable, Problems));
		TestEqual(TEXT("the flag on a HostSelectable entry: one problem"), Problems.Num(), 1);

		const FStealthTuningCatalogEntry FlagOnBoth[] = { Entry(TEXT("Default"), true, true), Entry(TEXT("Playtest"), true, true) };
		Problems.Reset();
		TestFalse(TEXT("the flag on Default and a selectable entry is rejected"), UStealthTuningCatalog::CheckEntries(FlagOnBoth, Problems));
		TestEqual(TEXT("the flag on both: one problem per rule and entry"), Problems.Num(), 3);

		const FStealthTuningCatalogEntry NoDefault[] = { Entry(TEXT("Playtest"), true, false) };
		Problems.Reset();
		TestFalse(TEXT("a catalog without Default is rejected"), UStealthTuningCatalog::CheckEntries(NoDefault, Problems));

		Problems.Reset();
		TestFalse(TEXT("an empty catalog is rejected (Default is mandatory)"), UStealthTuningCatalog::CheckEntries(TConstArrayView<FStealthTuningCatalogEntry>(), Problems));
		TestEqual(TEXT("an empty catalog: one problem"), Problems.Num(), 1);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
