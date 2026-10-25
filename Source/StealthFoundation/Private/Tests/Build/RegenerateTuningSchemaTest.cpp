// Story tdc-042 (production/epics/tuning-data-config/story-042-schema-bootstrap-regeneration.md):
// G2 wrapper for tools/ci/regenerate-tuning-schema.ps1 (ADR-0004 section 2, bootstrap guard). Runs ONLY
// the script's -SelfTest, which drives the real stage logic with stub build and editor tools:
//   the define is set for the bootstrap build and removed for the final build; a generator FAIL line, an
//   editor that prints no summary, a failing bootstrap or final build, and a verify that reports a
//   bootstrap build (define left set) each stop the script with exit 1; -DryRun runs nothing.
// It never runs the real regeneration: that rebuilds the editor this test runs in. The real run is the
// disposable-worktree evidence of the story (production/qa/evidence/story-042-schema-bootstrap-regeneration/).
// Also checks that StealthFoundation.Build.cs maps the environment variable to the private define.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace RegenerateTuningSchemaTestData
{
	const TCHAR* const Cases[] = {
		TEXT("full-flow-pass"), TEXT("commandlet-failure-fails-script"), TEXT("editor-without-summary-fails"),
		TEXT("bootstrap-build-failure-stops"), TEXT("final-build-failure-stops"), TEXT("define-left-set-is-caught"),
		TEXT("caller-define-removed-before-final-build"), TEXT("dry-run-runs-nothing"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildRegenerateTuningSchemaTest, "StealthGame.Build.RegenerateTuningSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildRegenerateTuningSchemaTest::RunTest(const FString& Parameters)
{
	using namespace RegenerateTuningSchemaTestData;
	using namespace StealthBuildTest;

	FString BuildCs;
	if (TestTrue(TEXT("StealthFoundation.Build.cs is readable"), LoadProjectText(TEXT("Source/StealthFoundation/StealthFoundation.Build.cs"), BuildCs)))
	{
		const FString Live = StripLineComments(BuildCs);
		TestTrue(TEXT("Build.cs reads the STEALTH_TUNING_SCHEMA_BOOTSTRAP environment variable"),
			Live.Contains(TEXT("GetEnvironmentVariable(\"STEALTH_TUNING_SCHEMA_BOOTSTRAP\")")));
		TestTrue(TEXT("Build.cs adds the define as a PrivateDefinition with both values"),
			Live.Contains(TEXT("PrivateDefinitions.Add")) && Live.Contains(TEXT("STEALTH_TUNING_SCHEMA_BOOTSTRAP=1")) && Live.Contains(TEXT("STEALTH_TUNING_SCHEMA_BOOTSTRAP=0")));
	}

	const FString PowerShell = FindPowerShell();
	if (!TestFalse(TEXT("Windows PowerShell was found"), PowerShell.IsEmpty()))
	{
		return false;
	}
	const FScriptRun Run = RunPowerShellScript(PowerShell, TEXT("tools/ci/regenerate-tuning-schema.ps1"), TEXT("-SelfTest"));
	if (!TestTrue(TEXT("Regeneration selftest launched"), Run.bLaunched))
	{
		return false;
	}
	TestEqual(TEXT("Regeneration selftest exit code"), Run.ExitCode, 0);
	TestTrue(TEXT("Regeneration selftest reports PASS"), Run.Output.Contains(TEXT("SELFTEST RESULT: PASS")));
	for (const TCHAR* Case : Cases)
	{
		TestTrue(FString::Printf(TEXT("Selftest case %s ok"), Case), Run.Output.Contains(FString::Printf(TEXT("SELFTEST %s ok"), Case)));
	}
	if (HasAnyErrors())
	{
		AddInfo(Run.Output);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
