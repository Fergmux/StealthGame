// Story tdc-005 (production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md):
// G2 wrapper for the compile-probe gate (ADR-0008 section 7). Runs
// tools/ci/compile-probes/selftest/run-selftest.sh, which feeds canned observation files for every
// outcome to gate-probes.sh (clean pass, probe compiled, wrong error text, control failed or missing,
// no probes, empty or missing file, failed baseline build, probe or control not compiled by UBT). It also checks the guard probe's
// EXPECT-ERROR substring is still part of StealthServerOnlyGuard.h's #error text, and that the
// placeholder Server header includes the guard first. The real UBT -SingleFile run on a disposable
// worktree (run-probes.ps1) belongs to the nightly job (story tdc-023).
// EditorContext only because the test reads project files and runs a project script.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Internationalization/Regex.h"
#include "StealthBuildFileTestUtils.h"

namespace CompileProbesTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/compile-probes/selftest/run-selftest.sh");
	const TCHAR* const GuardProbe = TEXT("tools/ci/compile-probes/ServerHeaderInPresentation.cpp");
	const TCHAR* const GuardHeader = TEXT("Source/StealthFoundation/Public/StealthServerOnlyGuard.h");
	const TCHAR* const ProbeTargetHeader = TEXT("Source/StealthFoundation/Public/Server/StealthGuardProbeTarget.h");

	/** Selftest cases that must each report "ok" (one per gate outcome). */
	const TCHAR* const Cases[] = {
		TEXT("pass"), TEXT("probe-compiled"), TEXT("wrong-error-text"), TEXT("control-failed"), TEXT("control-missing"), TEXT("probe-not-compiled"), TEXT("control-not-compiled"),
		TEXT("mixed-pass-and-fail"), TEXT("no-probes"), TEXT("empty-file"), TEXT("missing-file"), TEXT("baseline-build-failed"),
	};

	/** Value of a "// KEY: value" probe header (quotes removed), or empty. */
	FString ReadProbeHeader(const FString& Body, const FString& Key)
	{
		FRegexMatcher Matcher(FRegexPattern(FString::Printf(TEXT("(?m)^//\\s*%s\\s*:\\s*\"?([^\"\\r\\n]+)\"?"), *Key)), Body);
		return Matcher.FindNext() ? Matcher.GetCaptureGroup(1).TrimStartAndEnd() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildCompileProbesTest, "StealthGame.Build.CompileProbes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildCompileProbesTest::RunTest(const FString& Parameters)
{
	using namespace CompileProbesTestData;
	using namespace StealthBuildTest;

	// The probe's expected text is the guard's #error text, and its headers name the modules.
	FString Probe, Guard, ProbeTarget;
	if (TestTrue(TEXT("Guard probe file exists"), LoadProjectText(GuardProbe, Probe))
		&& TestTrue(TEXT("Guard header exists"), LoadProjectText(GuardHeader, Guard)))
	{
		const FString Expect = ReadProbeHeader(Probe, TEXT("EXPECT-ERROR"));
		TestFalse(TEXT("Probe has an EXPECT-ERROR header"), Expect.IsEmpty());
		TestTrue(TEXT("EXPECT-ERROR substring is part of the guard #error text"),
			!Expect.IsEmpty() && Guard.Contains(TEXT("#error \"") + Expect, ESearchCase::CaseSensitive));
		TestEqual(TEXT("Probe TARGET-MODULE"), ReadProbeHeader(Probe, TEXT("TARGET-MODULE")), FString(TEXT("StealthPresentation")));
		TestEqual(TEXT("Probe CONTROL-MODULE"), ReadProbeHeader(Probe, TEXT("CONTROL-MODULE")), FString(TEXT("StealthGame")));
		TestTrue(TEXT("Probe includes the placeholder Server header"), Probe.Contains(TEXT("#include \"Server/StealthGuardProbeTarget.h\"")));
	}
	if (TestTrue(TEXT("Placeholder Server header exists"), LoadProjectText(ProbeTargetHeader, ProbeTarget)))
	{
		const int32 FirstInclude = ProbeTarget.Find(TEXT("#include"));
		TestEqual(TEXT("Placeholder Server header includes StealthServerOnlyGuard.h first"),
			ProbeTarget.Find(TEXT("#include \"StealthServerOnlyGuard.h\"")), FirstInclude);
	}

	// The gate selftest: clean pass plus every seeded violation detected.
	const FString Bash = FindBash();
	if (!TestFalse(TEXT("A bash for the CI scripts was found (set STEALTH_BASH to override)"), Bash.IsEmpty()))
	{
		return false;
	}
	const FScriptRun Run = RunBashScript(Bash, SelftestScript);
	AddInfo(Run.Output);
	if (!TestTrue(TEXT("Selftest script launched"), Run.bLaunched))
	{
		return false;
	}
	TestEqual(TEXT("Selftest exit code"), Run.ExitCode, 0);
	TestTrue(TEXT("Selftest reports PASS"), Run.Output.Contains(TEXT("SELFTEST RESULT: PASS")));
	for (const TCHAR* Case : Cases)
	{
		FRegexMatcher CaseOk(FRegexPattern(FString::Printf(TEXT("SELFTEST %s expected=\\d got=\\d ok"), Case)), Run.Output);
		TestTrue(FString::Printf(TEXT("Selftest case %s gave its expected gate exit"), Case), CaseOk.FindNext());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
