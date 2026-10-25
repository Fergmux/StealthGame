// Story tdc-031 (production/epics/tuning-data-config/story-031-doc-lint-cr15-columns.md):
// G2 wrapper for the CR-15 column doc lint (tools/ci/lint/knob-lint-columns.sh; TDC GDD AC-3b bullet 1). Runs the selftest: a table missing a column is reported naming file and column; a complete table, reordered or extra columns, owner-level preamble tables and consumed tables are silent; then runs the lint through the gate on the real design/gdd/*.md.
// EditorContext only because the test runs project scripts (bash, and Python run as `python -I`; set STEALTH_BASH or
// STEALTH_PYTHON to override the interpreters the scripts find).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildKnobLintColumnsTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/knob_lint_columns_selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/knob-lint-columns.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("complete-table-silent"),
		TEXT("effect-matched-by-leading-word"),
		TEXT("missing-latch-reported"),
		TEXT("missing-effect-reported"),
		TEXT("missing-hard-range-reported"),
		TEXT("each-missing-column-is-its-own-finding"),
		TEXT("reordered-columns-silent"),
		TEXT("extra-column-silent"),
		TEXT("two-tables-checked-one-by-one"),
		TEXT("preamble-supplies-owner-audience-latch-silent"),
		TEXT("preamble-without-latch-reported"),
		TEXT("preamble-audience-only-reports-owner-and-latch"),
		TEXT("table-without-preamble-reports-all-three"),
		TEXT("consumed-elsewhere-table-silent"),
		TEXT("no-owned-table-is-not-assessed"),
		TEXT("empty-scope-is-not-assessed"),
		TEXT("base-fixture-clean"),
		TEXT("real-tree-zero-findings"),
	};

	/** Seeded observations the selftest must echo. */
	const TCHAR* const SeededObservations[] = {
		TEXT("design/gdd/one.md:5 knob-columns Tuning Knobs table is missing the CR-15 column 'Latch'"),
		TEXT("design/gdd/one.md:11 knob-columns Tuning Knobs table is missing the CR-15 column 'Unit'"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildKnobLintColumnsTest, "StealthGame.Build.KnobLintColumns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildKnobLintColumnsTest::RunTest(const FString& Parameters)
{
	using namespace BuildKnobLintColumnsTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
