// Story tdc-024 (production/epics/tuning-data-config/story-024-vcs-conformance-check.md):
// G2 wrapper for the VCS conformance check (ADR-0009 sections 2-3). Runs
// tools/ci/lint/selftest/run-vcs-conformance-selftest.sh: a compliant ignore/attribute pair passes,
// each single violation (Build/ ignored, a required folder not ignored, two patterns on one
// .gitattributes line, *.uasset or *.umap without `lockable` or missing, a leftover Godot or Unity
// header or pattern) is reported with its file and line, a folder that is not a git checkout is
// NOT ASSESSED, and the real repository conforms. It also runs the check through the gate on the
// real repository. EditorContext only because the test runs project scripts and git.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace VcsConformanceTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/run-vcs-conformance-selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/vcs-conformance.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("compliant-passes"), TEXT("build-ignored-reported"), TEXT("lowercase-build-rule-handled"),
		TEXT("missing-ignore-_snapshots-reported"), TEXT("missing-ignore-_backup_pre_rebase-reported"),
		TEXT("missing-ignore-Plugins----Binaries-reported"), TEXT("missing-ignore-Plugins----Intermediate-reported"),
		TEXT("missing-ignore-Binaries-reported"), TEXT("missing-ignore-Intermediate-reported"),
		TEXT("missing-ignore-DerivedDataCache-reported"), TEXT("missing-ignore-Saved-reported"),
		TEXT("two-patterns-on-one-line-reported"), TEXT("slash-token-reported"),
		TEXT("uasset-not-lockable-reported"), TEXT("umap-not-lockable-reported"), TEXT("umap-line-missing-reported"),
		TEXT("godot-header-reported"), TEXT("unity-header-case-insensitive-reported"),
		TEXT("pattern--godot--reported"), TEXT("pattern---import-reported"), TEXT("pattern-Library--reported"),
		TEXT("pattern-Temp--reported"), TEXT("pattern-UnityLockfile-reported"),
		TEXT("not-a-repository-not-assessed"), TEXT("real-repo-conforms"),
	};

	/** Seeded observations the selftest echoes from the gate. */
	const TCHAR* const SeededObservations[] = {
		TEXT(".gitignore:0 vcs Build/Scripts/x.cs is ignored"),
		TEXT(".gitattributes:9 vcs '*.psd' after pattern '*.blend'"),
		TEXT(".gitattributes:0 vcs the *.uasset line lacks 'lockable'"),
		TEXT(".gitignore:19 vcs Godot/Unity section header remains"),
		TEXT("GATE: NOT ASSESSED"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildVcsConformanceTest, "StealthGame.Build.VcsConformance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildVcsConformanceTest::RunTest(const FString& Parameters)
{
	using namespace VcsConformanceTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
