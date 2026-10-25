// Story tdc-029 (production/epics/tuning-data-config/story-029-knob-table-parser.md):
// G2 wrapper for the Tuning Knobs table parser (tools/ci/lint/knob_tables.py; GDD CR-15, AC-3b). Runs the selftest: fixture GDDs with a standard table, a paired row, a reordered table, an owner-level preamble table, a consumed-elsewhere table, a retired row and a reference row; a seeded count mismatch, duplicate key and missing section are reported as observations; and the real design/gdd/*.md are parsed.
// EditorContext only because the test runs project scripts (bash, and Python run as `python -I`; set STEALTH_BASH or
// STEALTH_PYTHON to override the interpreters the scripts find).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildKnobTableParserTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/knob_tables_selftest.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("record-count-per-gdd"),
		TEXT("pair-row-split-per-key"),
		TEXT("column-order-variant-mapped-by-name"),
		TEXT("preamble-style-inherits-owner-audience-latch"),
		TEXT("standard-style-has-nothing-inherited"),
		TEXT("consumed-table-row-is-not-ownership"),
		TEXT("retired-row-is-listed-not-owned"),
		TEXT("retired-row-has-no-knob-record"),
		TEXT("reference-row-is-listed-not-owned"),
		TEXT("reference-row-does-not-double-own-AlphaMaxCount"),
		TEXT("record-carries-gdd-and-line"),
		TEXT("table-record-lists-header-cells"),
		TEXT("clean-fixture-scanned"),
		TEXT("clean-fixture-no-observations"),
		TEXT("seeded-count-mismatch-reported"),
		TEXT("seeded-count-mismatch-is-the-only-observation"),
		TEXT("seeded-duplicate-key-reported"),
		TEXT("seeded-missing-section-reported"),
		TEXT("seeded-short-row-reported"),
		TEXT("real-tree-record-count"),
		TEXT("real-tree-owned-table-per-gdd"),
		TEXT("real-tree-every-section-yields-records"),
		TEXT("real-tree-preamble-style-lsc"),
		TEXT("real-tree-preamble-style-generator"),
		TEXT("real-tree-pair-tail-expansion"),
		TEXT("real-tree-ellipsis-expansion"),
		TEXT("real-tree-whole-key-after-slash"),
		TEXT("real-tree-no-retired-key-record"),
		TEXT("real-tree-observations-keep-the-contract"),
	};

	/** Seeded observations the selftest must echo. */
	const TCHAR* const SeededObservations[] = {
		TEXT("design/gdd/tuning-data-config.md:9 knob-count design/gdd/alpha-system.md: Registered Owner Keys says 5"),
		TEXT("design/gdd/alpha-system.md:20 knob-duplicate-key key AlphaHoldSec"),
		TEXT("design/gdd/gamma-system.md:0 knob-no-section GDD has no Tuning Knobs section"),
		TEXT("design/gdd/beta-system.md:10 knob-row-shape row has 6 cells, header has 7"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildKnobTableParserTest, "StealthGame.Build.KnobTableParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildKnobTableParserTest::RunTest(const FString& Parameters)
{
	using namespace BuildKnobTableParserTestData;
	using namespace StealthBuildTest;

	return RunBashSelftest(*this, SelftestScript, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
