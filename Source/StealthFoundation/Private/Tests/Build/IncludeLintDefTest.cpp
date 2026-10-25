// Story tdc-013 (production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md):
// G2 wrapper for include lint rules (d)-(f) (ADR-0007 section 3, ADR-0008 sections 1 and 3).
// Runs tools/ci/lint/selftest/run-include-def-selftest.sh: seeded fixtures for Core including Features (d),
// Foundation including an upper module (e) and a header name shared across modules (f) are each reported
// as file:line with their letter; clean controls (Features including Core, the prototype exempt list)
// and the real tree give zero findings. It also runs the lint through the gate on the real tree.
// EditorContext only because the test runs project scripts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildIncludeLintDefTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/run-include-def-selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/include-lint-def.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = { TEXT("d-seeded"), TEXT("d-clean"), TEXT("e-seeded"), TEXT("e-clean"), TEXT("f-seeded"), TEXT("f-clean"), TEXT("f-prototype-exempt-allowed"), TEXT("f-prototype-unlisted-reported"), TEXT("real-tree-zero-findings") };

	/** Seeded observations (file:line plus the rule letter) the selftest must echo from the gate. */
	const TCHAR* const SeededObservations[] = {
		TEXT("Source/StealthGameplay/Core/StealthCoreBad.cpp:3 d "),
		TEXT("Source/StealthGameplay/Private/Core/StealthResolvedBad.cpp:1 d "),
		TEXT("Source/StealthFoundation/Private/StealthUpward.cpp:1 e "),
		TEXT("Source/StealthFoundation/Private/StealthUpward.cpp:4 e "),
		TEXT("Source/StealthPresentation/Private/StealthDupName.h:1 f "),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildIncludeLintDefTest, "StealthGame.Build.IncludeLintDef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildIncludeLintDefTest::RunTest(const FString& Parameters)
{
	using namespace BuildIncludeLintDefTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
