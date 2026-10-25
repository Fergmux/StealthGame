// Story tdc-028 (production/epics/tuning-data-config/story-028-commit-message-check.md):
// G2 wrapper for the commit-msg hook (ADR-0009 section 5). Runs tools/ci/hooks/selftest/run-commit-msg-selftest.sh:
// the six Conventional Commits prefixes (scoped accepted), the three reference forms (Story/Task/Decision line,
// ADR-NNNN, design/gdd/ or docs/architecture/ path), ADR-12 not a reference, trailers neither rejected nor counted,
// empty and merge/revert subjects, one real `git commit` rejection and acceptance, and the project's own history.
// EditorContext only because the test runs project scripts and git. Nothing is installed in the real working copy.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthBuildFileTestUtils.h"

namespace CommitMsgCheckTestData
{
	const TCHAR* const SelftestScript = TEXT("tools/ci/hooks/selftest/run-commit-msg-selftest.sh");

	/** Selftest cases that must each report "ok". */
	const TCHAR* const Cases[] = {
		TEXT("prefix-feat-with-story-accepted"), TEXT("prefix-fix-with-story-accepted"),
		TEXT("prefix-chore-with-story-accepted"), TEXT("prefix-docs-with-story-accepted"),
		TEXT("prefix-test-with-story-accepted"), TEXT("prefix-refactor-with-story-accepted"),
		TEXT("wrong-prefix-rejected"), TEXT("no-prefix-rejected"), TEXT("prefix-needs-colon-space-rejected"),
		TEXT("prefix-needs-description-rejected"), TEXT("uppercase-prefix-rejected"),
		TEXT("scoped-prefix-accepted"), TEXT("breaking-prefix-accepted"), TEXT("story-line-accepted"),
		TEXT("task-line-accepted"), TEXT("decision-line-accepted"), TEXT("adr-token-accepted"),
		TEXT("design-path-accepted"), TEXT("architecture-path-accepted"), TEXT("no-reference-rejected"),
		TEXT("subject-only-rejected"), TEXT("adr-two-digits-not-a-reference"),
		TEXT("adr-five-digits-not-a-reference"), TEXT("adr-in-subject-only-not-counted"),
		TEXT("story-without-id-not-a-reference"), TEXT("unrelated-path-not-a-reference"),
		TEXT("path-after-punctuation-accepted"), TEXT("path-inside-other-word-not-a-reference"),
		TEXT("trailer-after-story-accepted"), TEXT("trailer-alone-not-a-reference"),
		TEXT("trailer-naming-an-adr-not-counted"), TEXT("empty-message-rejected"), TEXT("blank-message-rejected"),
		TEXT("comments-only-message-rejected"), TEXT("merge-subject-exempt"), TEXT("revert-subject-exempt"),
		TEXT("merge-word-inside-other-subject-not-exempt"), TEXT("comment-lines-ignored"),
		TEXT("crlf-message-accepted"), TEXT("no-argument-exit-2"), TEXT("git-rejects-bad-message"),
		TEXT("git-accepts-good-message"), TEXT("real-history-style-accepted"),
	};

	/** Observations the selftest echoes from the script under test. */
	const TCHAR* const Observations[] = {
		TEXT("commit-msg: REJECTED - subject must start with"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildCommitMsgCheckTest, "StealthGame.Build.CommitMsgCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildCommitMsgCheckTest::RunTest(const FString& Parameters)
{
	using namespace CommitMsgCheckTestData;
	using namespace StealthBuildTest;

	return RunBashSelftest(*this, SelftestScript, MakeArrayView(Cases), MakeArrayView(Observations));
}

#endif // WITH_DEV_AUTOMATION_TESTS
