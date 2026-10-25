// Story tdc-025 (production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md):
// G2 wrapper for the tracked pre-commit hook framework and its installer (ADR-0009 section 5 and Validation 5).
// Runs tools/ci/hooks/selftest/run-pre-commit-selftest.sh: in temporary git repositories holding a copy of the
// hooks, the installer sets core.hooksPath (idempotent, refuses a foreign value, never edits a tracked file, writes
// the per-clone LFS hooks), a clean commit is accepted, a commit with a seeded lint violation is rejected with the
// finding printed, a new check-*.sh plugs in without editing the hook body, and CRLF shims still work.
// EditorContext only because the test runs project scripts and git. Nothing is installed in the real working copy.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace PreCommitHookTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/hooks/selftest/run-pre-commit-selftest.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("installer-sets-hookspath"), TEXT("installer-idempotent"),
		TEXT("installer-leaves-tracked-files-alone"), TEXT("installer-writes-lfs-hooks-untracked-and-ignored"),
		TEXT("installer-status-installed"), TEXT("installer-refuses-foreign-hookspath"),
		TEXT("installer-force-replaces"), TEXT("installer-uninstall"), TEXT("installer-powershell-twin"),
		TEXT("no-staged-files-passes"), TEXT("clean-commit-accepted"), TEXT("seeded-lint-violation-rejected"),
		TEXT("check-contract-reject-aborts"), TEXT("check-contract-not-assessed-aborts"),
		TEXT("check-contract-pass-and-env"), TEXT("crlf-shims-still-work"), TEXT("hook-rules-match-gate-rules"),
	};

	/** Observations the selftest echoes from the script under test. */
	const TCHAR* const Observations[] = {
		TEXT("PRE-COMMIT: REJECTED - the commit was not made"),
		TEXT("Source/StealthFoundation/Public/Server/StealthNoGuard.h:2 b first include"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPreCommitHookTest, "StealthGame.Build.PreCommitHook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildPreCommitHookTest::RunTest(const FString& Parameters)
{
	using namespace PreCommitHookTestData;
	using namespace StealthBuildTest;

	return RunBashSelftest(*this, SelftestScript, MakeArrayView(Cases), MakeArrayView(Observations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
