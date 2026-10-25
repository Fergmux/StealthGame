// Story tdc-030 (production/epics/tuning-data-config/story-030-registry-mirror-completion.md):
// G2 wrapper for the registry mirror (tools/ci/lint/registry_mirror.py; GDD CR-15.7, OQ-15, TD-REG-1). Runs the selftest: a missing key is added with every field, existing constants (including an altered one) stay byte-identical, the file still parses with no duplicate names, a second run adds nothing, a CRLF registry keeps its line endings, an unsupported layout writes nothing, and the real tree needs no further mirroring.
// EditorContext only because the test runs project scripts (bash, and Python run as `python -I`; set STEALTH_BASH or
// STEALTH_PYTHON to override the interpreters the scripts find).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace BuildRegistryMirrorTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/registry_mirror_selftest.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("mirror-exit-zero"),
		TEXT("missing-key-reported-and-added"),
		TEXT("added-constant-has-every-field"),
		TEXT("old-text-is-byte-identical"),
		TEXT("existing-constant-not-altered"),
		TEXT("altered-existing-constant-left-alone"),
		TEXT("file-parses-with-no-duplicate-names"),
		TEXT("second-run-adds-nothing"),
		TEXT("second-run-leaves-the-file-identical"),
		TEXT("dry-run-reports"),
		TEXT("dry-run-writes-nothing"),
		TEXT("pair-key-added-for-its-owner"),
		TEXT("reference-row-not-mirrored-as-owned"),
		TEXT("pair-key-takes-its-own-half-of-each-cell"),
		TEXT("bool-key-added"),
		TEXT("bool-value-bare-and-unit-bool"),
		TEXT("placeholder-ranges-write-no-field"),
		TEXT("placeholder-safe-range-writes-no-field"),
		TEXT("crlf-registry-mirrored"),
		TEXT("crlf-old-text-byte-identical"),
		TEXT("crlf-appended-lines-end-in-crlf"),
		TEXT("unsupported-layout-exits-2"),
		TEXT("unsupported-layout-reported"),
		TEXT("unsupported-layout-writes-nothing"),
		TEXT("real-tree-mirror-complete"),
	};

	/** Seeded observations the selftest must echo. */
	const TCHAR* const SeededObservations[] = {
		TEXT("ADDED BetaLimitCount design/gdd/beta-system.md:10"),
		TEXT("MIRROR result: added 1, existing 7 untouched"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildRegistryMirrorTest, "StealthGame.Build.RegistryMirror",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildRegistryMirrorTest::RunTest(const FString& Parameters)
{
	using namespace BuildRegistryMirrorTestData;
	using namespace StealthBuildTest;

	return RunBashSelftest(*this, SelftestScript, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
