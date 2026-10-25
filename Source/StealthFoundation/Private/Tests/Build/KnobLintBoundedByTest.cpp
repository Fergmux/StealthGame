// Story tdc-036 (production/epics/tuning-data-config/story-036-doc-lint-bounded-by.md):
// G2 wrapper for the bounded-by doc lint (tools/ci/lint/knob-lint-bounded-by.sh; ADR-0004 section 4, the C-W18 pattern). Runs the selftest: a conforming row passes, a non-conforming row is reported per key and invariant at its file:line, a key read by two invariants needs both ids, the baseline hides a listed pair and reports a stale one, and a baseline that grows fails the shrink-only check against a temporary repository with a base commit; then runs the lint through the gate on the real tree (its accepted findings are baselined, citing TD-REG-2).
// EditorContext only because the test runs project scripts (bash, and Python run as `python -I`; set STEALTH_BASH or
// STEALTH_PYTHON to override the interpreters the scripts find).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildKnobLintBoundedByTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/knob_lint_bounded_by_selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/knob-lint-bounded-by.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("conforming-rows-pass"),
		TEXT("nonconforming-row-reported-with-file-and-line"),
		TEXT("bullet-invariant-read-like-a-table-row"),
		TEXT("below-by-owner-word-variant-conforms"),
		TEXT("longer-id-does-not-satisfy-a-shorter-one"),
		TEXT("mentioning-the-id-without-bounded-by-does-not-conform"),
		TEXT("second-invariant-needs-its-own-text"),
		TEXT("both-ids-present-passes"),
		TEXT("range-row-reported-once-naming-the-range"),
		TEXT("any-id-of-the-range-satisfies-it"),
		TEXT("baselined-pair-is-not-reported"),
		TEXT("stale-entry-reported-so-the-list-shrinks"),
		TEXT("entry-without-a-citation-or-in-another-shape-reported"),
		TEXT("baseline-growth-fails-against-the-base"),
		TEXT("unchanged-baseline-is-clean"),
		TEXT("shrinking-baseline-is-clean"),
		TEXT("commit-creating-the-baseline-is-exempt"),
		TEXT("unresolvable-base-ref-fails-exit"),
		TEXT("unresolvable-base-ref-is-reported"),
		TEXT("non-git-tree-with-a-baseline-fails-exit"),
		TEXT("non-git-tree-baseline-is-unverifiable"),
		TEXT("real-tree-zero-findings"),
	};

	/** Seeded observations the selftest must echo. */
	const TCHAR* const SeededObservations[] = {
		TEXT("design/gdd/beta-system.md:10 knob-bounded-by key BetaLimitCount is read by invariant I-AB-2"),
		TEXT("design/gdd/alpha-system.md:13 knob-bounded-by key AlphaHoldSec is read by invariant I-AB-4"),
		TEXT("knob-baseline-stale baseline entry"),
		TEXT("knob-baseline-malformed"),
		TEXT("tools/ci/lint/knob-bounded-by-baseline.txt:3 shrink-only entry"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildKnobLintBoundedByTest, "StealthGame.Build.KnobLintBoundedBy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildKnobLintBoundedByTest::RunTest(const FString& Parameters)
{
	using namespace BuildKnobLintBoundedByTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
