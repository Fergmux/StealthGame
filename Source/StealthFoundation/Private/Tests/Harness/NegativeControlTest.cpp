// Story tdc-010 (production/epics/tuning-data-config/story-010-negative-control-helper.md):
// FStealthNegativeControl passes its enclosing test only when the inner check fails (ADR-0008
// section 3). Both directions are observed through the returned result; the reporting step is
// exercised for real only in the detected direction, so this self-test never fails itself.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "Server/Testing/StealthNegativeControl.h"

namespace NegativeControlTestData
{
	/** A stand-in gate: "the value must be within [0, Max]". */
	constexpr int32 GateMax = 10;
	constexpr int32 SeededViolation = 11;
	constexpr int32 CleanValue = 4;

	void GateInnerCheck(FStealthCheckContext& Ctx, int32 Value)
	{
		Ctx.TestTrue(FString::Printf(TEXT("value %d within [0, %d]"), Value, GateMax), Value >= 0 && Value <= GateMax);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarnessNegativeControlDetectedTest, "StealthGame.Harness.NegativeControlDetected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHarnessNegativeControlDetectedTest::RunTest(const FString& Parameters)
{
	using namespace NegativeControlTestData;
	FStealthRecordingFaultSink Faults;

	// (a) The inner check fails as expected: detected, and the enclosing test passes.
	const FStealthNegativeControl Control(TEXT("ValueAboveMax"));
	const FStealthNegativeControlResult Detected = Control.Run([](FStealthCheckContext& Ctx) { GateInnerCheck(Ctx, SeededViolation); });
	TestTrue(TEXT("(a) failing inner check: control detected"), Detected.bDetected);
	TestEqual(TEXT("(a) the inner failure is kept"), Detected.InnerFailures.Num(), 1);
	TestTrue(TEXT("(a) message names the control"), Detected.Describe().Contains(TEXT("control detected: ValueAboveMax")));
	TestTrue(TEXT("(a) Report on a detected control adds no error and returns true"), FStealthNegativeControl::Report(*this, Detected));
	TestFalse(TEXT("(a) the inner failure did not fail this test"), HasAnyErrors());

	// (b) The inner check passes: not detected; observed through the result (Report would AddError).
	const FStealthNegativeControlResult Missed = Control.Run([](FStealthCheckContext& Ctx) { GateInnerCheck(Ctx, CleanValue); });
	TestFalse(TEXT("(b) passing inner check: control not detected"), Missed.bDetected);
	TestEqual(TEXT("(b) message is 'control not detected' with the name"), Missed.Describe(), FString(TEXT("control not detected: ValueAboveMax")));
	TestTrue(TEXT("(b) no inner failures recorded"), Missed.InnerFailures.IsEmpty());

	// Edge: an assertion-style failure (AddFailure) counts as the inner check failing.
	const FStealthNegativeControlResult Asserted = FStealthNegativeControl(TEXT("AssertStyle")).Run([](FStealthCheckContext& Ctx)
	{
		Ctx.AddFailure(TEXT("ensure-style assertion in the inner check"));
	});
	TestTrue(TEXT("assertion-style inner failure: detected"), Asserted.bDetected);

	// Edge: an inner check that records nothing is "passed" (not detected).
	const FStealthNegativeControlResult Empty = FStealthNegativeControl(TEXT("NoChecks")).Run([](FStealthCheckContext&) {});
	TestFalse(TEXT("inner check with no assertions: not detected"), Empty.bDetected);

	// Edge: the same helper used twice in one test; runs are independent (no state carried over).
	const FStealthNegativeControlResult First = Control.Run([](FStealthCheckContext& Ctx) { GateInnerCheck(Ctx, SeededViolation); });
	const FStealthNegativeControlResult Second = Control.Run([](FStealthCheckContext& Ctx) { GateInnerCheck(Ctx, CleanValue); });
	TestTrue(TEXT("reuse: first run detected"), First.bDetected);
	TestFalse(TEXT("reuse: second run not detected (no failure carried from the first)"), Second.bDetected);
	TestTrue(TEXT("reuse: RunAndReport on a detected control returns true"),
		Control.RunAndReport(*this, [](FStealthCheckContext& Ctx) { GateInnerCheck(Ctx, SeededViolation); }));
	TestFalse(TEXT("reuse: still no errors on this test"), HasAnyErrors());

	const FStealthFaultTeardownResult Teardown = Faults.EvaluateTeardown({});
	TestTrue(Teardown.Describe(), Teardown.IsClean());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
