// Story tdc-015 (production/epics/tuning-data-config/story-015-include-lint-rules-i-j.md):
// G2 wrapper for include lint rules (i)-(j) (ADR-0007 section 3, ADR-0008 sections 1 and 3).
// Runs tools/ci/lint/selftest/run-include-ij-selftest.sh: each listed log, check and trace macro carrying a
// forbidden identifier (i) and a CityGenerator file including Server/Random/ (j) are reported as file:line
// with their letter; "Binding" inside a longer identifier, a forbidden name outside a macro and the real
// tree give zero findings. It also runs the lint through the gate on the real tree.
// EditorContext only because the test runs project scripts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildIncludeLintIjTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/run-include-ij-selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/include-lint-ij.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = { TEXT("i-seeded"), TEXT("i-clean"), TEXT("j-seeded"), TEXT("j-clean"), TEXT("real-tree-zero-findings") };

	/** Seeded observations (file:line plus the rule letter) the selftest must echo from the gate. */
	const TCHAR* const SeededObservations[] = {
		TEXT("Source/StealthGameplay/Private/StealthLogLeaks.cpp:4 i "),
		TEXT("Source/StealthGameplay/Private/StealthLogLeaks.cpp:6 i "),
		TEXT("Source/StealthGameplay/Private/StealthLogLeaks.cpp:8 i "),
		TEXT("Source/StealthGameplay/Private/StealthLogLeaks.cpp:11 i "),
		TEXT("Source/StealthGameplay/Private/StealthLogLeaks.cpp:13 i "),
		TEXT("Source/StealthGameplay/Private/Features/CityGenerator/StealthCityGenTick.cpp:2 j "),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildIncludeLintIjTest, "StealthGame.Build.IncludeLintIj",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildIncludeLintIjTest::RunTest(const FString& Parameters)
{
	using namespace BuildIncludeLintIjTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
