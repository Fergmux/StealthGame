// Story tdc-004 (production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md):
// static exclusion checks for StealthTestHarness (ADR-0007 section 1, ADR-0008 section 1).
// - the .uproject entry's TargetConfigurationDenyList is exactly {Shipping, Test};
// - no Source/*.Target.cs lists the harness in ExtraModuleNames (it has no configuration check);
// - no Build.cs other than the harness's own names StealthTestHarness;
// - no Build.cs other than StealthGameEditor's names StealthGameEditor;
// - the harness is loaded in this Development editor (hosted here so a load failure is observable).
// Each check also runs against seeded text to prove it detects a violation.
// EditorContext only because the test reads source files from the project directory.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "StealthBuildFileTestUtils.h"

namespace HarnessExclusionsTestData
{
	const FString HarnessModule = TEXT("StealthTestHarness");
	const FString EditorModule = TEXT("StealthGameEditor");

	TSet<FString> ExpectedDenyList() { return { TEXT("Shipping"), TEXT("Test") }; }

	/** True when a Target.cs text lists Module in an ExtraModuleNames statement. */
	bool TargetListsModule(const FString& TargetCs, const FString& Module)
	{
		return StealthBuildTest::ExtractStatementTokens(TargetCs, TEXT("ExtraModuleNames")).Contains(Module);
	}

	/** True when a Build.cs text names Module as a quoted string outside comments. */
	bool BuildCsNamesModule(const FString& BuildCs, const FString& Module)
	{
		return StealthBuildTest::ExtractQuotedTokens(StealthBuildTest::StripLineComments(BuildCs)).Contains(Module);
	}

	/** The deny-list entries of one .uproject module entry (empty when the field is absent). */
	TSet<FString> DenyListOf(const TSharedPtr<FJsonObject>& Entry)
	{
		TSet<FString> Values;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Entry.IsValid() && Entry->TryGetArrayField(TEXT("TargetConfigurationDenyList"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				Values.Add(Value->AsString());
			}
		}
		return Values;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildHarnessModuleExclusionsTest, "StealthGame.Build.HarnessModuleExclusions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildHarnessModuleExclusionsTest::RunTest(const FString& Parameters)
{
	using namespace HarnessExclusionsTestData;
	using namespace StealthBuildTest;

	// .uproject deny list, exactly {Shipping, Test}.
	const TSharedPtr<FJsonObject> UProject = LoadUProject();
	if (!TestTrue(TEXT("StealthGame.uproject parses as JSON"), UProject.IsValid()))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonObject>> Entries = FindUProjectModuleEntries(UProject, HarnessModule);
	if (TestEqual(TEXT("StealthTestHarness appears exactly once in the .uproject Modules"), Entries.Num(), 1))
	{
		TestEqual(TEXT("StealthTestHarness .uproject Type"), Entries[0]->GetStringField(TEXT("Type")), FString(TEXT("Runtime")));
		const FString Diff = DescribeSetDifference(ExpectedDenyList(), DenyListOf(Entries[0]));
		TestTrue(FString::Printf(TEXT("TargetConfigurationDenyList is exactly Shipping and Test (%s)"), *Diff), Diff.IsEmpty());
	}

	// No Target.cs lists the harness in ExtraModuleNames.
	TArray<FString> TargetFiles;
	IFileManager::Get().FindFiles(TargetFiles, *ProjectPath(TEXT("Source/*.Target.cs")), true, false);
	TestTrue(TEXT("At least one Source/*.Target.cs was found"), TargetFiles.Num() > 0);
	for (const FString& File : TargetFiles)
	{
		FString Text;
		if (TestTrue(FString::Printf(TEXT("%s is readable"), *File), LoadProjectText(FString(TEXT("Source")) / File, Text)))
		{
			TestFalse(FString::Printf(TEXT("%s does not list StealthTestHarness in ExtraModuleNames"), *File), TargetListsModule(Text, HarnessModule));
		}
	}

	// No foreign Build.cs names the harness or the editor module.
	TArray<FString> ModuleDirs;
	IFileManager::Get().FindFiles(ModuleDirs, *ProjectPath(TEXT("Source/*")), false, true);
	int32 BuildFilesRead = 0;
	for (const FString& Module : ModuleDirs)
	{
		FString Text;
		if (!LoadProjectText(FString::Printf(TEXT("Source/%s/%s.Build.cs"), *Module, *Module), Text))
		{
			continue;
		}
		++BuildFilesRead;
		if (Module != HarnessModule)
		{
			TestFalse(FString::Printf(TEXT("%s.Build.cs does not name StealthTestHarness"), *Module), BuildCsNamesModule(Text, HarnessModule));
		}
		if (Module != EditorModule)
		{
			TestFalse(FString::Printf(TEXT("%s.Build.cs does not name StealthGameEditor"), *Module), BuildCsNamesModule(Text, EditorModule));
		}
	}
	TestTrue(TEXT("Every module Build.cs was read (at least the six ADR-0007 modules)"), BuildFilesRead >= 6);

	// Controls: each check detects a seeded violation.
	TestTrue(TEXT("Control: a Target.cs listing the harness is detected"),
		TargetListsModule(TEXT("ExtraModuleNames.AddRange(new string[] { \"StealthGame\", \"StealthTestHarness\" });"), HarnessModule));
	TestTrue(TEXT("Control: a Build.cs depending on the harness is detected"),
		BuildCsNamesModule(TEXT("PrivateDependencyModuleNames.Add(\"StealthTestHarness\");"), HarnessModule));
	TestTrue(TEXT("Control: a runtime Build.cs depending on the editor module is detected"),
		BuildCsNamesModule(TEXT("PublicDependencyModuleNames.AddRange(new string[] { \"Core\", \"StealthGameEditor\" });"), EditorModule));
	TestFalse(TEXT("Control: a commented-out dependency is not a violation"),
		BuildCsNamesModule(TEXT("// PrivateDependencyModuleNames.Add(\"StealthTestHarness\");"), HarnessModule));
	TestFalse(TEXT("Control: a deny list of only Shipping is a mismatch"),
		DescribeSetDifference(ExpectedDenyList(), { TEXT("Shipping") }).IsEmpty());

	// The harness is loaded in this Development editor.
	TestTrue(TEXT("StealthTestHarness is loaded in the Development editor"), FModuleManager::Get().IsModuleLoaded(*HarnessModule));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
