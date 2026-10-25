#pragma once

// Story tdc-018 (production/epics/tuning-data-config/story-018-replication-system-assertion.md):
// the harness's replication-system assertion (ADR-0008 section 5, ADR-0001). Every T2/T3 run reads which
// replication system the game NetDriver uses, compares it with the pinned expectation (legacy), fails the
// run on a mismatch and records the active system in the result JSON, so no leak or parity result is
// trusted under the wrong driver. Test-only module code, never compiled into Shipping or Test.

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "StealthHarnessResult.h"
#include "Server/StealthFaultSink.h"
#include "Templates/Function.h"

class UNetDriver;

namespace StealthHarness
{
	/**
	 * The replication system every run must be on until ADR-0024 moves the game to Iris (ADR-0001).
	 * A single named constant, so an Iris switch later is one edit; the Iris rerun gate (AC-IVP-29) then
	 * activates by itself.
	 */
	inline constexpr EHarnessReplicationSystem PinnedReplicationSystem = EHarnessReplicationSystem::Legacy;
}

/** The seam through which the runner learns the active replication system; tests inject fakes so no live world is needed. */
class IStealthReplicationSystemProbe
{
public:
	virtual ~IStealthReplicationSystemProbe() = default;

	/** The replication system in use, or Unknown when it cannot be read. */
	virtual StealthHarness::EHarnessReplicationSystem Read() const = 0;
};

/** Production probe: UNetDriver::IsUsingIrisReplication() of the driver the getter returns (Unknown when it returns null). */
class FStealthNetDriverReplicationProbe final : public IStealthReplicationSystemProbe
{
public:
	/** @param InGetDriver  returns the game net driver, or null when there is none yet. */
	explicit FStealthNetDriverReplicationProbe(TFunction<const UNetDriver*()> InGetDriver) : GetDriver(MoveTemp(InGetDriver)) {}

	virtual StealthHarness::EHarnessReplicationSystem Read() const override;

private:
	TFunction<const UNetDriver*()> GetDriver;
};

/** A probe that reports a fixed value: the fake of the tests and of the runner's negative control. */
class FStealthFixedReplicationProbe final : public IStealthReplicationSystemProbe
{
public:
	explicit FStealthFixedReplicationProbe(StealthHarness::EHarnessReplicationSystem InValue) : Value(InValue) {}

	virtual StealthHarness::EHarnessReplicationSystem Read() const override { return Value; }

private:
	StealthHarness::EHarnessReplicationSystem Value;
};

/** Outcome of writing the result JSON. */
enum class EStealthHarnessResultWrite : uint8
{
	/** The file was written. */
	Written,
	/** No -StealthHarnessOut was given: nothing is written, the result is still evaluated and logged. */
	NotRequested,
	/** The path could not be written: the run is marked failed (a result that cannot be recorded is a failed run). */
	Failed,
};

/**
 * Evaluates one harness process's result: asserts the replication system, runs its own negative
 * control, collects faults (it is the process's IStealthFaultSink) and captures, and writes the result JSON.
 */
class FStealthHarnessScenarioRunner final : public IStealthFaultSink
{
public:
	/**
	 * @param InProbe     reads the active replication system; must outlive the runner.
	 * @param InScenario  scenario name for the result.
	 * @param InBuild     build configuration string, for example "Development".
	 * @param InNetProfile  -PktEmulationProfile name, or "none".
	 * @param InExpected  the expected replication system (the pinned legacy by default).
	 */
	FStealthHarnessScenarioRunner(const IStealthReplicationSystemProbe& InProbe, FString InScenario, FString InBuild, FString InNetProfile,
		StealthHarness::EHarnessReplicationSystem InExpected = StealthHarness::PinnedReplicationSystem);

	/**
	 * Reads the probe, records the system in the result and compares it with the expectation, then runs
	 * the negative control once. Returns true only when the system matches the expectation and the control
	 * was detected; otherwise the result is marked not passed. A mismatch is a failure, never a warning.
	 */
	bool AssertReplicationSystem();

	/** Marks the run as not passed (an abnormal end such as a ceiling hit); Reason is logged. */
	void MarkFailed(const FString& Reason);

	/** Adds a produced file (record file, packet capture) to captures[]. */
	void AddCapture(const FString& Path) { Result.Captures.AddUnique(Path); }

	/** IStealthFaultSink: records the fault in faults[]; any fault makes the run not passed. */
	virtual void Report(EStealthFault Fault, FName Source, int64 Detail) override;

	/**
	 * Finalises the verdict and writes the result JSON to Path. An empty Path means no file was requested
	 * (NotRequested; the result is still evaluated and logged). An unwritable Path returns Failed and marks the run failed.
	 */
	EStealthHarnessResultWrite WriteResult(const FString& Path);

	/** The result so far. */
	const StealthHarness::FHarnessResult& GetResult() const { return Result; }

	/** True while nothing has failed the run. */
	bool IsPassing() const { return bPassing; }

	/** The reasons the run failed, for logs and test messages. */
	const TArray<FString>& GetFailureReasons() const { return FailureReasons; }

	/**
	 * Where this process writes its result: OutParam is the -StealthHarnessOut value. Empty returns empty
	 * (no file). A value ending in ".json" is the file itself; any other value is a folder and the file is
	 * "<folder>/server-result.json" or "<folder>/client<N>-result.json" (the multi-process launcher passes a
	 * folder shared by all its processes).
	 */
	static FString ResolveResultPath(const FString& OutParam, bool bServer, int32 ClientIndex);

private:
	const IStealthReplicationSystemProbe& Probe;
	StealthHarness::EHarnessReplicationSystem Expected;
	StealthHarness::FHarnessResult Result;
	TArray<FString> FailureReasons;
	bool bPassing = true;
	bool bAsserted = false;
};
