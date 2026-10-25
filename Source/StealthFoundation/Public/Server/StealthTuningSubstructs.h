#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "View/StealthTuningPublicSubstructs.h"

#include "StealthTuningSubstructs.generated.h"

// The _Server tuning sub-structs (ADR-0004 sections 1 and 2; story tdc-040): ServerOnly keys, on
// the server face only, never replicated, in TuningHash but not ClientHash. Same declaration rule as
// View/StealthTuningPublicSubstructs.h (one UPROPERTY per key, initialiser = default, ClampMin/Max,
// SafeMin/Max, Latch/Owner/Unit meta); regenerate the schema table after any change.

/** Round & Match Framework ServerOnly keys. */
USTRUCT()
struct STEALTHFOUNDATION_API FRoundTuning_Server
{
	GENERATED_BODY()

	/** Non-binding design target for the round length in minutes; no timer exists, logged only. */
	UPROPERTY(EditAnywhere, Category = "Round", meta = (ClampMin = "5", ClampMax = "30", SafeMin = "8", SafeMax = "15", Latch = "Round", Owner = "Round", Unit = "Minutes"))
	float TargetRoundMinutes = 12.0f;
};

/** Crowd & NPC Behaviour ServerOnly keys (the owner has no public key yet, so no _Public struct). */
USTRUCT()
struct STEALTHFOUNDATION_API FCrowdTuning_Server
{
	GENERATED_BODY()

	/** NPC characters in the crowd (default 50, user decision 2026-10-25). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "6", ClampMax = "100", SafeMin = "45", SafeMax = "50", Latch = "Round", Owner = "Crowd", Unit = "Count"))
	int32 NpcCount = 50;
};
