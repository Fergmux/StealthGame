// Story cnb-036 (production/epics/crowd-npc-behaviour/story-036-no-crowd-ui.md): no crowd UI, debug draw or indicator
// (GDD AC-CNB-25; ADR-0016 Implementation Guidelines).
//   SourceScan   the crowd lint's crowd-ui rule is watched failing on planted draws (DrawDebug*, AddOnScreenDebugMessage,
//                the debug-draw, UMG, canvas, HUD and Presentation headers) and the real Crowd tree has none.
//   DebugCompiledOut  the crowd's instrumentation (the cost meter, the Look override, the virtual-input recording) is
//                compiled only outside Shipping and carries the test-seam sentinel prefix, so the nightly Shipping-symbol
//                check (tools/ci/shipping-symbol-check.sh) proves it absent from Shipping (this Development build is the
//                positive control).
// The Shipping screen captures of each viewer class need a packaged Shipping build: the nightly job's.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestUtils.h"
#include "Server/CrowdCostMeter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdNoUiSourceScanTest, "StealthGame.Crowd.NoUi.SourceScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdNoUiSourceScanTest::RunTest(const FString& Parameters)
{
	const StealthCrowdTest::FScriptRun Run = StealthCrowdTest::RunBashScript(TEXT("tools/ci/lint/selftest/run-crowd-lint-selftest.sh"));
	if (!TestTrue(TEXT("the crowd lint selftest ran (bash found)"), Run.bLaunched))
	{
		return false;
	}
	TestTrue(TEXT("the crowd-ui rule is watched failing on the planted draws"), Run.Output.Contains(TEXT("SELFTEST crowd-ui-seeded ok")));
	TestTrue(TEXT("the real Crowd tree draws nothing"), Run.Output.Contains(TEXT("SELFTEST real-tree-zero-findings ok")));
	if (HasAnyErrors())
	{
		AddInfo(Run.Output);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdNoUiDebugCompiledOutTest, "StealthGame.Crowd.NoUi.DebugCompiledOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdNoUiDebugCompiledOutTest::RunTest(const FString& Parameters)
{
#if !UE_BUILD_SHIPPING
	const FString Sentinel = FCrowdCostMeter::DebugSentinel();
	TestTrue(TEXT("the crowd debug sentinel carries the Shipping-symbol check's prefix"), Sentinel.StartsWith(TEXT("STEALTH_TEST_SEAM_SENTINEL_")));
	TestFalse(TEXT("the cost meter is off unless a harness turns it on"), FCrowdCostMeter::Get().bEnabled);
#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
