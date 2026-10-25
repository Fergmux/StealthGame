// Story tdc-009 (production/epics/tuning-data-config/story-009-log-capture-hook.md):
// FStealthLogCapture records LogStealthFoundation warnings and asserts "exactly one warning
// naming key K with authored A and effective E" (ADR-0008 section 4).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthFoundationModule.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "Server/Testing/StealthLogCapture.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthLogCaptureTestOther, Log, All);

namespace LogCaptureTestData
{
	/** Every synthetic warning carries this marker so the automation framework expects it. */
	const TCHAR* const Marker = TEXT("[LogCaptureSelfTest]");
	const TCHAR* const Key = TEXT("NpcCount");
	const TCHAR* const LongerKey = TEXT("NpcCountMax");
	const TCHAR* const OtherKey = TEXT("RoundSeconds");
	const TCHAR* const Authored = TEXT("3");
	const TCHAR* const Effective = TEXT("2");

	/** Emits one synthetic validation-style warning on LogStealthFoundation. */
	void EmitFoundationWarning(const TCHAR* InKey, const TCHAR* InAuthored, const TCHAR* InEffective)
	{
		UE_LOGF(LogStealthFoundation, Warning, "%ls tuning key %ls: authored %ls, effective %ls.", Marker, InKey, InAuthored, InEffective);
	}

	/** The same warning on a different category, which the capture ignores. */
	void EmitOtherCategoryWarning(const TCHAR* InKey, const TCHAR* InAuthored, const TCHAR* InEffective)
	{
		UE_LOGF(LogStealthLogCaptureTestOther, Warning, "%ls tuning key %ls: authored %ls, effective %ls.", Marker, InKey, InAuthored, InEffective);
	}
}

// FStealthLogCaptureTestBase: the synthetic warnings are intended and must reach the capture at
// Warning verbosity, so they are not declared with AddExpectedMessage (which demotes them).
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FHarnessLogCaptureAssertsWarningByKeyTest, FStealthLogCaptureTestBase,
	"StealthGame.Harness.LogCaptureAssertsWarningByKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHarnessLogCaptureAssertsWarningByKeyTest::RunTest(const FString& Parameters)
{
	using namespace LogCaptureTestData;
	FStealthRecordingFaultSink Faults;

	// One matching warning: passes.
	{
		FStealthLogCapture Capture;
		EmitFoundationWarning(Key, Authored, Effective);
		const FStealthLogMatchResult Result = Capture.ExpectExactlyOneWarning(Key, Authored, Effective);
		TestTrue(TEXT("one matching warning: passes"), Result.Passed());
	}

	// Zero warnings: fails.
	{
		FStealthLogCapture Capture;
		const FStealthLogMatchResult Result = Capture.ExpectExactlyOneWarning(Key, Authored, Effective);
		TestFalse(TEXT("zero warnings: fails"), Result.Passed());
		TestEqual(TEXT("zero warnings: no match"), Result.Matches.Num(), 0);
	}

	// Two matching warnings: fails.
	{
		FStealthLogCapture Capture;
		EmitFoundationWarning(Key, Authored, Effective);
		EmitFoundationWarning(Key, Authored, Effective);
		const FStealthLogMatchResult Result = Capture.ExpectExactlyOneWarning(Key, Authored, Effective);
		TestFalse(TEXT("two warnings: fails"), Result.Passed());
		TestEqual(TEXT("two warnings: two matches"), Result.Matches.Num(), 2);
	}

	// A warning naming a different key: fails.
	{
		FStealthLogCapture Capture;
		EmitFoundationWarning(OtherKey, Authored, Effective);
		TestFalse(TEXT("different key: fails"), Capture.ExpectExactlyOneWarning(Key, Authored, Effective).Passed());
	}

	// Substring collision: NpcCountMax does not satisfy a query for NpcCount, and the reverse.
	{
		FStealthLogCapture Capture;
		EmitFoundationWarning(LongerKey, Authored, Effective);
		TestFalse(TEXT("NpcCountMax does not match NpcCount"), Capture.ExpectExactlyOneWarning(Key, Authored, Effective).Passed());
		TestTrue(TEXT("NpcCountMax matches itself"), Capture.ExpectExactlyOneWarning(LongerKey, Authored, Effective).Passed());
	}

	// Wrong authored or effective value: no match (values are whole tokens too).
	{
		FStealthLogCapture Capture;
		EmitFoundationWarning(Key, TEXT("13"), TEXT("2.5"));
		TestEqual(TEXT("authored 13 is not authored 3"), Capture.CountWarnings(Key, Authored, TEXT("2.5")), 0);
		TestEqual(TEXT("effective 2.5 is not effective 2"), Capture.CountWarnings(Key, TEXT("13"), Effective), 0);
		TestEqual(TEXT("exact values match"), Capture.CountWarnings(Key, TEXT("13"), TEXT("2.5")), 1);
	}

	// A non-LogStealthFoundation warning is ignored.
	{
		FStealthLogCapture Capture;
		EmitOtherCategoryWarning(Key, Authored, Effective);
		TestEqual(TEXT("other-category warning is not recorded"), Capture.GetWarnings().Num(), 0);
		TestFalse(TEXT("other-category warning does not satisfy the query"), Capture.ExpectExactlyOneWarning(Key, Authored, Effective).Passed());
	}

	// A LogStealthFoundation line below Warning is ignored.
	{
		FStealthLogCapture Capture;
		UE_LOGF(LogStealthFoundation, Log, "%ls tuning key %ls: authored %ls, effective %ls.", Marker, Key, Authored, Effective);
		TestEqual(TEXT("Log-verbosity line is not recorded"), Capture.GetWarnings().Num(), 0);
	}

	// Capture stops at scope exit.
	{
		FStealthLogCapture Capture;
		Capture.Stop();
		EmitFoundationWarning(Key, Authored, Effective);
		TestEqual(TEXT("after Stop nothing is recorded"), Capture.GetWarnings().Num(), 0);
	}
	{
		TOptional<FStealthLogCapture> Scoped;
		Scoped.Emplace();
		EmitFoundationWarning(Key, Authored, Effective);
		const TArray<FString> Captured = Scoped->GetWarnings();
		Scoped.Reset(); // scope exit: the device leaves GLog
		EmitFoundationWarning(Key, Authored, Effective);
		FStealthLogCapture Later;
		TestEqual(TEXT("scoped capture saw the in-scope warning"), Captured.Num(), 1);
		TestEqual(TEXT("a capture opened later does not see earlier warnings"), Later.GetWarnings().Num(), 0);
	}

	// Nested scopes both record; the inner one only while it is open.
	{
		FStealthLogCapture Outer;
		EmitFoundationWarning(OtherKey, Authored, Effective);
		{
			FStealthLogCapture Inner;
			EmitFoundationWarning(Key, Authored, Effective);
			TestTrue(TEXT("nested: inner sees its warning"), Inner.ExpectExactlyOneWarning(Key, Authored, Effective).Passed());
			TestEqual(TEXT("nested: inner did not see the earlier warning"), Inner.GetWarnings().Num(), 1);
		}
		EmitFoundationWarning(Key, Authored, Effective);
		TestEqual(TEXT("nested: outer saw all three"), Outer.GetWarnings().Num(), 3);
		TestEqual(TEXT("nested: outer saw two NpcCount warnings"), Outer.CountWarnings(Key, Authored, Effective), 2);
	}

	// Token rules on their own (no logging).
	TestTrue(TEXT("token at end of sentence"), FStealthLogCapture::ContainsToken(TEXT("effective 3."), TEXT("3")));
	TestFalse(TEXT("3 is not part of 3.5"), FStealthLogCapture::ContainsToken(TEXT("effective 3.5"), TEXT("3")));
	TestFalse(TEXT("5 is not part of 3.5"), FStealthLogCapture::ContainsToken(TEXT("effective 3.5"), TEXT("5")));
	TestFalse(TEXT("3 is not part of -3"), FStealthLogCapture::ContainsToken(TEXT("authored -3"), TEXT("3")));
	TestTrue(TEXT("-3 matches -3"), FStealthLogCapture::ContainsToken(TEXT("authored -3,"), TEXT("-3")));
	TestFalse(TEXT("empty token never matches"), FStealthLogCapture::ContainsToken(TEXT("anything"), TEXT("")));
	TestTrue(TEXT("second occurrence found after a collision"), FStealthLogCapture::ContainsToken(TEXT("NpcCountMax NpcCount"), TEXT("NpcCount")));

	const FStealthFaultTeardownResult Teardown = Faults.EvaluateTeardown({});
	TestTrue(Teardown.Describe(), Teardown.IsClean());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
