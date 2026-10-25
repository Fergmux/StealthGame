// Story tdc-032 (production/epics/tuning-data-config/story-032-doc-lint-one-owner.md):
// G2 wrapper for the exactly-one-owner doc lint (tools/ci/lint/knob-lint-owner.sh; TDC GDD AC-3b bullet 2, CR-15.3). Runs the selftest: a key owned by two GDDs is reported with both file names, a registry constant no GDD owns is reported, a single owner and a key only referenced elsewhere are silent; then runs the lint through the gate on the real tree.
// EditorContext only because the test runs project scripts (bash, and Python run as `python -I`; set STEALTH_BASH or
// STEALTH_PYTHON to override the interpreters the scripts find).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildKnobLintOwnerTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/knob_lint_owner_selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/knob-lint-owner.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("single-owner-silent"),
		TEXT("two-owners-reported-with-both-files"),
		TEXT("one-gdd-owning-a-key-twice-reported"),
		TEXT("zero-owner-reported-at-the-registry-line"),
		TEXT("deprecated-constant-is-not-in-the-universe"),
		TEXT("referenced-not-owned-silent"),
		TEXT("renamed-key-shows-as-zero-owner-under-its-old-name"),
		TEXT("retired-row-is-not-a-second-owner"),
		TEXT("missing-registry-is-not-assessed"),
		TEXT("base-fixture-clean"),
		TEXT("real-tree-zero-findings"),
	};

	/** Seeded observations the selftest must echo. */
	const TCHAR* const SeededObservations[] = {
		TEXT("design/gdd/alpha.md:7 knob-owner key SharedSec is owned by 2 Tuning Knobs rows (design/gdd/alpha.md:7 and design/gdd/beta.md:7)"),
		TEXT("design/registry/entities.yaml:5 knob-owner registry constant OrphanSec"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildKnobLintOwnerTest, "StealthGame.Build.KnobLintOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildKnobLintOwnerTest::RunTest(const FString& Parameters)
{
	using namespace BuildKnobLintOwnerTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
