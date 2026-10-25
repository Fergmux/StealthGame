// Story tdc-033 (production/epics/tuning-data-config/story-033-doc-lint-registry-match.md):
// G2 wrapper for the registry-match doc lint (tools/ci/lint/knob-lint-registry.sh; TDC GDD AC-3b bullet 3 registry half, CR-15.7). Runs the selftest: one seeded mismatch per compared field (source GDD, default, hard range, safe range, audience, latch, name) is reported per field naming key and field, a missing constant is reported once, and 60 versus 60.0 and the other equivalent spellings are silent; then runs the lint through the gate on the real tree.
// EditorContext only because the test runs project scripts (bash, and Python run as `python -I`; set STEALTH_BASH or
// STEALTH_PYTHON to override the interpreters the scripts find).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildKnobLintRegistryTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/knob_lint_registry_selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/knob-lint-registry.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("equivalent-spellings-silent"),
		TEXT("one-finding-per-mismatching-field"),
		TEXT("missing-constant-is-one-observation-not-one-per-field"),
		TEXT("matching-key-is-silent"),
		TEXT("finding-is-at-the-owner-row-line"),
		TEXT("schema-constant-and-catalog-flag-rows-not-compared"),
		TEXT("missing-registry-is-not-assessed"),
		TEXT("base-fixture-clean"),
		TEXT("real-tree-zero-findings"),
	};

	/** Seeded observations the selftest must echo. */
	const TCHAR* const SeededObservations[] = {
		TEXT("knob-registry key KDefault: default differs: owner row '5' vs registry '6'"),
		TEXT("knob-registry key KHard: hard range differs: owner row '1-9' vs registry '1-10'"),
		TEXT("knob-registry key KSafe: safe range differs: owner row '2-4' vs registry '2-5'"),
		TEXT("knob-registry key KAudience: audience differs: owner row 'ServerOnly' vs registry 'All'"),
		TEXT("knob-registry key KLatch: latch differs: owner row 'Round' vs registry 'Use'"),
		TEXT("knob-registry key KSource: source GDD differs"),
		TEXT("knob-registry key NameSec: name differs from registry constant 'nameSec'"),
		TEXT("knob-registry key KMissing: missing constant in design/registry/entities.yaml"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildKnobLintRegistryTest, "StealthGame.Build.KnobLintRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildKnobLintRegistryTest::RunTest(const FString& Parameters)
{
	using namespace BuildKnobLintRegistryTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
