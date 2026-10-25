// Story tdc-002 (production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md):
// ADR-0007 Verification Required item (1). StealthPresentation's PrivateDefinitions entry
// STEALTH_PRESENTATION_MODULE=1 reaches a Presentation translation unit and does not reach a
// dependent module. The dependent probe is evaluated HERE, in a StealthGameEditor translation unit
// (not in a header inlined from Presentation), so a leaked PublicDefinitions entry would flip it.
// Lives in the Editor module because Foundation must not depend upward on Presentation.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "StealthPresentationModule.h"

namespace PresentationGuardDefineTestData
{
	const TCHAR* const DefineName = TEXT("STEALTH_PRESENTATION_MODULE");
	const TCHAR* const PresentationBuildCs = TEXT("Source/StealthPresentation/StealthPresentation.Build.cs");
	const TCHAR* const GameplayCoreDir = TEXT("Source/StealthGameplay/Core");
	const TCHAR* const GameplayFeaturesDir = TEXT("Source/StealthGameplay/Features");

#if defined(STEALTH_PRESENTATION_MODULE) && STEALTH_PRESENTATION_MODULE
	constexpr bool bDependentSeesDefine = true;
#else
	constexpr bool bDependentSeesDefine = false;
#endif

	FString ProjectPath(const TCHAR* Relative)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Relative));
	}

	/** True when a statement starting at Identifier (up to the next ';') mentions Token. Line comments are ignored. */
	bool StatementMentions(const FString& SourceText, const FString& Identifier, const FString& Token)
	{
		TArray<FString> Lines;
		SourceText.ParseIntoArrayLines(Lines, false);
		FString Text;
		for (const FString& Line : Lines)
		{
			const int32 Comment = Line.Find(TEXT("//"));
			Text += (Comment == INDEX_NONE ? Line : Line.Left(Comment)) + TEXT("\n");
		}
		for (int32 At = Text.Find(Identifier, ESearchCase::CaseSensitive); At != INDEX_NONE;
			 At = Text.Find(Identifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + Identifier.Len()))
		{
			const int32 End = Text.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
			if ((End == INDEX_NONE ? Text.Mid(At) : Text.Mid(At, End - At)).Contains(Token, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPresentationGuardDefineIsPrivateTest, "StealthGame.Build.PresentationGuardDefineIsPrivate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildPresentationGuardDefineIsPrivateTest::RunTest(const FString& Parameters)
{
	using namespace PresentationGuardDefineTestData;

	// Both modules load.
	TestTrue(TEXT("StealthGameplay is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("StealthGameplay")));
	TestTrue(TEXT("StealthPresentation is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("StealthPresentation")));

	// The define reaches a Presentation TU and not this dependent module's TU.
	TestTrue(TEXT("A StealthPresentation TU sees STEALTH_PRESENTATION_MODULE=1"), StealthPresentationSeesGuardDefine());
	TestFalse(TEXT("A dependent module TU (StealthGameEditor) does not see STEALTH_PRESENTATION_MODULE"), bDependentSeesDefine);

	// Control by inspection: the define is declared privately and never publicly.
	FString BuildCs;
	if (TestTrue(TEXT("StealthPresentation.Build.cs is readable"), FFileHelper::LoadFileToString(BuildCs, *ProjectPath(PresentationBuildCs))))
	{
		TestTrue(TEXT("PrivateDefinitions declares STEALTH_PRESENTATION_MODULE=1"),
			StatementMentions(BuildCs, TEXT("PrivateDefinitions"), TEXT("\"STEALTH_PRESENTATION_MODULE=1\"")));
		TestFalse(TEXT("No PublicDefinitions entry mentions STEALTH_PRESENTATION_MODULE"),
			StatementMentions(BuildCs, TEXT("PublicDefinitions"), DefineName));

		// The inspection detects a seeded public entry (the case that would make the dependent probe true).
		const FString Seeded = BuildCs + TEXT("\nPublicDefinitions.Add(\"STEALTH_PRESENTATION_MODULE=1\");\n");
		TestTrue(TEXT("Control: a seeded PublicDefinitions entry is detected"),
			StatementMentions(Seeded, TEXT("PublicDefinitions"), DefineName));
	}

	// Gameplay source folders.
	TestTrue(TEXT("StealthGameplay/Core exists"), IFileManager::Get().DirectoryExists(*ProjectPath(GameplayCoreDir)));
	TestTrue(TEXT("StealthGameplay/Features exists"), IFileManager::Get().DirectoryExists(*ProjectPath(GameplayFeaturesDir)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
