// Stories tdc-022 and tdc-023 (production/epics/tuning-data-config/story-022-ci-workflow-t0-lint-step.md and
// story-023-nightly-workflow.md): file-text checks on the two CI workflows (ADR-0008 section 8). The workflows
// themselves only run on a self-hosted Windows runner that is not registered yet (a user action), so this test pins
// what can be observed from the repository:
//   tests.yml  runs the per-commit gate script (T0 lint, then build, then the ^StealthGame. run, verdict from the
//              output) in one step, does not re-implement the verdict inline, maps NOT ASSESSED and FAIL to a failing
//              job, uploads logs on failure, and states that a queued job without a runner is "not run", never a pass.
//   nightly.yml  has a schedule trigger on the same self-hosted runner, starts tools/ci/nightly.ps1, fails the job on
//              FAIL and on NOT ASSESSED, uploads logs with `if: always()`, and carries the same "not run" statement.
//   nightly.ps1  names the four component groups' scripts and both net-emulation profiles.
// A real run is DEFERRED (the runner). Plain file reads only: no process is started.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildCiWorkflowsTest, "StealthGame.Build.CiWorkflows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildCiWorkflowsTest::RunTest(const FString& Parameters)
{
	using namespace StealthBuildTest;

	FString Tests;
	FString Nightly;
	FString NightlyScript;
	if (!TestTrue(TEXT(".github/workflows/tests.yml is readable"), LoadProjectText(TEXT(".github/workflows/tests.yml"), Tests))
		|| !TestTrue(TEXT(".github/workflows/nightly.yml is readable"), LoadProjectText(TEXT(".github/workflows/nightly.yml"), Nightly))
		|| !TestTrue(TEXT("tools/ci/nightly.ps1 is readable"), LoadProjectText(TEXT("tools/ci/nightly.ps1"), NightlyScript)))
	{
		return false;
	}

	// ---- tests.yml (story 022) ----
	TestTrue(TEXT("tests.yml: runs on the self-hosted Windows runner"), Tests.Contains(TEXT("runs-on: [self-hosted, windows]")));
	TestTrue(TEXT("tests.yml: states that a queued job is 'not run', not a pass"), Tests.Contains(TEXT("\"not run\", not a pass")));
	TestTrue(TEXT("tests.yml: runs the per-commit gate script"), Tests.Contains(TEXT("tools/ci/gate-per-commit.ps1")));
	TestTrue(TEXT("tests.yml: documents the ^StealthGame. prefix filter"), Tests.Contains(TEXT("^StealthGame.")));
	TestTrue(TEXT("tests.yml: documents NOT ASSESSED on no match"), Tests.Contains(TEXT("\"No automation tests matched\" is NOT ASSESSED")));
	TestTrue(TEXT("tests.yml: documents failure on Result={Fail}"), Tests.Contains(TEXT("no Result={Fail}")));
	const int32 LintAt = Tests.Find(TEXT("1. T0 lint"), ESearchCase::CaseSensitive);
	const int32 BuildAt = Tests.Find(TEXT("2. Build"), ESearchCase::CaseSensitive);
	TestTrue(TEXT("tests.yml: the T0 lint stage is listed before the build stage"), LintAt != INDEX_NONE && BuildAt != INDEX_NONE && LintAt < BuildAt);
	TestFalse(TEXT("tests.yml: no separate build step ahead of the gate (the gate builds after its lint)"), Tests.Contains(TEXT("UnrealBuildTool.exe")));
	TestFalse(TEXT("tests.yml: the verdict is not re-implemented inline"), Tests.Contains(TEXT("grep -q")));
	TestTrue(TEXT("tests.yml: a gate FAIL fails the job"), Tests.Contains(TEXT("per-commit gate: FAIL")) && Tests.Contains(TEXT("exit 1 ;;")));
	TestTrue(TEXT("tests.yml: a gate NOT ASSESSED fails the job"), Tests.Contains(TEXT("per-commit gate: NOT ASSESSED")) && Tests.Contains(TEXT("exit 2 ;;")));
	TestTrue(TEXT("tests.yml: logs are uploaded even when the gate fails"), Tests.Contains(TEXT("if: always()")) && Tests.Contains(TEXT("actions/upload-artifact")));

	// ---- nightly.yml and nightly.ps1 (story 023) ----
	TestTrue(TEXT("nightly.yml: has a daily schedule trigger"), Nightly.Contains(TEXT("schedule:")) && Nightly.Contains(TEXT("cron:")));
	TestTrue(TEXT("nightly.yml: runs on the same self-hosted Windows runner"), Nightly.Contains(TEXT("runs-on: [self-hosted, windows]")));
	TestTrue(TEXT("nightly.yml: states that a queued job is 'not run', not a pass"), Nightly.Contains(TEXT("\"not run\", not a pass")));
	TestTrue(TEXT("nightly.yml: starts tools/ci/nightly.ps1"), Nightly.Contains(TEXT("tools/ci/nightly.ps1")));
	TestTrue(TEXT("nightly.yml: a nightly FAIL fails the job"), Nightly.Contains(TEXT("nightly gate: FAIL")) && Nightly.Contains(TEXT("exit 1 ;;")));
	TestTrue(TEXT("nightly.yml: a nightly NOT ASSESSED fails the job"), Nightly.Contains(TEXT("nightly gate: NOT ASSESSED")) && Nightly.Contains(TEXT("exit 2 ;;")));
	TestTrue(TEXT("nightly.yml: logs are uploaded with if: always()"), Nightly.Contains(TEXT("if: always()")) && Nightly.Contains(TEXT("actions/upload-artifact")) && Nightly.Contains(TEXT("Saved/Nightly/")));
	for (const TCHAR* Needle : { TEXT("gate-per-commit.ps1"), TEXT("run-probes.ps1"), TEXT("gate-probes.sh"), TEXT("shipping-symbol-check.sh"),
			TEXT("StealthLeak2"), TEXT("StealthClean"), TEXT("-PktEmulationProfile="), TEXT("StealthMultiProcessSmoke") })
	{
		TestTrue(FString::Printf(TEXT("nightly.ps1 wires '%s'"), Needle), NightlyScript.Contains(Needle));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
