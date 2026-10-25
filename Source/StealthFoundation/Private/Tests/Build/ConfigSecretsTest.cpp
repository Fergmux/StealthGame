// Decision D52 (production/design-closeout/2026-10-25_rereview-followups.md; GitGuardian finding follow-up):
// G2 wrapper for the config secret check. Runs tools/ci/lint/selftest/run-config-secrets-selftest.sh: a clean
// Config/ passes; a SecurityToken= line in Config/DefaultEngine.ini, in a platform ini, written in lower case with
// spaces, commented out or as an array entry is reported with its file and line and fails the gate; the token value
// is never printed; a mention without an assignment passes; ini files outside Config/ are not scanned; a root with no
// Config/ or no .ini is NOT ASSESSED; the real repository is clean. It then runs the check through the gate on the
// real repository. EditorContext only because the test runs project scripts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace ConfigSecretsTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/lint/selftest/run-config-secrets-selftest.sh");
	const TCHAR* const RuleScript = TEXT("tools/ci/lint/config-secrets.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("clean-config-passes"), TEXT("token-in-default-engine-reported"), TEXT("token-in-platform-ini-reported"),
		TEXT("token-spaced-lowercase-reported"), TEXT("commented-and-array-token-reported"), TEXT("token-value-not-echoed"),
		TEXT("mention-without-assignment-passes"), TEXT("outside-config-ini-ignored"), TEXT("no-config-not-assessed"),
		TEXT("empty-config-not-assessed"), TEXT("real-repo-clean"),
	};

	/** Seeded observations the selftest echoes from the gate. */
	const TCHAR* const SeededObservations[] = {
		TEXT("Config/DefaultEngine.ini:7 secret SecurityToken= line"),
		TEXT("Config/Windows/WindowsEngine.ini:3 secret"),
		TEXT("GATE: FAIL"),
		TEXT("GATE: NOT ASSESSED"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildConfigSecretsTest, "StealthGame.Build.ConfigSecrets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildConfigSecretsTest::RunTest(const FString& Parameters)
{
	using namespace ConfigSecretsTestData;
	using namespace StealthBuildTest;

	return RunLintSelftestSuite(*this, SelftestScript, { RuleScript }, MakeArrayView(Cases), MakeArrayView(SeededObservations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
