// Story oc-002 (production/epics/objectives-chain/story-002-chain-data-types-and-cacao-asset.md):
//   StealthGame.Objectives.ChainAssetId (AC3 (a), ADR-0020 E5): the class default object's GetPrimaryAssetId() is invalid; an
//     asset's is ("ObjectiveChain", name); the type name equals the Config/DefaultGame.ini scan entry exactly (case-sensitive)
//     and that entry's cook rule is AlwaysCook; the editor's Asset Manager resolves the Cacao chain by FPrimaryAssetId. A
//     misspelled type ("objectiveChain") fails the same comparison. The cooked-client half (OBJ-V3) is recorded in
//     production/qa/evidence/oc-cook-check.md.
//   StealthGame.Objectives.ChainTextLocalisable (AC4, ADR-0020 E6, section 2): Config/Localization/Game_Gather.ini includes
//     the chain asset folder in an IncludePathFilters entry (a folder outside it fails the same check); CardTitle and CardLine
//     are FText (a seeded FString card line fails); no chain or definition property is replicated (no network read path).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObjectivesTestTypes.h"
#include "UObject/UnrealType.h"
#include "View/ObjectiveChainData.h"
#include "View/Testing/ObjectiveCacaoChainFixture.h"

namespace ObjectivesChainAssetIdTestData
{
	/** The PrimaryAssetType="..." value of every DefaultGame.ini scan line that names ObjectiveChainData, with its line. */
	TArray<TPair<FString, FString>> ScanEntries()
	{
		TArray<TPair<FString, FString>> Out;
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultGame.ini")));
		for (const FString& Line : Lines)
		{
			if (!Line.StartsWith(TEXT("+PrimaryAssetTypesToScan=")) || !Line.Contains(TEXT("/Script/StealthGameplay.ObjectiveChainData")))
			{
				continue;
			}
			FString Type;
			const FString Key = TEXT("PrimaryAssetType=\"");
			const int32 At = Line.Find(Key, ESearchCase::CaseSensitive);
			if (At != INDEX_NONE)
			{
				const int32 Start = At + Key.Len();
				const int32 End = Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
				Type = End != INDEX_NONE ? Line.Mid(Start, End - Start) : FString();
			}
			Out.Emplace(Type, Line);
		}
		return Out;
	}

	/** The IncludePathFilters values of every GatherTextFromAssets step of the gather config text. */
	TArray<FString> GatherIncludeFilters(const FString& Config)
	{
		TArray<FString> Lines;
		Config.ParseIntoArrayLines(Lines);
		TArray<FString> Out;
		bool bAssetStep = false;
		for (const FString& Raw : Lines)
		{
			const FString Line = Raw.TrimStartAndEnd();
			if (Line.StartsWith(TEXT("[")))
			{
				bAssetStep = false;
			}
			else if (Line == TEXT("CommandletClass=GatherTextFromAssets"))
			{
				bAssetStep = true;
			}
			else if (bAssetStep && Line.StartsWith(TEXT("IncludePathFilters=")))
			{
				Out.Add(Line.RightChop(19));
			}
		}
		return Out;
	}

	/** True when a filter ("Content/X/*") covers the project-relative folder ("Content/X/Y"). */
	bool Covers(TConstArrayView<FString> Filters, const FString& Folder)
	{
		const FString Wanted = Folder.EndsWith(TEXT("/")) ? Folder : Folder + TEXT("/");
		for (const FString& Filter : Filters)
		{
			const FString Prefix = Filter.EndsWith(TEXT("*")) ? Filter.LeftChop(1) : Filter;
			if (Wanted.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** "/Game/StealthGame/Objectives" -> "Content/StealthGame/Objectives". */
	FString ContentFolder(const TCHAR* GamePath)
	{
		return FString(GamePath).Replace(TEXT("/Game/"), TEXT("Content/"));
	}

	bool IsTextProperty(const UScriptStruct* Struct, const TCHAR* Name)
	{
		const FProperty* Property = Struct->FindPropertyByName(Name);
		return Property != nullptr && Property->IsA<FTextProperty>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesChainAssetIdTest, "StealthGame.Objectives.ChainAssetId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesChainAssetIdTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesChainAssetIdTestData;

	// The CDO and an asset.
	TestFalse(TEXT("the class default object's primary asset id is invalid"), GetDefault<UObjectiveChainData>()->GetPrimaryAssetId().IsValid());
	const FString Path = FString::Printf(TEXT("%s/%s.%s"), StealthObjectivesCacaoFixture::AssetFolder, StealthObjectivesCacaoFixture::AssetName, StealthObjectivesCacaoFixture::AssetName);
	const UObjectiveChainData* Chain = LoadObject<UObjectiveChainData>(nullptr, *Path);
	if (TestNotNull(TEXT("the Cacao chain loads"), Chain))
	{
		const FPrimaryAssetId Id = Chain->GetPrimaryAssetId();
		TestEqual(TEXT("the asset's type is ObjectiveChain"), Id.PrimaryAssetType.GetName().ToString(), FString(TEXT("ObjectiveChain")));
		TestEqual(TEXT("the asset's name is its object name"), Id.PrimaryAssetName, FName(StealthObjectivesCacaoFixture::AssetName));
	}

	// The DefaultGame.ini scan entry: exactly one, the exact (case-sensitive) type, AlwaysCook, the Objectives folder.
	const TArray<TPair<FString, FString>> Entries = ScanEntries();
	if (TestEqual(TEXT("DefaultGame.ini has one ObjectiveChainData scan entry"), Entries.Num(), 1))
	{
		const FString TypeName = UObjectiveChainData::PrimaryAssetType.GetName().ToString();
		TestTrue(TEXT("the ini type equals the class's type exactly (case-sensitive)"), Entries[0].Key.Equals(TypeName, ESearchCase::CaseSensitive));
		TestFalse(TEXT("seeded: a misspelled type fails the same comparison"), FString(TEXT("objectiveChain")).Equals(TypeName, ESearchCase::CaseSensitive));
		TestTrue(TEXT("the entry's cook rule is AlwaysCook"), Entries[0].Value.Contains(TEXT("CookRule=AlwaysCook")));
		TestTrue(TEXT("the entry scans the Objectives folder"), Entries[0].Value.Contains(FString::Printf(TEXT("Path=\"%s\""), StealthObjectivesCacaoFixture::AssetFolder)));
	}

	// The loaded settings and the editor's Asset Manager agree.
	bool bInSettings = false;
	for (const FPrimaryAssetTypeInfo& Info : GetDefault<UAssetManagerSettings>()->PrimaryAssetTypesToScan)
	{
		if (Info.PrimaryAssetType == UObjectiveChainData::PrimaryAssetType.GetName())
		{
			bInSettings = true;
			TestEqual(TEXT("settings: AlwaysCook"), static_cast<int32>(Info.Rules.CookRule), static_cast<int32>(EPrimaryAssetCookRule::AlwaysCook));
		}
	}
	TestTrue(TEXT("the Asset Manager settings scan ObjectiveChain"), bInSettings);
	if (UAssetManager::IsInitialized())
	{
		const FPrimaryAssetId Id(UObjectiveChainData::PrimaryAssetType, FName(StealthObjectivesCacaoFixture::AssetName));
		TestTrue(TEXT("the Asset Manager resolves the Cacao chain by FPrimaryAssetId"), UAssetManager::Get().GetPrimaryAssetPath(Id).IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesChainTextLocalisableTest, "StealthGame.Objectives.ChainTextLocalisable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesChainTextLocalisableTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesChainAssetIdTestData;

	// (a) The gather target includes the chain folder.
	FString Config;
	const FString ConfigPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Localization"), TEXT("Game_Gather.ini"));
	if (!TestTrue(TEXT("Config/Localization/Game_Gather.ini exists"), FFileHelper::LoadFileToString(Config, *ConfigPath)))
	{
		return false;
	}
	const TArray<FString> Filters = GatherIncludeFilters(Config);
	TestTrue(TEXT("a GatherTextFromAssets step has include filters"), Filters.Num() > 0);
	TestTrue(TEXT("the chain asset folder is inside the gather target"), Covers(Filters, ContentFolder(StealthObjectivesCacaoFixture::AssetFolder)));
	TestFalse(TEXT("seeded: a folder outside the gather path is not covered"), Covers(Filters, TEXT("Content/StealthGame/Maps")));

	// (b) Card text is FText; nothing of the chain replicates.
	TestTrue(TEXT("CardTitle is FText"), IsTextProperty(FObjectiveDef::StaticStruct(), TEXT("CardTitle")));
	TestTrue(TEXT("CardLine is FText"), IsTextProperty(FObjectiveDef::StaticStruct(), TEXT("CardLine")));
	TestFalse(TEXT("seeded: an FString CardLine fails the FText check"), IsTextProperty(FObjectivesSeededStringCardDef::StaticStruct(), TEXT("CardLine")));
	int32 Replicated = 0;
	for (TFieldIterator<FProperty> It(FObjectiveDef::StaticStruct()); It; ++It)
	{
		Replicated += It->HasAnyPropertyFlags(CPF_Net | CPF_RepNotify) ? 1 : 0;
	}
	for (TFieldIterator<FProperty> It(UObjectiveChainData::StaticClass()); It; ++It)
	{
		Replicated += It->HasAnyPropertyFlags(CPF_Net | CPF_RepNotify) ? 1 : 0;
	}
	TestEqual(TEXT("no chain or definition property is replicated (clients read card text from their own cooked asset)"), Replicated, 0);
	return true;
}

#endif
