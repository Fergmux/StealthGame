#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/StealthFaultSink.h"

/**
 * Test support for IStealthFaultSink (ADR-0008 section 4, story tdc-008). Compiled only under
 * WITH_STEALTH_TEST_SEAMS, so nothing here reaches a Shipping build.
 *
 * Usage in a test:
 *   FStealthRecordingFaultSink Sink;              // inject into the system under test
 *   ... act ...
 *   const FStealthFaultTeardownResult R = Sink.EvaluateTeardown({ { EStealthFault::StaleEpoch } });
 *   TestTrue(R.Describe(), R.IsClean());
 */

/** One reported fault. */
struct FStealthFaultRecord
{
	EStealthFault Fault = EStealthFault::InvariantViolation;
	FName Source;
	int64 Detail = 0;
};

/**
 * A fault a test declares as expected. It covers every report of that category (from Source,
 * or from any source when Source is NAME_None), however many times it is raised; a declared
 * fault that is never raised is reported as missing.
 */
struct FStealthExpectedFault
{
	EStealthFault Fault = EStealthFault::InvariantViolation;
	FName Source = NAME_None;

	/** True when Record is covered by this declaration. */
	bool Covers(const FStealthFaultRecord& Record) const
	{
		return Record.Fault == Fault && (Source.IsNone() || Record.Source == Source);
	}
};

/** Outcome of a teardown check, returned instead of failing the test so self-tests can inspect it. */
struct FStealthFaultTeardownResult
{
	/** Reports no expected declaration covers, in report order. */
	TArray<FStealthFaultRecord> Unexpected;
	/** Expected declarations that no report matched. */
	TArray<FStealthExpectedFault> Missing;

	/** True when there is no unexpected fault and every declared fault was raised. */
	bool IsClean() const { return Unexpected.IsEmpty() && Missing.IsEmpty(); }

	/** One-line description for a test message. */
	FString Describe() const
	{
		return FString::Printf(TEXT("fault teardown: %d unexpected, %d declared but never raised"), Unexpected.Num(), Missing.Num());
	}
};

/** IStealthFaultSink that records every report for later assertion. */
class FStealthRecordingFaultSink : public IStealthFaultSink
{
public:
	virtual void Report(EStealthFault Fault, FName Source, int64 Detail) override
	{
		Records.Add({ Fault, Source, Detail });
	}

	/** Every report so far, in order. */
	const TArray<FStealthFaultRecord>& GetRecords() const { return Records; }

	/** Reports not covered by any of Expected (ADR-0008 Key Interfaces: GetUnexpectedFaults). */
	TArray<FStealthFaultRecord> GetUnexpectedFaults(TConstArrayView<FStealthExpectedFault> Expected) const
	{
		return Records.FilterByPredicate([Expected](const FStealthFaultRecord& Record)
		{
			return !Expected.ContainsByPredicate([&Record](const FStealthExpectedFault& E) { return E.Covers(Record); });
		});
	}

	/** Teardown check: unexpected reports and declared-but-never-raised faults. */
	FStealthFaultTeardownResult EvaluateTeardown(TConstArrayView<FStealthExpectedFault> Expected) const
	{
		FStealthFaultTeardownResult Result;
		Result.Unexpected = GetUnexpectedFaults(Expected);
		for (const FStealthExpectedFault& E : Expected)
		{
			if (!Records.ContainsByPredicate([&E](const FStealthFaultRecord& Record) { return E.Covers(Record); }))
			{
				Result.Missing.Add(E);
			}
		}
		return Result;
	}

	/** Forgets every report. */
	void Reset() { Records.Reset(); }

private:
	TArray<FStealthFaultRecord> Records;
};

#endif // WITH_STEALTH_TEST_SEAMS
