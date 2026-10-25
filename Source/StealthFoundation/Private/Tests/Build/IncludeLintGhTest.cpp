// Story tdc-014 (production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md):
// G2 wrapper for include lint rules (g)-(h) (ADR-0007 section 3, ADR-0008 sections 1 and 3).
// Runs tools/ci/lint/selftest/run-include-gh-selftest.sh: seeded server types without the Blueprint-hiding
// specifiers or with Blueprint specifiers on members (g), and presentation source using the forbidden world
// searches or an unannotated GetPlayerState/GetController (h), are each reported as file:line with their
// letter; a correct UCLASS/UINTERFACE, annotated lines and API names in comments or strings, and the real
// tree, give zero findings. It also runs the lint through the gate on the real tree.
// EditorContext only because the test runs project scripts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildIncludeLintGhTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/run-include-gh-selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/include-lint-gh.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = { TEXT("g-seeded"), TEXT("g-clean"), TEXT("h-seeded"), TEXT("h-clean"), TEXT("real-tree-zero-findings") };

	/** Seeded observations (file:line plus the rule letter) the selftest must echo from the gate. */
	const TCHAR* const SeededObservations[] = {
		TEXT("Source/StealthFoundation/Public/Server/StealthBadServerTypes.h:3 g "),
		TEXT("Source/StealthFoundation/Public/Server/StealthBadServerTypes.h:9 g "),
		TEXT("Source/StealthFoundation/Public/Server/StealthBadServerTypes.h:21 g "),
		TEXT("Source/StealthPresentation/Private/StealthBadHud.cpp:4 h "),
		TEXT("Source/StealthPresentation/Private/StealthBadHud.cpp:12 h "),
		TEXT("Source/StealthPresentation/Private/StealthBadHud.cpp:17 h "),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildIncludeLintGhTest, "StealthGame.Build.IncludeLintGh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildIncludeLintGhTest::RunTest(const FString& Parameters)
{
	using namespace BuildIncludeLintGhTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
