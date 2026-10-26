// Story st-012 (production/epics/session-travel/story-012-forbidden-rng-ci-grep-and-include-lint.md):
// G2 wrapper for the forbidden-RNG CI grep (ADR-0006 Implementation Guideline 1 and Validation Criteria). Runs
// tools/ci/lint/selftest/run-forbidden-rng-selftest.sh: every planted FMath::Rand*/FRand*/RandRange/SRand*/VRand*,
// FRandomStream, std::rand, <random> engine or distribution and #include <random> is reported as file:line rng (the
// gate watched failing first), clean controls and an allowlisted file give zero findings, and the real tree passes.
// The Generator include rule (j) is proven by StealthGame.Build.IncludeLintIj. EditorContext: runs project scripts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildForbiddenRngLintTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/run-forbidden-rng-selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/forbidden-rng.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = { TEXT("rng-seeded"), TEXT("rng-clean"), TEXT("rng-allowlist"), TEXT("real-tree-zero-findings") };

	/** Seeded observations (file:line plus the rule id) the selftest must echo from the gate. */
	const TCHAR* const SeededObservations[] = {
		TEXT("Source/StealthGameplay/Private/StealthRngLeaks.cpp:2 rng "),
		TEXT("Source/StealthGameplay/Private/StealthRngLeaks.cpp:3 rng "),
		TEXT("Source/StealthGameplay/Private/StealthRngLeaks.cpp:7 rng "),
		TEXT("Source/StealthGameplay/Private/StealthRngLeaks.cpp:8 rng "),
		TEXT("Source/StealthGameplay/Private/StealthRngLeaks.cpp:9 rng "),
		TEXT("Source/StealthFoundation/Private/StealthRngLeaks2.h:3 rng "),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildForbiddenRngLintTest, "StealthGame.Build.ForbiddenRngLint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildForbiddenRngLintTest::RunTest(const FString& Parameters)
{
	using namespace BuildForbiddenRngLintTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
