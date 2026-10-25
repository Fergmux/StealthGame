// Story tdc-006 (production/epics/tuning-data-config/story-006-shipping-symbol-check.md):
// G2 wrapper for the Shipping-symbol check (ADR-0008 sections 4 and 7). Runs
// tools/ci/selftest/shipping-symbol-check/run-selftest.sh, which builds synthetic packages at run
// time and checks tools/ci/shipping-symbol-check.sh for a clean pass, seeded Shipping violations
// (sentinel ASCII / UTF-16LE, harness file / name), a blind Development control and missing input.
// Also checks the sentinel was registered at Foundation startup in this (seams-on) build.
// The scan of real packaged Shipping and Development builds is deferred to the nightly job.
// EditorContext only because the test runs a project script.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Internationalization/Regex.h"
#include "Server/Testing/StealthTestSeams.h"
#include "StealthBuildFileTestUtils.h"

namespace ShippingSymbolCheckTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/selftest/shipping-symbol-check/run-selftest.sh");
	const TCHAR* const SentinelPrefix = TEXT("STEALTH_TEST_SEAM_SENTINEL_");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("clean-pass"), TEXT("clean-pass-harness-file"),
		TEXT("seeded-shipping-sentinel-ascii"), TEXT("seeded-shipping-sentinel-utf16le"),
		TEXT("seeded-shipping-harness-file"), TEXT("seeded-shipping-harness-symbol"),
		TEXT("control-finds-nothing"), TEXT("control-sentinel-missing"),
		TEXT("missing-shipping-dir"), TEXT("empty-development-dir"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildShippingSymbolCheckTest, "StealthGame.Build.ShippingSymbolCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildShippingSymbolCheckTest::RunTest(const FString& Parameters)
{
	using namespace ShippingSymbolCheckTestData;
	using namespace StealthBuildTest;

	// The sentinel is registered at module startup in a seams-on build.
#if WITH_STEALTH_TEST_SEAMS
	const TCHAR* Sentinel = StealthGetTestSeamSentinel();
	if (TestNotNull(TEXT("Test-seam sentinel was registered at StealthFoundation startup"), Sentinel))
	{
		TestTrue(TEXT("Sentinel carries the scanned prefix"), FString(Sentinel).StartsWith(SentinelPrefix, ESearchCase::CaseSensitive));
	}
#else
	AddError(TEXT("WITH_STEALTH_TEST_SEAMS is 0 in a build that runs automation tests"));
#endif

	// The check script's selftest.
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
		FRegexMatcher CaseOk(FRegexPattern(FString::Printf(TEXT("SELFTEST %s expected=\\d got=\\d ok"), Case)), Run.Output);
		TestTrue(FString::Printf(TEXT("Selftest case %s gave its expected exit"), Case), CaseOk.FindNext());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
