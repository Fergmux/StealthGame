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

/** Shared Character & Locomotion ServerOnly keys (GDD Tuning Knobs, audience ServerOnly; story SCL-003): never replicated. */
USTRUCT()
struct STEALTHFOUNDATION_API FLocomotionTuning_Server
{
	GENERATED_BODY()

	/** IsRunning enter threshold (F4, I-SCL-1). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "180", ClampMax = "500", SafeMin = "250", SafeMax = "350", Latch = "Use", Owner = "SCL", Unit = "CmPerSec"))
	float RunSpeedThresholdCmPerSec = 300.0f;

	/** IsRunning exits at threshold minus this (F4). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "15", SafeMax = "40", Latch = "Use", Owner = "SCL", Unit = "CmPerSec"))
	float RunExitHysteresisCmPerSec = 25.0f;

	/** Unchanged look input this long starts the idle head scan (R8.9, D14). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "1", ClampMax = "10", SafeMin = "2", SafeMax = "5", Latch = "Use", Owner = "SCL", Unit = "Sec"))
	float IdleGazeDelaySec = 3.0f;

	/** Control-rotation drift below this rate counts as no look input for the idle-gaze timer (R8.9, D31). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0", ClampMax = "3", SafeMin = "0.2", SafeMax = "1.0", Latch = "Use", Owner = "SCL", Unit = "PerSec"))
	float IdleGazeLookDeadzoneDegPerSec = 0.5f;

	/** NetUpdateFrequency = MinNetUpdateFrequency of every pawn (TR-scl-009, I-SCL-8/9). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "12", ClampMax = "17", SafeMin = "13", SafeMax = "16", Latch = "Round", Owner = "SCL", Unit = "Hz"))
	float PawnNetUpdateHz = 15.0f;

	/** Server time without an owner move before the pawn brakes by the baseline (R15, I-SCL-12/15). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.25", ClampMax = "1.0", SafeMin = "0.25", SafeMax = "0.4", Latch = "Use", Owner = "SCL", Unit = "Sec"))
	float InputStarvationSec = 0.25f;

	/** Cap on observed speed over the gait max while a burst of late moves is consumed (F12, I-SCL-7). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "1.0", ClampMax = "1.5", SafeMin = "1.05", SafeMax = "1.25", Latch = "Use", Owner = "SCL", Unit = "Mult"))
	float StarvationCatchUpMaxMult = 1.15f;

	/** After an AI takeover the AI keeps the heading at walk this long (R4). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.3", ClampMax = "2.0", SafeMin = "0.5", SafeMax = "1.2", Latch = "Use", Owner = "SCL", Unit = "Sec"))
	float HandoverHoldSec = 0.8f;
};
