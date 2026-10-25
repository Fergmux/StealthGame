// Story tdc-008 (production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md):
// the recording fault sink and its teardown assertion (ADR-0008 section 4).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthFaultSinkTestSupport.h"

namespace FaultSinkTestData
{
	const FName RosterSource(TEXT("RosterService"));
	const FName TuningSource(TEXT("TuningStore"));
	constexpr int64 StaleEpochDetail = 7;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarnessFaultSinkTeardownAssertionTest, "StealthGame.Harness.FaultSinkTeardownAssertion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHarnessFaultSinkTeardownAssertionTest::RunTest(const FString& Parameters)
{
	using namespace FaultSinkTestData;

	// The define is 1 in this non-Shipping build (the Shipping value is proven by story tdc-006).
	TestEqual(TEXT("WITH_STEALTH_TEST_SEAMS is 1 outside Shipping"), WITH_STEALTH_TEST_SEAMS, 1);

	// (a) No faults reported: teardown is clean.
	{
		FStealthRecordingFaultSink Sink;
		const FStealthFaultTeardownResult Result = Sink.EvaluateTeardown({});
		TestTrue(TEXT("(a) no faults: teardown is clean"), Result.IsClean());
	}

	// (b) An undeclared fault: listed as unexpected, with its source and detail.
	{
		FStealthRecordingFaultSink Sink;
		IStealthFaultSink& AsInterface = Sink;
		AsInterface.Report(EStealthFault::StaleEpoch, RosterSource, StaleEpochDetail);
		const FStealthFaultTeardownResult Result = Sink.EvaluateTeardown({});
		TestFalse(TEXT("(b) undeclared fault: teardown is not clean"), Result.IsClean());
		if (TestEqual(TEXT("(b) one unexpected fault"), Result.Unexpected.Num(), 1))
		{
			TestTrue(TEXT("(b) unexpected fault keeps category"), Result.Unexpected[0].Fault == EStealthFault::StaleEpoch);
			TestEqual(TEXT("(b) unexpected fault keeps source"), Result.Unexpected[0].Source, RosterSource);
			TestEqual(TEXT("(b) unexpected fault keeps detail"), Result.Unexpected[0].Detail, StaleEpochDetail);
		}
	}

	// (c) A declared expected fault that is raised: clean.
	{
		FStealthRecordingFaultSink Sink;
		Sink.Report(EStealthFault::RejectedTuning, TuningSource, 0);
		const FStealthFaultTeardownResult Result = Sink.EvaluateTeardown({ { EStealthFault::RejectedTuning, TuningSource } });
		TestTrue(TEXT("(c) declared fault raised: teardown is clean"), Result.IsClean());
	}

	// Edge: declared but never raised -> reported as missing.
	{
		FStealthRecordingFaultSink Sink;
		const FStealthFaultTeardownResult Result = Sink.EvaluateTeardown({ { EStealthFault::DeadRegistration } });
		TestFalse(TEXT("declared-but-never-raised: not clean"), Result.IsClean());
		TestEqual(TEXT("declared-but-never-raised: one missing"), Result.Missing.Num(), 1);
		TestEqual(TEXT("declared-but-never-raised: no unexpected"), Result.Unexpected.Num(), 0);
	}

	// Edge: the same fault twice. Declared: both covered. Undeclared: both listed.
	{
		FStealthRecordingFaultSink Sink;
		Sink.Report(EStealthFault::InvariantViolation, RosterSource, 1);
		Sink.Report(EStealthFault::InvariantViolation, RosterSource, 1);
		TestTrue(TEXT("same fault twice, declared once (any source): clean"),
			Sink.EvaluateTeardown({ { EStealthFault::InvariantViolation } }).IsClean());
		TestEqual(TEXT("same fault twice, undeclared: two unexpected"), Sink.EvaluateTeardown({}).Unexpected.Num(), 2);
	}

	// Edge: a declaration scoped to one source does not cover another source.
	{
		FStealthRecordingFaultSink Sink;
		Sink.Report(EStealthFault::RejectedTuning, RosterSource, 0);
		const FStealthFaultTeardownResult Result = Sink.EvaluateTeardown({ { EStealthFault::RejectedTuning, TuningSource } });
		TestEqual(TEXT("source-scoped declaration: other source is unexpected"), Result.Unexpected.Num(), 1);
		TestEqual(TEXT("source-scoped declaration: declared source is missing"), Result.Missing.Num(), 1);
	}

	// Reset clears the record.
	{
		FStealthRecordingFaultSink Sink;
		Sink.Report(EStealthFault::StaleEpoch, RosterSource, 1);
		Sink.Reset();
		TestEqual(TEXT("Reset clears every record"), Sink.GetRecords().Num(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
