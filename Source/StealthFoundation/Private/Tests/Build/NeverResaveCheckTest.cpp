// Story tdc-026 (production/epics/tuning-data-config/story-026-never-resave-commit-check.md):
// G2 wrapper for the pre-commit never-resave check (ADR-0009 section 4 and Validation 4). Runs
// tools/ci/hooks/selftest/run-never-resave-selftest.sh: a staged change to a listed asset is rejected naming the path
// (also a rename or deletion of it), an unlisted change is accepted, the table header, separator and prose are not
// parsed as paths, case and slash differences match, a missing or empty list is NOT ASSESSED, and the real list
// parses to its five camera assets.
// EditorContext only because the test runs project scripts and git. Nothing is installed in the real working copy.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace NeverResaveCheckTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/hooks/selftest/run-never-resave-selftest.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("no-staged-files-accepted"), TEXT("listed-modification-rejected-names-path"),
		TEXT("unlisted-change-accepted"), TEXT("quoted-backslash-row-is-a-path"),
		TEXT("not-parsed-as-path-Mention"), TEXT("not-parsed-as-path-Column"), TEXT("header-row-not-a-path"),
		TEXT("case-and-slash-differences-matched"), TEXT("rename-of-listed-asset-rejected"),
		TEXT("rename-onto-listed-path-rejected"), TEXT("deletion-of-listed-asset-rejected"),
		TEXT("missing-list-not-assessed"), TEXT("empty-list-not-assessed"), TEXT("real-list-parses-five-paths"),
		TEXT("hook-rejects-listed-change"), TEXT("hook-accepts-unlisted-change"),
	};

	/** Observations the selftest echoes from the script under test. */
	const TCHAR* const Observations[] = {
		TEXT("never-resave staged modification of a never-resave asset"),
		TEXT("never-resave: REJECT (1 listed path(s)"),
		TEXT("PRE-COMMIT check-never-resave: REJECTED"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildNeverResaveCheckTest, "StealthGame.Build.NeverResaveCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildNeverResaveCheckTest::RunTest(const FString& Parameters)
{
	using namespace NeverResaveCheckTestData;
	using namespace StealthBuildTest;

	return RunBashSelftest(*this, SelftestScript, MakeArrayView(Cases), MakeArrayView(Observations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
