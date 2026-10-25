// Story tdc-001 (production/epics/tuning-data-config/story-001-foundation-module-skeleton.md):
// StealthFoundation exists as a Runtime module with the ADR-0007 dependency sets, log category,
// server-only guard header and View/Server/Tests folder contract.
// EditorContext only because the test reads source files from the project directory.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "StealthBuildFileTestUtils.h"
#include "StealthFoundationModule.h"

namespace FoundationLayoutTestData
{
	const TCHAR* const ModuleName = TEXT("StealthFoundation");
	const TCHAR* const BuildCsPath = TEXT("Source/StealthFoundation/StealthFoundation.Build.cs");
	const TCHAR* const GuardPath = TEXT("Source/StealthFoundation/Public/StealthServerOnlyGuard.h");
	const TCHAR* const GuardErrorLine = TEXT("#error \"Server-only header included from StealthPresentation (ADR-0007, IVP R10)\"");
	const TCHAR* const PresentationDefine = TEXT("STEALTH_PRESENTATION_MODULE");

	TSet<FString> ExpectedPublicDeps() { return { TEXT("Core"), TEXT("CoreUObject"), TEXT("Engine"), TEXT("NetCore") }; }
	TSet<FString> ExpectedPrivateDeps() { return { TEXT("DeveloperSettings"), TEXT("Json") }; }

	/** Folders of the ADR-0007 contract; each holds a tracked placeholder (git cannot hold empty folders). */
	const TCHAR* const ContractFolders[] = {
		TEXT("Source/StealthFoundation/Public/View"),
		TEXT("Source/StealthFoundation/Public/Server"),
		TEXT("Source/StealthFoundation/Private/Tests"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildFoundationLayoutTest, "StealthGame.Build.FoundationLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildFoundationLayoutTest::RunTest(const FString& Parameters)
{
	using namespace FoundationLayoutTestData;
	using namespace StealthBuildTest;

	// Module is loaded.
	TestTrue(TEXT("StealthFoundation is loaded by FModuleManager"), FModuleManager::Get().IsModuleLoaded(ModuleName));

	// .uproject entry: exactly one, Type Runtime.
	const TSharedPtr<FJsonObject> UProject = LoadUProject();
	if (!TestTrue(TEXT("StealthGame.uproject parses as JSON"), UProject.IsValid()))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonObject>> Entries = FindUProjectModuleEntries(UProject, ModuleName);
	if (TestEqual(TEXT("StealthFoundation appears exactly once in the .uproject Modules"), Entries.Num(), 1))
	{
		TestEqual(TEXT("StealthFoundation .uproject Type"), Entries[0]->GetStringField(TEXT("Type")), FString(TEXT("Runtime")));
	}

	// Build.cs dependency sets, exact.
	FString BuildCs;
	if (TestTrue(TEXT("StealthFoundation.Build.cs is readable"), LoadProjectText(BuildCsPath, BuildCs)))
	{
		const FString PublicDiff = DescribeSetDifference(ExpectedPublicDeps(), ExtractStatementTokens(BuildCs, TEXT("PublicDependencyModuleNames")));
		const FString PrivateDiff = DescribeSetDifference(ExpectedPrivateDeps(), ExtractStatementTokens(BuildCs, TEXT("PrivateDependencyModuleNames")));
		TestTrue(FString::Printf(TEXT("Public dependencies are exactly Core/CoreUObject/Engine/NetCore (%s)"), *PublicDiff), PublicDiff.IsEmpty());
		TestTrue(FString::Printf(TEXT("Private dependencies are exactly DeveloperSettings/Json (%s)"), *PrivateDiff), PrivateDiff.IsEmpty());

		// Negative control: one extra and one missing dependency must be reported as a mismatch.
		const FString Mutated = BuildCs.Replace(TEXT("\"NetCore\""), TEXT("\"InputCore\""));
		const FString MutatedDiff = DescribeSetDifference(ExpectedPublicDeps(), ExtractStatementTokens(Mutated, TEXT("PublicDependencyModuleNames")));
		TestTrue(TEXT("Control: a seeded extra and missing dependency is detected"),
			MutatedDiff.Contains(TEXT("NetCore")) && MutatedDiff.Contains(TEXT("InputCore")));
	}

	// Log category resolves.
	TestEqual(TEXT("LogStealthFoundation category name"), LogStealthFoundation.GetCategoryName(), FName(TEXT("LogStealthFoundation")));

	// Guard header carries the #error for the presentation define.
	FString Guard;
	if (TestTrue(TEXT("Public/StealthServerOnlyGuard.h exists"), LoadProjectText(GuardPath, Guard)))
	{
		TestTrue(TEXT("Guard contains the ADR-0007 #error line"), Guard.Contains(GuardErrorLine, ESearchCase::CaseSensitive));
		TestTrue(TEXT("Guard tests STEALTH_PRESENTATION_MODULE"), Guard.Contains(PresentationDefine, ESearchCase::CaseSensitive));
	}

	// Folder contract with tracked placeholders.
	for (const TCHAR* Folder : ContractFolders)
	{
		TestTrue(FString::Printf(TEXT("%s exists"), Folder), IFileManager::Get().DirectoryExists(*ProjectPath(Folder)));
		TestTrue(FString::Printf(TEXT("%s/.gitkeep exists"), Folder), IFileManager::Get().FileExists(*ProjectPath(FString(Folder) / TEXT(".gitkeep"))));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
