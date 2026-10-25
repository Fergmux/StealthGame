// Story tdc-012 (production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md):
// G2 wrapper for include lint rules (a)-(c) (ADR-0007 section 3, ADR-0008 sections 1 and 3).
// Runs tools/ci/lint/selftest/run-include-abc-selftest.sh: one seeded fixture per rule letter is
// reported as file:line with its letter, each clean control and the real tree give zero findings,
// the prototype exempt list suppresses (a)/(b) for listed headers only, and the list may only
// shrink (G3). It also runs the lint through the gate on the real tree directly.
// EditorContext only because the test runs project scripts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace IncludeLintAbcTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/run-include-abc-selftest.sh");
	const TCHAR* const GateScript = TEXT("tools/ci/lint/gate.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/include-lint-abc.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("a-seeded"), TEXT("a-clean"), TEXT("b-seeded"), TEXT("b-clean"), TEXT("c-seeded"), TEXT("c-clean"),
		TEXT("prototype-exempt-and-unlisted"), TEXT("real-tree-zero-findings"), TEXT("shrink-only-unchanged"),
		TEXT("shrink-only-removal-allowed"), TEXT("shrink-only-growth-reported"), TEXT("shrink-only-unverifiable-base-reported"),
	};

	/** One seeded observation per rule letter, as file:line plus the letter. */
	const TCHAR* const SeededObservations[] = {
		TEXT("Source/StealthGameplay/Public/StealthLooseHeader.h:1 a "),
		TEXT("Source/StealthFoundation/Public/Server/StealthGuardNotFirst.h:3 b "),
		TEXT("Source/StealthGameplay/Public/View/StealthLeakyView.h:4 c "),
		TEXT("Source/StealthGame/Public/StealthUnlistedNew.h:1 a "),
		TEXT("tools/ci/lint/prototype-exempt.txt:3 shrink-only "),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildIncludeLintAbcTest, "StealthGame.Build.IncludeLintAbc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildIncludeLintAbcTest::RunTest(const FString& Parameters)
{
	using namespace IncludeLintAbcTestData;
	using namespace StealthBuildTest;

	const FString Bash = FindBash();
	if (!TestFalse(TEXT("A bash for the CI scripts was found (set STEALTH_BASH to override)"), Bash.IsEmpty()))
	{
		return false;
	}

	// Seeded fixtures, clean controls, prototype exemptions and G3.
	const FScriptRun Selftest = RunBashScript(Bash, SelftestScript);
	AddInfo(Selftest.Output);
	if (TestTrue(TEXT("Selftest script launched"), Selftest.bLaunched))
	{
		TestEqual(TEXT("Selftest exit code"), Selftest.ExitCode, 0);
		TestTrue(TEXT("Selftest reports PASS"), Selftest.Output.Contains(TEXT("SELFTEST RESULT: PASS")));
		for (const TCHAR* Case : Cases)
		{
			TestTrue(FString::Printf(TEXT("Selftest case %s ok"), Case), Selftest.Output.Contains(FString::Printf(TEXT("SELFTEST %s ok"), Case)));
		}
		for (const TCHAR* Observation : SeededObservations)
		{
			TestTrue(FString::Printf(TEXT("Seeded violation reported: '%s'"), Observation), Selftest.Output.Contains(Observation));
		}
	}

	// The real tree, through the gate: exit 0 and PASS.
	const FScriptRun RealTree = RunBashScript(Bash, GateScript, FString::Printf(TEXT("\"%s\""), *ProjectPath(RuleScript)));
	AddInfo(RealTree.Output);
	if (TestTrue(TEXT("Gate on the real tree launched"), RealTree.bLaunched))
	{
		TestEqual(TEXT("Real tree: gate exit code 0 (zero findings)"), RealTree.ExitCode, 0);
		TestTrue(TEXT("Real tree: GATE: PASS"), RealTree.Output.Contains(TEXT("GATE: PASS")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
