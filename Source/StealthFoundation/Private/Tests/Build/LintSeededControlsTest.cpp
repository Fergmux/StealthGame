// Story tdc-016 (production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md):
// G2 wrapper for the seeded controls of include lint rules (a)-(j) (ADR-0008 section 3, ADR-0007
// Validation 4 and 7). Runs tools/ci/lint/selftest/seeded-controls.sh: the fixture tree
// tools/ci/lint/fixtures/seeded holds exactly one seeded violation per rule and must produce ten
// findings, one per rule letter; the gate must fail on it and pass on the real tree; removing any
// one seed must drop exactly that rule's finding (the control is detected); and the fixture must
// lie outside the real-tree scan. It also runs all four rule scripts through the gate on the real
// tree. EditorContext only because the test runs project scripts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace LintSeededControlsTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/seeded-controls.sh");

	/** Selftest cases that must each report "ok": ten seeds reported, ten removals, and the two gate verdicts. */
	const TCHAR* const Cases[] = {
		TEXT("seeded-one-finding-per-rule"), TEXT("seeded-gate-fails"), TEXT("real-tree-passes"), TEXT("fixture-excluded-from-real-tree-scan"),
		TEXT("seed-a-reported-at-its-file"), TEXT("seed-b-reported-at-its-file"), TEXT("seed-c-reported-at-its-file"),
		TEXT("seed-d-reported-at-its-file"), TEXT("seed-e-reported-at-its-file"), TEXT("seed-f-reported-at-its-file"),
		TEXT("seed-g-reported-at-its-file"), TEXT("seed-h-reported-at-its-file"), TEXT("seed-i-reported-at-its-file"),
		TEXT("seed-j-reported-at-its-file"),
		TEXT("seed-a-removal-drops-only-a"), TEXT("seed-b-removal-drops-only-b"), TEXT("seed-c-removal-drops-only-c"),
		TEXT("seed-d-removal-drops-only-d"), TEXT("seed-e-removal-drops-only-e"), TEXT("seed-f-removal-drops-only-f"),
		TEXT("seed-g-removal-drops-only-g"), TEXT("seed-h-removal-drops-only-h"), TEXT("seed-i-removal-drops-only-i"),
		TEXT("seed-j-removal-drops-only-j"),
	};

	/** The ten seeded observations, one per rule letter, plus the gate's FAIL line. */
	const TCHAR* const SeededObservations[] = {
		TEXT("Source/StealthFoundation/Public/StealthLooseHeader.h:1 a "),
		TEXT("Source/StealthFoundation/Public/Server/StealthNoGuard.h:2 b "),
		TEXT("Source/StealthGameplay/Public/View/StealthLeakyView.h:3 c "),
		TEXT("Source/StealthGameplay/Private/Core/StealthCoreUsesFeature.cpp:2 d "),
		TEXT("Source/StealthFoundation/Private/StealthUpwardInclude.cpp:2 e "),
		TEXT("Source/StealthPresentation/Private/StealthDupName.h:1 f "),
		TEXT("Source/StealthGameplay/Public/Server/StealthBadClass.h:3 g "),
		TEXT("Source/StealthPresentation/Private/StealthHudSearch.cpp:4 h "),
		TEXT("Source/StealthGameplay/Private/StealthLogLeak.cpp:4 i "),
		TEXT("Source/StealthGameplay/Private/Features/CityGenerator/StealthCityGenerator.cpp:2 j "),
		TEXT("GATE: FAIL (10 finding(s)"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildLintSeededControlsTest, "StealthGame.Build.LintSeededControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildLintSeededControlsTest::RunTest(const FString& Parameters)
{
	using namespace LintSeededControlsTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript,
		{ TEXT("tools/ci/lint/include-lint-abc.sh"), TEXT("tools/ci/lint/include-lint-def.sh"), TEXT("tools/ci/lint/include-lint-gh.sh"), TEXT("tools/ci/lint/include-lint-ij.sh") },
		MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
