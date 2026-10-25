// Story tdc-011 (production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md):
// G2 wrapper for the T0 lint framework (ADR-0008 sections 1 and 8). Runs
// tools/ci/lint/selftest/run-framework-selftest.sh, which drives tools/ci/lint/gate.sh with a
// demonstration rule over fixtures: clean -> PASS (exit 0), one finding -> FAIL (exit 1, the
// observation line printed), zero files -> NOT ASSESSED (exit 2), plus contract breaks.
// EditorContext only because the test runs a project script.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace LintFrameworkTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/run-framework-selftest.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("clean-pass"), TEXT("one-finding-fail"), TEXT("one-finding-fail-text"), TEXT("zero-files-not-assessed"),
		TEXT("two-findings-two-lines"), TEXT("emitter-no-verdict"), TEXT("obs-clean-pass"), TEXT("malformed-line-not-assessed"),
		TEXT("missing-obs-not-assessed"), TEXT("crashed-rule-not-assessed"), TEXT("scanned-counts-distinct-files"),
	};

	/** Lines the selftest echoes from the gate, proving the observation and verdict formats. */
	const TCHAR* const RequiredLines[] = {
		TEXT("finding.txt:3 demo seeded violation marker"),
		TEXT("GATE: PASS (1 file(s) scanned, 0 findings)"),
		TEXT("GATE: FAIL (1 finding(s); 2 file(s) scanned)"),
		TEXT("GATE: NOT ASSESSED (0 file(s) scanned"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildLintFrameworkTest, "StealthGame.Build.LintFramework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildLintFrameworkTest::RunTest(const FString& Parameters)
{
	using namespace LintFrameworkTestData;
	using namespace StealthBuildTest;

	const FString Bash = FindBash();
	if (!TestFalse(TEXT("A bash for the CI scripts was found (set STEALTH_BASH to override)"), Bash.IsEmpty()))
	{
		return false;
	}
	const FScriptRun Run = RunBashScript(Bash, SelftestScript);
	AddInfo(Run.Output);
	if (!TestTrue(TEXT("Selftest script launched"), Run.bLaunched))
	{
		return false;
	}
	TestEqual(TEXT("Selftest exit code"), Run.ExitCode, 0);
	TestTrue(TEXT("Selftest reports PASS"), Run.Output.Contains(TEXT("SELFTEST RESULT: PASS")));
	for (const TCHAR* Case : Cases)
	{
		TestTrue(FString::Printf(TEXT("Selftest case %s ok"), Case), Run.Output.Contains(FString::Printf(TEXT("SELFTEST %s ok"), Case)));
	}
	for (const TCHAR* Line : RequiredLines)
	{
		TestTrue(FString::Printf(TEXT("Gate output contains '%s'"), Line), Run.Output.Contains(Line));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
