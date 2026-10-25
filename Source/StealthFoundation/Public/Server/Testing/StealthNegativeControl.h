#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Misc/AutomationTest.h"
#include "Templates/Function.h"

/**
 * Negative-control helper (ADR-0008 section 3, story tdc-010): "a gate you have not watched fail is
 * not a gate". Compiled only under WITH_STEALTH_TEST_SEAMS, so no control reaches Shipping.
 *
 * A negative control runs a gate's inner check against a seeded violation; the inner check is
 * expected to FAIL. When it fails the control is detected and the enclosing test passes; when it
 * passes the gate did not see the violation and the enclosing test fails with "control not
 * detected".
 *
 * The inner check reports into an FStealthCheckContext, never into the real test, so its expected
 * failures do not fail the enclosing test. Inner checks must not call check()/checkf(): those abort
 * the process and cannot be captured (an assertion-style failure is FStealthCheckContext::AddFailure).
 *
 * Usage in a test:
 *   FStealthNegativeControl Control(TEXT("UnlistedReplicatedProperty"));
 *   const FStealthNegativeControlResult R = Control.Run([&](FStealthCheckContext& Ctx)
 *   {
 *       Ctx.TestTrue(TEXT("allowlist audit"), RunAudit(SeededClass));
 *   });
 *   FStealthNegativeControl::Report(*this, R);   // adds "control not detected" when R is not detected
 */

/** Collects an inner check's failures instead of the enclosing automation test. */
class FStealthCheckContext
{
public:
	/** Records a failure with its description. */
	void AddFailure(const FString& What) { Failures.Add(What); }

	/** Records a failure unless bValue is true; returns bValue. */
	bool TestTrue(const FString& What, bool bValue)
	{
		if (!bValue)
		{
			AddFailure(What);
		}
		return bValue;
	}

	/** Records a failure unless bValue is false; returns !bValue. */
	bool TestFalse(const FString& What, bool bValue) { return TestTrue(What, !bValue); }

	/** Records a failure unless Actual == Expected; returns the comparison. */
	template <typename TValue>
	bool TestEqual(const FString& What, const TValue& Actual, const TValue& Expected)
	{
		return TestTrue(What, Actual == Expected);
	}

	/** True when any failure was recorded. */
	bool HasFailed() const { return !Failures.IsEmpty(); }

	/** The recorded failures, in order. */
	const TArray<FString>& GetFailures() const { return Failures; }

private:
	TArray<FString> Failures;
};

/** Outcome of one negative-control run, a value a self-test can inspect without failing. */
struct FStealthNegativeControlResult
{
	/** The control's name, for messages. */
	FString ControlName;
	/** True when the inner check failed as expected (the gate saw the seeded violation). */
	bool bDetected = false;
	/** The inner check's failures (empty when the control was not detected). */
	TArray<FString> InnerFailures;

	/** "control detected: <name>" or "control not detected: <name>". */
	FString Describe() const
	{
		return FString::Printf(TEXT("%s: %s"), bDetected ? TEXT("control detected") : TEXT("control not detected"), *ControlName);
	}
};

/** Runs inner checks that must fail and reports the outcome to an enclosing test. */
class FStealthNegativeControl
{
public:
	/** A named control; the name appears in every message. */
	explicit FStealthNegativeControl(FString InName) : Name(MoveTemp(InName)) {}

	/** Runs InnerCheck once with a fresh context. Reusable: each call is independent. */
	FStealthNegativeControlResult Run(TFunctionRef<void(FStealthCheckContext&)> InnerCheck) const
	{
		FStealthCheckContext Context;
		InnerCheck(Context);
		FStealthNegativeControlResult Result;
		Result.ControlName = Name;
		Result.bDetected = Context.HasFailed();
		Result.InnerFailures = Context.GetFailures();
		return Result;
	}

	/**
	 * The explicit reporting step: adds the error "control not detected: <name>" to Test when the
	 * control was not detected, else an info line. Returns Result.bDetected.
	 */
	static bool Report(FAutomationTestBase& Test, const FStealthNegativeControlResult& Result)
	{
		if (Result.bDetected)
		{
			Test.AddInfo(Result.Describe());
		}
		else
		{
			Test.AddError(Result.Describe());
		}
		return Result.bDetected;
	}

	/** Run then Report in one call. */
	bool RunAndReport(FAutomationTestBase& Test, TFunctionRef<void(FStealthCheckContext&)> InnerCheck) const
	{
		return Report(Test, Run(InnerCheck));
	}

	/** The control's name. */
	const FString& GetName() const { return Name; }

private:
	FString Name;
};

#endif // WITH_STEALTH_TEST_SEAMS
