// Story tdc-021 (production/epics/tuning-data-config/story-021-per-commit-gate-script.md):
// G2 wrapper for the per-commit gate script (ADR-0008 section 8). Runs ONLY
// `tools/ci/gate-per-commit.ps1 -SelfTest`, which feeds canned automation logs and stub build and
// editor tools to the real parser and stage logic:
//   no tests matched -> NOT ASSESSED (exit 2); any Result={Fail} -> FAIL (exit 1), also when the log
//   holds passes; at least one "Test Completed" and no failure -> PASS (exit 0); an empty log or a log
//   with no completion line -> NOT ASSESSED; a lint finding stops the run before the build; a build
//   failure stops it before the automation run.
// It must never run the full gate: that runs the automation suite, which includes this test, and would
// recurse. EditorContext only because the test runs a project script.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace GatePerCommitTestData
{
	const TCHAR* const SelftestArgs = TEXT("-SelfTest");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("no-tests-matched"), TEXT("one-failure"), TEXT("all-pass"), TEXT("pass-and-fail-is-fail"),
		TEXT("empty-log-not-assessed"), TEXT("no-completion-lines-not-assessed"), TEXT("missing-log-not-assessed"),
		TEXT("lint-failure-stops-before-build"), TEXT("build-failure-stops-before-automation"),
		TEXT("full-flow-pass"), TEXT("full-flow-no-tests-matched"), TEXT("full-flow-one-failure"),
	};

	/** Verdict lines the selftest echoes from the script. */
	const TCHAR* const VerdictLines[] = {
		TEXT("GATE-PER-COMMIT: NOT ASSESSED [analyze] No automation tests matched"),
		TEXT("GATE-PER-COMMIT: FAIL [analyze] 1 test(s) reported Result={Fail}"),
		TEXT("GATE-PER-COMMIT: PASS [analyze] 3 test(s) completed, none failed"),
		TEXT("GATE-PER-COMMIT: FAIL [lint]"),
		TEXT("GATE-PER-COMMIT: FAIL [build]"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildGatePerCommitTest, "StealthGame.Build.GatePerCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildGatePerCommitTest::RunTest(const FString& Parameters)
{
	using namespace GatePerCommitTestData;
	using namespace StealthBuildTest;

	const FString PowerShell = FindPowerShell();
	if (!TestFalse(TEXT("Windows PowerShell was found"), PowerShell.IsEmpty()))
	{
		return false;
	}
	const FScriptRun Run = RunPowerShellScript(PowerShell, TEXT("tools/ci/gate-per-commit.ps1"), SelftestArgs);
	if (!TestTrue(TEXT("Gate selftest launched"), Run.bLaunched))
	{
		return false;
	}
	TestEqual(TEXT("Gate selftest exit code"), Run.ExitCode, 0);
	TestTrue(TEXT("Gate selftest reports PASS"), Run.Output.Contains(TEXT("SELFTEST RESULT: PASS")));
	for (const TCHAR* Case : Cases)
	{
		TestTrue(FString::Printf(TEXT("Selftest case %s ok"), Case), Run.Output.Contains(FString::Printf(TEXT("SELFTEST %s ok"), Case)));
	}
	for (const TCHAR* Line : VerdictLines)
	{
		TestTrue(FString::Printf(TEXT("Verdict line echoed: '%s'"), Line), Run.Output.Contains(Line));
	}
	// The selftest output quotes canned verdict lines (including "Result={Fail}"), so it is echoed only when
	// this test already failed: a passing run must leave the automation log free of text that looks like a failure.
	if (HasAnyErrors())
	{
		AddInfo(Run.Output);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
