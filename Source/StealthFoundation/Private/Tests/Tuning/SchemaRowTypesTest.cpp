// Story tdc-038 (production/epics/tuning-data-config/story-038-schema-row-types.md): the schema row
// vocabulary (ADR-0004 sections 2 and 8). FTuningSchemaRow carries Key, TypeTag, Get/Set, Default, the hard
// and safe ranges, bPublic, Latch, Owner and the sentinel; the type tags are bool 0, int 1, float 2, enum 3
// (SoftRef 4, ADR-0004 amendment 2026-10-25b); ETuningLatch is {Round, Action, Use, Session};
// GetTuningSchema() exists; and StealthTuningConstants.h defines exactly the six schema constants of
// Amendments 2, each with its GDD reference and none a preset field. "Exactly" is checked twice: the
// constants list against the expected six, and every `inline constexpr` declaration of the header text
// against that list, so an extra undeclared constant fails. The not-a-preset-field check walks the preset
// class and both faces by reflection (stories tdc-039/040 have landed, so it is the full check).
// The empty-table-under-BOOTSTRAP clause is story tdc-042.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningConstants.h"
#include "Server/StealthTuningData.h"
#include "Server/StealthTuningSchema.h"
#include "UObject/UnrealType.h"
#include "View/StealthClientTuning.h"

#include <type_traits>

namespace SchemaRowTypesTestData
{
	struct FExpectedConstant
	{
		const TCHAR* Name;
		double Value;
	};

	/** Amendments 2: exactly these six schema constants. */
	const FExpectedConstant ExpectedConstants[] = {
		{ TEXT("BlindGuessWarnPct"), 50.0 },
		{ TEXT("GraceMarginSec"), 30.0 },
		{ TEXT("BushDomeInnerRadiusCm"), 90.0 },
		{ TEXT("SclCapsuleRadiusCm"), 35.0 },
		{ TEXT("PingSkewTolSec"), 0.1 },
		{ TEXT("DueEpsilonMs"), 1.0 },
	};

	void CollectPropertyNames(const UStruct* Struct, TSet<FString>& OutNames)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			OutNames.Add(It->GetName());
			if (const FStructProperty* Inner = CastField<FStructProperty>(*It))
			{
				CollectPropertyNames(Inner->Struct, OutNames);
			}
		}
	}

	/** Names of every `inline constexpr <type> <Name> =` declaration in the header text (the list itself is `FEntry All[]`, not matched). */
	TArray<FString> DeclaredConstantNames(const FString& HeaderText)
	{
		TArray<FString> Names;
		const FRegexPattern Pattern(TEXT("inline\\s+constexpr\\s+(?:double|float|int32|int64|uint8|bool)\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*="));
		FRegexMatcher Matcher(Pattern, HeaderText);
		while (Matcher.FindNext())
		{
			Names.Add(Matcher.GetCaptureGroup(1));
		}
		return Names;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTuningSchemaRowTypesTest, "StealthGame.Tuning.SchemaRowTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningSchemaRowTypesTest::RunTest(const FString& Parameters)
{
	using namespace SchemaRowTypesTestData;

	// Type tags (F-2) and the latch enum order (CR-11).
	TestEqual(TEXT("TypeTag bool is 0"), StealthTuningTypeTag::Bool, static_cast<uint8>(0));
	TestEqual(TEXT("TypeTag int is 1"), StealthTuningTypeTag::Int, static_cast<uint8>(1));
	TestEqual(TEXT("TypeTag float is 2"), StealthTuningTypeTag::Float, static_cast<uint8>(2));
	TestEqual(TEXT("TypeTag enum is 3"), StealthTuningTypeTag::Enum, static_cast<uint8>(3));
	TestEqual(TEXT("TypeTag SoftRef is 4 (ADR-0004 amendment 2026-10-25b)"), StealthTuningTypeTag::SoftRef, static_cast<uint8>(4));
	static_assert(std::is_same_v<std::underlying_type_t<ETuningLatch>, uint8>, "ETuningLatch is a uint8 enum class");
	TestEqual(TEXT("ETuningLatch::Round is 0"), static_cast<int32>(ETuningLatch::Round), 0);
	TestEqual(TEXT("ETuningLatch::Action is 1"), static_cast<int32>(ETuningLatch::Action), 1);
	TestEqual(TEXT("ETuningLatch::Use is 2"), static_cast<int32>(ETuningLatch::Use), 2);
	TestEqual(TEXT("ETuningLatch::Session is 3"), static_cast<int32>(ETuningLatch::Session), 3);

	// The row carries every field the AC names (a designated initialiser fails to compile if one is missing).
	{
		constexpr FTuningSchemaRow Row{
			.Key = "FixtureKeySec", .TypeTag = StealthTuningTypeTag::Float, .Get = nullptr, .Set = nullptr,
			.Default = 1.0, .HardMin = 0.5, .HardMax = 2.0, .SafeMin = 0.75, .SafeMax = 1.5,
			.bPublic = true, .Latch = ETuningLatch::Action, .Owner = "Fixture", .Unit = "Sec",
			.bHasSentinel = true, .Sentinel = 0.0 };
		TestEqual(TEXT("row Key"), FString(Row.Key), FString(TEXT("FixtureKeySec")));
		TestTrue(TEXT("row ranges"), Row.HardMin <= Row.SafeMin && Row.SafeMin <= Row.Default && Row.Default <= Row.SafeMax && Row.SafeMax <= Row.HardMax);
		TestTrue(TEXT("row audience, latch, owner, sentinel"), Row.bPublic && Row.Latch == ETuningLatch::Action && FString(Row.Owner) == TEXT("Fixture") && Row.bHasSentinel);
	}

	// GetTuningSchema() exists and every row it returns has its accessors (the table itself is tdc-041/044).
	for (const FTuningSchemaRow& Row : GetTuningSchema())
	{
		TestTrue(FString::Printf(TEXT("%s has Get and Set"), *FString(Row.Key)), Row.Get != nullptr && Row.Set != nullptr);
	}

	// Exactly the six constants, with their values and GDD references.
	TestEqual(TEXT("the constants list holds exactly six entries"), static_cast<int32>(UE_ARRAY_COUNT(StealthTuningConstants::All)), static_cast<int32>(UE_ARRAY_COUNT(ExpectedConstants)));
	for (const FExpectedConstant& Expected : ExpectedConstants)
	{
		const StealthTuningConstants::FEntry* Found = nullptr;
		for (const StealthTuningConstants::FEntry& Entry : StealthTuningConstants::All)
		{
			if (FCString::Strcmp(Entry.Name, Expected.Name) == 0)
			{
				Found = &Entry;
			}
		}
		if (TestNotNull(FString::Printf(TEXT("constant %s is declared"), Expected.Name), Found))
		{
			TestEqual(FString::Printf(TEXT("constant %s value"), Expected.Name), Found->Value, Expected.Value, 1e-12);
			TestTrue(FString::Printf(TEXT("constant %s cites its GDD reference"), Expected.Name), FCString::Strstr(Found->GddRef, TEXT(".md ")) != nullptr);
		}
	}
	TestEqual(TEXT("BlindGuessWarnPct is 50"), StealthTuningConstants::BlindGuessWarnPct, 50.0);
	TestEqual(TEXT("GraceMarginSec is 30"), StealthTuningConstants::GraceMarginSec, 30.0);
	TestEqual(TEXT("BushDomeInnerRadiusCm is 90"), StealthTuningConstants::BushDomeInnerRadiusCm, 90.0);
	TestEqual(TEXT("PingSkewTolSec is 0.1"), StealthTuningConstants::PingSkewTolSec, 0.1);

	// The header declares no constant outside the list (an extra undeclared constant fails here).
	{
		FString HeaderText;
		const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthFoundation/Public/Server/StealthTuningConstants.h"));
		if (TestTrue(TEXT("StealthTuningConstants.h is readable"), FFileHelper::LoadFileToString(HeaderText, *HeaderPath)))
		{
			const TArray<FString> Declared = DeclaredConstantNames(HeaderText);
			TestEqual(TEXT("the header declares exactly as many constants as the list"), Declared.Num(), static_cast<int32>(UE_ARRAY_COUNT(StealthTuningConstants::All)));
			for (const FString& Name : Declared)
			{
				bool bListed = false;
				for (const StealthTuningConstants::FEntry& Entry : StealthTuningConstants::All)
				{
					bListed |= Name.Equals(Entry.Name, ESearchCase::CaseSensitive);
				}
				TestTrue(FString::Printf(TEXT("declared constant %s is in the list"), *Name), bListed);
			}
			// Control: the scan sees a seeded extra declaration.
			const TArray<FString> Seeded = DeclaredConstantNames(HeaderText + TEXT("\ninline constexpr double SeededExtraPct = 1.0;\n"));
			TestEqual(TEXT("control: a seeded extra constant is counted"), Seeded.Num(), Declared.Num() + 1);
		}
	}

	// No schema constant is a preset field (preset class, server face, client face).
	{
		TSet<FString> PresetNames;
		CollectPropertyNames(UStealthTuningData::StaticClass(), PresetNames);
		CollectPropertyNames(FStealthServerTuning::StaticStruct(), PresetNames);
		CollectPropertyNames(FStealthClientTuning::StaticStruct(), PresetNames);
		TestTrue(TEXT("the reflection walk sees the preset keys"), PresetNames.Contains(TEXT("TeamSize")));
		for (const StealthTuningConstants::FEntry& Entry : StealthTuningConstants::All)
		{
			TestFalse(FString::Printf(TEXT("constant %s is not a preset field"), Entry.Name), PresetNames.Contains(Entry.Name));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
