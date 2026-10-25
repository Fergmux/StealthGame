#pragma once

// Story tdc-018 (production/epics/tuning-data-config/story-018-replication-system-assertion.md):
// the result record every T2/T3 harness process writes (ADR-0008 section 5). Test-only module code;
// the harness module is excluded from Shipping and Test builds.

#include "CoreMinimal.h"

namespace StealthHarness
{
	/** The replication system a game net driver uses. Unknown means there was no driver to ask: it is never read as legacy. */
	enum class EHarnessReplicationSystem : uint8
	{
		Legacy,
		Iris,
		Unknown,
	};

	/** The JSON/log spelling: "legacy", "iris" or "unknown". */
	inline const TCHAR* ToString(EHarnessReplicationSystem System)
	{
		switch (System)
		{
		case EHarnessReplicationSystem::Legacy: return TEXT("legacy");
		case EHarnessReplicationSystem::Iris: return TEXT("iris");
		default: return TEXT("unknown");
		}
	}

	/** One fault reported through the harness runner's IStealthFaultSink face. */
	struct FHarnessFault
	{
		/** The EStealthFault category name (for example "StaleEpoch"). */
		FString Category;
		/** The reporting system or object. */
		FName Source;
		/** The category-specific detail value. */
		int64 Detail = 0;
	};

	/**
	 * What one harness process records: {scenario, build, replicationSystem, netProfile, passed,
	 * negativeControlDetected, faults[], captures[]} (ADR-0008 section 5). Written to the path given
	 * by -StealthHarnessOut=<path>; see FStealthHarnessScenarioRunner::ResolveResultPath.
	 */
	struct FHarnessResult
	{
		/** The scenario name (-StealthHarnessScenario=<Name>). */
		FString Scenario;
		/** The build configuration string, for example "Development". */
		FString Build;
		/** The replication system the game net driver reported. */
		EHarnessReplicationSystem ReplicationSystem = EHarnessReplicationSystem::Unknown;
		/** The -PktEmulationProfile name, or "none". */
		FString NetProfile = TEXT("none");
		/** True only when the replication system matched the pinned expectation, the negative control was detected and the run ended normally. */
		bool bPassed = false;
		/** True when the runner's own control (a fake probe reporting the other system) was detected as a failure. */
		bool bNegativeControlDetected = false;
		/** Faults reported during the run. */
		TArray<FHarnessFault> Faults;
		/** Files the run produced (record files, packet captures). */
		TArray<FString> Captures;

		/** The record as pretty-printed JSON with exactly the eight keys above. */
		FString ToJson() const;
	};
}
