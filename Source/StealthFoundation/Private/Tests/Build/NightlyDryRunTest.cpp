// Story tdc-023 (production/epics/tuning-data-config/story-023-nightly-workflow.md):
// G2 wrapper for the nightly gate script (ADR-0008 section 8). Runs ONLY `tools/ci/nightly.ps1 -SelfTest`, which
// runs the script in dry-run mode with every component replaced by a stub that can be told to emit a failing
// observation:
//   a clean run passes with all four component groups run and a log per component; a seeded failing observation
//   from any ONE component (gate, probes, either T3 profile run, Shipping-symbol check) fails the job while the
//   others still run; a component that cannot decide, did not run or crashed is NOT ASSESSED, never a pass; a failure
//   wins over a not-assessed component; running a subset can never pass; the profile applied to each T3 run is
//   recorded in the log.
// It must never run the real nightly: that builds, cooks and launches game processes. EditorContext only because the
// test runs a project script.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace NightlyDryRunTestData
{
	const TCHAR* const SelftestArgs = TEXT("-SelfTest");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("clean-run-passes"), TEXT("all-four-groups-ran"), TEXT("logs-collected-per-component-and-summary"),
		TEXT("t3-profile-recorded-in-log"),
		TEXT("seeded-failure-in-gate-fails-the-job"), TEXT("seeded-failure-in-probes-fails-the-job"),
		TEXT("seeded-failure-in-t3-StealthLeak2-fails-the-job"), TEXT("seeded-failure-in-t3-StealthClean-fails-the-job"),
		TEXT("seeded-failure-in-shipping-symbol-fails-the-job"), TEXT("seeded-failing-observation-is-in-the-component-log"),
		TEXT("not-assessed-component-gives-not-assessed"), TEXT("component-with-no-verdict-did-not-run-not-assessed"),
		TEXT("crashed-component-is-not-assessed-not-pass"), TEXT("fail-wins-over-not-assessed"), TEXT("only-subset-can-never-pass"),
	};

	/** Lines the selftest echoes from the script under test. */
	const TCHAR* const EchoedLines[] = {
		TEXT("NIGHTLY: PASS - every component ran and passed"),
		TEXT("GROUPS RAN: gate probes t3 shipping-symbol (4 of 4)"),
		TEXT("NIGHTLY: FAIL - failing: probes"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildNightlyDryRunTest, "StealthGame.Build.NightlyDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildNightlyDryRunTest::RunTest(const FString& Parameters)
{
	using namespace NightlyDryRunTestData;
	using namespace StealthBuildTest;

	const FString PowerShell = FindPowerShell();
	if (!TestFalse(TEXT("Windows PowerShell was found"), PowerShell.IsEmpty()))
	{
		return false;
	}
	const FScriptRun Run = RunPowerShellScript(PowerShell, TEXT("tools/ci/nightly.ps1"), SelftestArgs);
	if (!TestTrue(TEXT("Nightly selftest launched"), Run.bLaunched))
	{
		return false;
	}
	TestEqual(TEXT("Nightly selftest exit code"), Run.ExitCode, 0);
	TestTrue(TEXT("Nightly selftest reports PASS"), Run.Output.Contains(TEXT("SELFTEST RESULT: PASS")));
	for (const TCHAR* Case : Cases)
	{
		TestTrue(FString::Printf(TEXT("Selftest case %s ok"), Case), Run.Output.Contains(FString::Printf(TEXT("SELFTEST %s ok"), Case)));
	}
	for (const TCHAR* Line : EchoedLines)
	{
		TestTrue(FString::Printf(TEXT("Line echoed: '%s'"), Line), Run.Output.Contains(Line));
	}
	if (HasAnyErrors())
	{
		AddInfo(Run.Output);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
