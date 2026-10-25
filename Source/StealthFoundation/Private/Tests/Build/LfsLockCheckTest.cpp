// Story tdc-027 (production/epics/tuning-data-config/story-027-lfs-lock-commit-check.md):
// G2 wrapper for the pre-commit LFS lock check (ADR-0009 section 4 and Validation 2). Runs
// tools/ci/hooks/selftest/run-lfs-locks-selftest.sh with an injected lock source (no remote): a lock held by the
// committer is accepted, no lock or another owner's lock is rejected naming the path, a remote without lock support
// degrades to a printed single-writer reminder, other source failures are NOT ASSESSED, non-asset changes are ignored.
// EditorContext only because the test runs project scripts and git. Nothing is installed in the real working copy.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace LfsLockCheckTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/hooks/selftest/run-lfs-locks-selftest.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("lock-held-by-committer-accepted"), TEXT("no-lock-rejected-names-path"),
		TEXT("other-owner-lock-rejected"), TEXT("unsupported-remote-degrades-with-reminder"),
		TEXT("no-remote-degrades-with-reminder"), TEXT("source-failure-not-assessed"),
		TEXT("malformed-source-line-not-assessed"), TEXT("non-asset-change-ignored"),
		TEXT("several-assets-one-unlocked-names-only-it"), TEXT("umap-checked"),
		TEXT("uppercase-extension-checked"), TEXT("lock-path-case-insensitive"), TEXT("deletion-needs-lock"),
		TEXT("rename-needs-lock-on-old-path"), TEXT("rename-with-lock-on-old-path-accepted"),
		TEXT("new-asset-needs-no-lock"), TEXT("new-asset-locked-by-other-rejected"),
		TEXT("hook-rejects-unlocked-asset"), TEXT("hook-accepts-locked-asset"),
	};

	/** Observations the selftest echoes from the script under test. */
	const TCHAR* const Observations[] = {
		TEXT("lfs-lock staged modification of Content/Props/SM_Crate.uasset without a lock held by you (it is not locked)"),
		TEXT("NOTE single-writer"),
		TEXT("lfs-locks: REJECT (1 changed asset(s)"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildLfsLockCheckTest, "StealthGame.Build.LfsLockCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildLfsLockCheckTest::RunTest(const FString& Parameters)
{
	using namespace LfsLockCheckTestData;
	using namespace StealthBuildTest;

	return RunBashSelftest(*this, SelftestScript, MakeArrayView(Cases), MakeArrayView(Observations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
