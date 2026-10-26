#pragma once

#include "CoreMinimal.h"

#include "StealthTuningPublicSubstructs.generated.h"

// The _Public tuning sub-structs (ADR-0004 sections 1 and 2; story tdc-040). Audience is
// structural: a key in a _Public struct is on the client face (FStealthClientTuning), in the
// replicated snapshot and in ClientHash. They live under Public/View/ so the client face can name
// them without including a Server/ header (ADR-0007 include rule (c)); the _Server structs are in
// Server/StealthTuningSubstructs.h.
//
// Declaration rule (ADR-0004 section 2): one UPROPERTY per key; the field name is the canonical key;
// the C++ initialiser is the schema default; ClampMin/ClampMax the hard range; SafeMin/SafeMax the
// safe range; Latch/Owner/Unit custom meta (Unit = the CR-15.2 name suffix). The values below are the
// GDD Canonical Tunables rows (design/gdd/tuning-data-config.md). After any change here, regenerate
// Private/Tuning/StealthTuningSchema.generated.inl (Stealth.Tuning.SchemaGen) and bump
// TuningSchemaVersion when the fingerprint changes (StealthTuningVersion.h).
// Never add Config/DefaultConfig/GlobalUserConfig/PerObjectConfig (CR-13) or a Blueprint specifier.

/** Round & Match Framework keys a client needs. */
USTRUCT()
struct STEALTHFOUNDATION_API FRoundTuning_Public
{
	GENERATED_BODY()

	/** Players per team (a real match is 5; 1 to 4 for tests). */
	UPROPERTY(EditAnywhere, Category = "Round", meta = (ClampMin = "1", ClampMax = "5", SafeMin = "2", SafeMax = "5", Latch = "Round", Owner = "Round", Unit = "Size"))
	int32 TeamSize = 5;
};

/** Assassination & Defence Reversal keys a client needs. */
USTRUCT()
struct STEALTHFOUNDATION_API FAssassinationTuning_Public
{
	GENERATED_BODY()

	/** Strike reach in cm (capped at 250 by I-ASN-1). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "100", ClampMax = "250", SafeMin = "150", SafeMax = "250", Latch = "Use", Owner = "Assassination", Unit = "Cm"))
	float AssassinationRangeCm = 200.0f;

	/** Hold-to-prepare time of the defence in seconds. */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "3.0", ClampMax = "5.0", SafeMin = "3.0", SafeMax = "5.0", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float DefenceDurationSec = 4.0f;

	/** Defence cooldown from activation, used or unused, in seconds. */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "30", ClampMax = "120", SafeMin = "45", SafeMax = "75", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float DefenceCooldownSec = 60.0f;
};

/** NPC-Kill Penalty & Informants keys a client needs. */
USTRUCT()
struct STEALTHFOUNDATION_API FPenaltyTuning_Public
{
	GENERATED_BODY()

	/** Removal on the Nth NPC kill (hard minimum 1 only in a bTestAllowsStrikeLimitOne preset, applied by F-1). */
	UPROPERTY(EditAnywhere, Category = "Penalty", meta = (ClampMin = "2", ClampMax = "5", SafeMin = "2", SafeMax = "4", Latch = "Round", Owner = "Penalty", Unit = "Limit"))
	int32 NpcKillStrikeLimit = 3;
};

/** Objectives Chain keys a client needs. */
USTRUCT()
struct STEALTHFOUNDATION_API FObjectivesTuning_Public
{
	GENERATED_BODY()

	/** Hold-to-interact time at an objective in seconds. */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "2.0", ClampMax = "6.0", SafeMin = "2.9", SafeMax = "4.5", Latch = "Action", Owner = "Objectives", Unit = "Sec"))
	float ObjectiveInteractSec = 3.0f;
};

/**
 * Shared Character & Locomotion keys a client needs (GDD shared-character-locomotion.md Tuning Knobs, audience Client;
 * story SCL-003). One value for every pawn: no key has an NPC or player variant (AC-SCL-01). Keys marked provisional in
 * the GDD (traversal, net emulation, spike R3) change only through /propagate-design-change.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FLocomotionTuning_Public
{
	GENERATED_BODY()

	/** Crowd pace and the LSC D4 reference speed (F1); the walk max speed is this times the quantised pace. */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "100", ClampMax = "200", SafeMin = "131", SafeMax = "160", Latch = "Round", Owner = "SCL", Unit = "CmPerSec"))
	float WalkSpeedCmPerSec = 140.0f;

	/** Slowest walk pace multiplier; must be <= 0.8, the minimum of the fixed pace set (I-SCL-5). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.6", ClampMax = "1.0", SafeMin = "0.75", SafeMax = "0.80", Latch = "Round", Owner = "SCL", Unit = "Mult"))
	float WalkSpeedScaleMinMult = 0.80f;

	/** Run tier speed (F1), no scale. */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "300", ClampMax = "650", SafeMin = "400", SafeMax = "500", Latch = "Round", Owner = "SCL", Unit = "CmPerSec"))
	float RunSpeedCmPerSec = 450.0f;

	/** Time 0 to walk speed at constant acceleration (F2). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.2", ClampMax = "1.0", SafeMin = "0.3", SafeMax = "0.5", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float WalkStartSec = 0.40f;

	/** Stop time at the slowest pace; sets the one walk deceleration (F2, I-SCL-4). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.5", ClampMax = "1.2", SafeMin = "0.5", SafeMax = "0.6", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float WalkStopSec = 0.60f;

	/** Stop floor: every stop to rest lasts at least this (F2, decision D2; I-SCL-16). Crowd reads it too. */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.2", ClampMax = "1.0", SafeMin = "0.4", SafeMax = "0.6", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float StopMinSec = 0.5f;

	/** Time 0 to run speed (F2). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.3", ClampMax = "1.5", SafeMin = "0.5", SafeMax = "0.8", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float RunStartSec = 0.60f;

	/** Time run speed to 0; also the run to walk slowdown rate (F2). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.4", ClampMax = "1.5", SafeMin = "0.6", SafeMax = "0.9", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float RunStopSec = 0.75f;

	/** Heading rate cap at walk (F3, I-SCL-6). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "30", ClampMax = "120", SafeMin = "50", SafeMax = "64", Latch = "Round", Owner = "SCL", Unit = "PerSec"))
	float WalkTurnRateDegPerSec = 60.0f;

	/** Heading rate cap while running (F3). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "90", ClampMax = "360", SafeMin = "150", SafeMax = "220", Latch = "Round", Owner = "SCL", Unit = "PerSec"))
	float RunTurnRateDegPerSec = 180.0f;

	/** In-place turn rate (pivot, idle re-centre; R5.4, R5.5). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "60", ClampMax = "180", SafeMin = "90", SafeMax = "150", Latch = "Round", Owner = "SCL", Unit = "PerSec"))
	float PivotTurnRateDegPerSec = 120.0f;

	/** At or below this speed the pawn pivots in place (R5.4). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "5", ClampMax = "50", SafeMin = "15", SafeMax = "30", Latch = "Round", Owner = "SCL", Unit = "CmPerSec"))
	float PivotSpeedCmPerSec = 20.0f;

	/** Input heading change that triggers stop-pivot-start (R5.4). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "90", ClampMax = "170", SafeMin = "110", SafeMax = "135", Latch = "Round", Owner = "SCL", Unit = "Deg"))
	float ReversalAngleDeg = 120.0f;

	/** Head yaw clamp relative to the torso (R8, F6). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "30", ClampMax = "90", SafeMin = "60", SafeMax = "75", Latch = "Round", Owner = "SCL", Unit = "Deg"))
	float LookYawLimitDeg = 70.0f;

	/** Head pitch clamp (R8, F6). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "10", ClampMax = "45", SafeMin = "15", SafeMax = "25", Latch = "Round", Owner = "SCL", Unit = "Deg"))
	float LookPitchLimitDeg = 20.0f;

	/** Head rate clamp (F6). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "60", ClampMax = "300", SafeMin = "120", SafeMax = "160", Latch = "Round", Owner = "SCL", Unit = "PerSec"))
	float LookMaxRateDegPerSec = 140.0f;

	/** Time to reach the head rate limit from rest (F6). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.0", ClampMax = "0.5", SafeMin = "0.1", SafeMax = "0.25", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float LookEaseSec = 0.15f;

	/** Look held beyond the yaw limit this long before the idle body re-centres (R5.5). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.1", ClampMax = "2.0", SafeMin = "0.3", SafeMax = "0.8", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float TurnInPlaceDelaySec = 0.50f;

	/** Vault capability height, equal to the kit vault coping (R7.1; final per spike SCL-045). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "60", ClampMax = "120", SafeMin = "75", SafeMax = "85", Latch = "Round", Owner = "SCL", Unit = "Cm"))
	float TraversalVaultHeightCm = 80.0f;

	/** Climb / mantle capability height (R7.1; final per spike SCL-045). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "120", ClampMax = "200", SafeMin = "150", SafeMax = "170", Latch = "Round", Owner = "SCL", Unit = "Cm"))
	float TraversalClimbHeightCm = 160.0f;

	/** Hurdle capability height (the sample's Hurdle action over a 1.2 m wall; added final by spike SCL-045, decision 2026-10-25). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "100", ClampMax = "140", SafeMin = "115", SafeMax = "125", Latch = "Round", Owner = "SCL", Unit = "Cm"))
	float TraversalHurdleHeightCm = 120.0f;

	/** At or below: stepped; also the CMC MaxStepHeight (one key, R17.1). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "30", ClampMax = "50", SafeMin = "40", SafeMax = "50", Latch = "Round", Owner = "SCL", Unit = "Cm"))
	float TraversalMinHeightCm = 45.0f;

	/** Accepted deviation from the authored heights (F5). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "2", ClampMax = "20", SafeMin = "5", SafeMax = "12", Latch = "Round", Owner = "SCL", Unit = "Cm"))
	float TraversalHeightToleranceCm = 10.0f;

	/** Max horizontal distance capsule edge to coping at Traverse (F5). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "50", ClampMax = "200", SafeMin = "80", SafeMax = "120", Latch = "Round", Owner = "SCL", Unit = "Cm"))
	float TraversalReachCm = 100.0f;

	/** Body-facing cone toward the coping normal (F5; never the camera). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "20", ClampMax = "80", SafeMin = "35", SafeMax = "55", Latch = "Round", Owner = "SCL", Unit = "Deg"))
	float TraversalFacingHalfAngleDeg = 45.0f;

	/** Anticipation lead-in every pawn plays before the traversal clip (R7.5). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.0", ClampMax = "0.3", SafeMin = "0.1", SafeMax = "0.2", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float TraversalAnticipationSec = 0.15f;

	/** Simulated-proxy error above which the proxy snaps (F10, I-SCL-8). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "128", ClampMax = "512", SafeMin = "200", SafeMax = "300", Latch = "Round", Owner = "SCL", Unit = "Cm"))
	float PawnSmoothSnapThresholdCm = 256.0f;

	/** Proxy and listen-host smoothing time (R14, I-SCL-9). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.07", ClampMax = "0.25", SafeMin = "0.08", SafeMax = "0.12", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float PawnSmoothLocationSec = 0.10f;

	/** Animation tier A threshold (F7, I-SCL-3). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "10", ClampMax = "30", SafeMin = "17", SafeMax = "21", Latch = "Round", Owner = "SCL", Unit = "Pct"))
	float AnimTierAMinScreenPct = 19.0f;

	/** Animation tier B threshold (F7). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "4", ClampMax = "15", SafeMin = "7", SafeMax = "10", Latch = "Round", Owner = "SCL", Unit = "Pct"))
	float AnimTierBMinScreenPct = 8.0f;

	/** Animation tier C threshold; below is tier D (F7). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "1", ClampMax = "6", SafeMin = "2.5", SafeMax = "4", Latch = "Round", Owner = "SCL", Unit = "Pct"))
	float AnimTierCMinScreenPct = 3.0f;

	/** Relative band on each tier threshold (F7). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0", ClampMax = "20", SafeMin = "5", SafeMax = "10", Latch = "Round", Owner = "SCL", Unit = "Pct"))
	float AnimTierHysteresisPct = 8.0f;

	/** Minimum time in a tier (F7). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.0", ClampMax = "2.0", SafeMin = "0.3", SafeMax = "0.8", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float AnimTierMinHoldSec = 0.5f;

	/** Max pawns in tier A (F8). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "4", ClampMax = "12", SafeMin = "6", SafeMax = "10", Latch = "Round", Owner = "SCL", Unit = "Count"))
	int32 AnimTierACapCount = 8;

	/** Max pawns in tier B (F8). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "8", ClampMax = "24", SafeMin = "12", SafeMax = "16", Latch = "Round", Owner = "SCL", Unit = "Count"))
	int32 AnimTierBCapCount = 14;

	/** Max pawns in tier C (F8). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "20", ClampMax = "50", SafeMin = "24", SafeMax = "32", Latch = "Round", Owner = "SCL", Unit = "Count"))
	int32 AnimTierCCapCount = 28;

	/** Motion-matching search interval in tier B (time-based). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.033", ClampMax = "0.2", SafeMin = "0.05", SafeMax = "0.1", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float AnimSearchIntervalTierBSec = 0.067f;

	/** Walk cadence at the slowest pace (F11, I-SCL-13). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "1.6", ClampMax = "2.0", SafeMin = "1.75", SafeMax = "1.9", Latch = "Round", Owner = "SCL", Unit = "PerSec"))
	float FootstepCadenceSlowPerSec = 1.8f;

	/** Walk cadence at full pace (F11, I-SCL-13). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "1.8", ClampMax = "2.2", SafeMin = "1.9", SafeMax = "2.05", Latch = "Round", Owner = "SCL", Unit = "PerSec"))
	float FootstepCadenceFullPerSec = 2.0f;

	/** First step after a start from idle (R18.2). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.2", ClampMax = "0.5", SafeMin = "0.25", SafeMax = "0.4", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float FootstepStartDelaySec = 0.30f;

	/** Stride length at run speed (F11). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "90", ClampMax = "180", SafeMin = "115", SafeMax = "145", Latch = "Round", Owner = "SCL", Unit = "Cm"))
	float FootstepStrideRunCm = 130.0f;

	/** Facing error at which Pivot exits, and above which an idle pawn pivots first (R5.4). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "5", ClampMax = "20", SafeMin = "8", SafeMax = "12", Latch = "Round", Owner = "SCL", Unit = "Deg"))
	float PivotExitAngleDeg = 10.0f;

	/** Band below ReversalAngleDeg in which a reversal never triggers (R5.4). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "5", ClampMax = "20", SafeMin = "8", SafeMax = "15", Latch = "Round", Owner = "SCL", Unit = "Deg"))
	float ReversalHysteresisDeg = 10.0f;

	/** Idle re-centre stops when head yaw <= this times LookYawLimitDeg (R5.5). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.2", ClampMax = "0.8", SafeMin = "0.4", SafeMax = "0.6", Latch = "Round", Owner = "SCL", Unit = "Frac"))
	float TurnInPlaceExitFrac = 0.5f;

	/** Motion-matching search interval in tier C. */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0.067", ClampMax = "0.3", SafeMin = "0.1", SafeMax = "0.17", Latch = "Round", Owner = "SCL", Unit = "Sec"))
	float AnimSearchIntervalTierCSec = 0.133f;

	/** Minimum pose update rate of a rendered tier-D pawn (R11.3). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "6", ClampMax = "15", SafeMin = "8", SafeMax = "10", Latch = "Round", Owner = "SCL", Unit = "Hz"))
	float AnimTierDVisibleMinHz = 8.0f;

	/** Relative speed at which two touching pawns play the shared brush reaction (R5.8). */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "10", ClampMax = "100", SafeMin = "30", SafeMax = "60", Latch = "Round", Owner = "SCL", Unit = "CmPerSec"))
	float ContactBrushMinRelSpeedCmPerSec = 40.0f;
};
