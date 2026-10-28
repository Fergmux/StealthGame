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

/**
 * Assassination & Defence Reversal ServerOnly keys (GDD Tuning Knobs, audience ServerOnly; story asn-001): never
 * replicated, read only by the server resolver. The All keys are FAssassinationTuning_Public.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FAssassinationTuning_Server
{
	GENERATED_BODY()

	/** F1 Z: capsule-centre height difference limit of a strike, on server positions. */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "30", ClampMax = "120", SafeMin = "50", SafeMax = "100", Latch = "Use", Owner = "Assassination", Unit = "Cm"))
	float StrikeVerticalReachCm = 75.0f;

	/** Open-ground spot strike tolerance around the WorldPoint (equal to BushSpotToleranceCm and PawnRefAgreementCm; W-ASN-9). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "20", ClampMax = "200", SafeMin = "40", SafeMax = "120", Latch = "Round", Owner = "Assassination", Unit = "Cm"))
	float OpenSpotToleranceCm = 75.0f;

	/** F1 lambda: death-write lead before contact; t_d = T_v - lambda is the victim's free-motion time (I-ASN-4). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "0.05", ClampMax = "0.30", SafeMin = "0.10", SafeMax = "0.15", Latch = "Round", Owner = "Assassination", Unit = "Sec"))
	float StrikeNetLeadSec = 0.15f;
};

/**
 * NPC-Kill Penalty & Informants ServerOnly keys (GDD npc-kill-penalty-informants.md Tuning Knobs, audience ServerOnly;
 * story nkp-002). The All key NpcKillStrikeLimit is FPenaltyTuning_Public.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FPenaltyTuning_Server
{
	GENERATED_BODY()

	/** PEN-R5: a released exile body killed at most this long after its release (inclusive) adds no strike (W-NKP-3 vs the lockout). */
	UPROPERTY(EditAnywhere, Category = "Penalty", meta = (ClampMin = "0", ClampMax = "5.0", SafeMin = "1.75", SafeMax = "2.5", Latch = "Round", Owner = "Penalty", Unit = "Sec"))
	float StrikeOutReleaseGraceSec = 2.0f;
};

/**
 * Objectives Chain ServerOnly keys (GDD objectives-chain.md Tuning Knobs, audience ServerOnly; ADR-0020 section 8; story
 * oc-001): never replicated, read only by the server's Objectives core. A client module cannot name them (this header is
 * guarded). Invariants I-OC1..I-OC3 are ADR-0004 Check rows (Server/StealthObjectivesInvariants.h).
 */
USTRUCT()
struct STEALTHFOUNDATION_API FObjectivesTuning_Server
{
	GENERATED_BODY()

	/** Sites per preliminary stage (OC-3); a value above 1 fails Preparing (ObjectiveChainInvalid) until LSC Selection supports it. */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "1", ClampMax = "3", SafeMin = "1", SafeMax = "1", Latch = "Round", Owner = "Objectives", Unit = "Count"))
	int32 ObjectivesPerStageCount = 1;

	/** OC-12 rival race signal on/off (ON by default, user 2026-10-25). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (Latch = "Round", Owner = "Objectives", Unit = "Bool"))
	bool bRivalSignalEnabled = true;

	/** Earliest per-completion rival release delay (OC-12 1, F4; I-OC2). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "30", ClampMax = "120", SafeMin = "30", SafeMax = "60", Latch = "Round", Owner = "Objectives", Unit = "Sec"))
	float RivalSignalDelayMinSec = 30.0f;

	/** Latest per-completion rival release delay (F4; I-OC2: W_eff = Max - Min + Q, and Max >= Min + 30). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "70", ClampMax = "300", SafeMin = "163", SafeMax = "200", Latch = "Round", Owner = "Objectives", Unit = "Sec"))
	float RivalSignalDelayMaxSec = 165.0f;

	/** Global release grid period anchored at Active entry, shared by both teams (OC-12 2, F4). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "10", ClampMax = "90", SafeMin = "28", SafeMax = "45", Latch = "Round", Owner = "Objectives", Unit = "Sec"))
	float RivalSignalReleaseQuantumSec = 30.0f;

	/** Courts a stage clue may match before the layout logs ClueTooBroad (OC-15). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "1", ClampMax = "6", SafeMin = "2", SafeMax = "3", Latch = "Round", Owner = "Objectives", Unit = "Count"))
	int32 ClueMatchMaxCount = 3;

	/** F5 mutual-gap defect threshold (reporting only, never a rule). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "120", ClampMax = "900", SafeMin = "240", SafeMax = "420", Latch = "Use", Owner = "Objectives", Unit = "Sec"))
	float ObjectiveStallGapDefectSec = 300.0f;

	/** F5 final-dwell defect threshold (reporting only, never a rule). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "90", ClampMax = "600", SafeMin = "180", SafeMax = "360", Latch = "Use", Owner = "Objectives", Unit = "Sec"))
	float ObjectiveFinalDwellDefectSec = 240.0f;
};

/** Crowd & NPC Behaviour ServerOnly keys (the owner has no public key yet, so no _Public struct). */
USTRUCT()
struct STEALTHFOUNDATION_API FCrowdTuning_Server
{
	GENERATED_BODY()

	/** NPC characters in the crowd (default 50, user decision 2026-10-25). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "6", ClampMax = "100", SafeMin = "45", SafeMax = "50", Latch = "Round", Owner = "Crowd", Unit = "Count"))
	int32 NpcCount = 50;

	/** F1 attractor weight of Causeway areas (a court inherits its host area's weight). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "2", SafeMin = "0.5", SafeMax = "1.6", Latch = "Round", Owner = "Crowd", Unit = "Mult"))
	float CrowdAreaWeightCausewayMult = 1.4f;

	/** F1 attractor weight of Market areas. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "2", SafeMin = "0.5", SafeMax = "1.6", Latch = "Round", Owner = "Crowd", Unit = "Mult"))
	float CrowdAreaWeightMarketMult = 1.6f;

	/** F1 attractor weight of Temple areas. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "2", SafeMin = "0.5", SafeMax = "1.6", Latch = "Round", Owner = "Crowd", Unit = "Mult"))
	float CrowdAreaWeightTempleMult = 0.8f;

	/** F1 attractor weight of Residential areas. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "2", SafeMin = "0.5", SafeMax = "1.6", Latch = "Round", Owner = "Crowd", Unit = "Mult"))
	float CrowdAreaWeightResidentialMult = 0.6f;

	/** Walking-time share at pace 1.0 (R3.1; SCL R5.1 floor 50). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "50", ClampMax = "100", SafeMin = "50", SafeMax = "65", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdFullPacePct = 55.0f;

	/** B6 side-step probability at a predicted approach (R3.3). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "70", SafeMax = "90", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdYieldPct = 70.0f;

	/** Share of long C2 pauses (F4; the regulator raises it up to CrowdLongPauseMaxPct). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "10", ClampMax = "60", SafeMin = "35", SafeMax = "50", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdLongPausePct = 45.0f;

	/** Long-pause length minimum (I-CNB-1, I-5). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "2", ClampMax = "30", SafeMin = "4", SafeMax = "16", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdIdleMinSec = 4.0f;

	/** Long-pause length maximum (I-CNB-2, I-5). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "2", ClampMax = "30", SafeMin = "4", SafeMax = "16", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdIdleMaxSec = 16.0f;

	/** F4 regulator floor (Session 25f). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "30", SafeMin = "10", SafeMax = "20", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdStallCoverMinPct = 10.0f;

	/** Longest visible stationary stretch of any catalogue entry and restricted driver (I-CNB-2). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "10", ClampMax = "60", SafeMin = "20", SafeMax = "25", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdMaxIdleSec = 20.0f;

	/** C3 conversation length minimum. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "2", ClampMax = "60", SafeMin = "5", SafeMax = "20", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdConversationMinSec = 6.0f;

	/** C3 conversation length maximum (I-CNB-2). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "2", ClampMax = "60", SafeMin = "5", SafeMax = "20", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdConversationMaxSec = 20.0f;

	/** C3 face-a-resting-neighbour probability (kind-blind). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "30", SafeMax = "70", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdConverseStartPct = 50.0f;

	/** C3 face-back probability (kind-blind). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "40", SafeMax = "80", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdConverseReplyPct = 60.0f;

	/** C4 pace matching per leg at most this long. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "20", SafeMin = "3", SafeMax = "8", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdGroupDriftSec = 5.0f;

	/** C9 touch overlay length minimum (off in MVP; registered for I-OC1). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.3", ClampMax = "3", SafeMin = "0.6", SafeMax = "1.5", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdAmbientTouchMinSec = 0.8f;

	/** C9 touch overlay length maximum. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.3", ClampMax = "3", SafeMin = "0.6", SafeMax = "1.5", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdAmbientTouchMaxSec = 1.5f;

	/** T1 mean interval per NPC (60 / r_N, r_N 0.148/min). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "60", ClampMax = "3600", SafeMin = "300", SafeMax = "600", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdRunIntervalSec = 405.0f;

	/** T2 vault mean interval (safe +-40 %). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "120", ClampMax = "7200", SafeMin = "866", SafeMax = "2020", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdVaultIntervalSec = 1443.0f;

	/** T2 climb mean interval (safe +-40 %). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "120", ClampMax = "7200", SafeMin = "2045", SafeMax = "4773", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdClimbIntervalSec = 3409.0f;

	/** T4 mean interval (flinch + startle total). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "300", ClampMax = "7200", SafeMin = "2000", SafeMax = "4500", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdBraceIntervalSec = 3000.0f;

	/** Share of NPC braces held to the cap. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "40", SafeMax = "70", Latch = "Action", Owner = "Crowd", Unit = "Pct"))
	float CrowdBraceCapSharePct = 55.0f;

	/** T3 decoy tending mean interval (r_N 0.0046/min). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1800", ClampMax = "60000", SafeMin = "10500", SafeMax = "16875", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdDecoyTendIntervalSec = 13125.0f;

	/** Decoy tends held to completion. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "60", SafeMax = "90", Latch = "Action", Owner = "Crowd", Unit = "Pct"))
	float CrowdDecoyCompletePct = 75.0f;

	/** Decoy early-release time minimum. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.3", ClampMax = "3", SafeMin = "0.6", SafeMax = "1", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdDecoyEarlyReleaseMinSec = 0.8f;

	/** Decoy early-release time maximum (I-CNB-6). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "5", SafeMin = "2.2", SafeMax = "2.9", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdDecoyEarlyReleaseMaxSec = 2.8f;

	/** T5 bush entry mean interval (decision D7). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "120", ClampMax = "320", SafeMin = "240", SafeMax = "315", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdBushIntervalSec = 300.0f;

	/** Median of the NPC's uncapped log-normal bush stay. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "3", ClampMax = "120", SafeMin = "8", SafeMax = "25", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdBushDwellMedianSec = 15.0f;

	/** Log-sigma of the NPC bush stay. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.3", ClampMax = "1.5", SafeMin = "0.6", SafeMax = "1", Latch = "Action", Owner = "Crowd", Unit = "Sigma"))
	float CrowdBushDwellSigma = 0.8f;

	/** T12 mean interval (safe +-40 %). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "60", ClampMax = "3600", SafeMin = "237", SafeMax = "553", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReversalIntervalSec = 395.0f;

	/** T13 G6 clock mean interval (safe +-40 %). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "120", ClampMax = "3600", SafeMin = "360", SafeMax = "840", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdHeadHoldIntervalSec = 600.0f;

	/** Alley dawdle mean interval per NPC. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "300", ClampMax = "7200", SafeMin = "900", SafeMax = "1800", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdAlleyDawdleIntervalSec = 1200.0f;

	/** R8 stimulus radius. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "500", ClampMax = "3200", SafeMin = "1000", SafeMax = "2000", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdStrikeReactRadiusCm = 1500.0f;

	/** Panic-run and startle-brace radius. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "200", ClampMax = "1000", SafeMin = "400", SafeMax = "800", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdStartleRadiusCm = 600.0f;

	/** R8 panic-run share. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "20", SafeMin = "2", SafeMax = "5", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdPanicRunPct = 3.0f;

	/** R8 startle-brace share. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "20", SafeMin = "5", SafeMax = "12", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdStartleBracePct = 8.0f;

	/** Minimum gap between two different tells by one NPC (I-CNB-5). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "10", ClampMax = "35", SafeMin = "20", SafeMax = "30", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdTellChainGapSec = 20.0f;

	/** Responders per strike. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "20", SafeMin = "6", SafeMax = "10", Latch = "Use", Owner = "Crowd", Unit = "Limit"))
	int32 CrowdStrikeResponderLimit = 8;

	/** Per-NPC reaction cooldown. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "30", SafeMin = "5", SafeMax = "12", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReactRefractorySec = 8.0f;

	/** Ambient cycle interval per socket (I-CNB-3, I-CNB-4). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "40", ClampMax = "90", SafeMin = "60", SafeMax = "90", Latch = "Round", Owner = "Crowd", Unit = "Sec"))
	float CrowdAmbientCycleIntervalSec = 80.0f;

	/** Exit-to-entry gap minimum. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "2", ClampMax = "60", SafeMin = "5", SafeMax = "25", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdCycleGapMinSec = 6.0f;

	/** Exit-to-entry gap maximum. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "2", ClampMax = "60", SafeMin = "5", SafeMax = "25", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdCycleGapMaxSec = 20.0f;

	/** F3: no replacement before this after a kill. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "3", ClampMax = "30", SafeMin = "8", SafeMax = "15", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReplacementMinDelaySec = 10.0f;

	/** F3: fallback to the next clear socket after this. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "30", ClampMax = "180", SafeMin = "45", SafeMax = "90", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReplacementMaxDelaySec = 60.0f;

	/** A watched exit turns back after this. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "2", ClampMax = "30", SafeMin = "5", SafeMax = "12", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdExitGiveUpSec = 8.0f;

	/** StateTree decision rate (one global value, F5). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "10", SafeMin = "3", SafeMax = "6", Latch = "Use", Owner = "Crowd", Unit = "PerSec"))
	float CrowdDecisionPerSec = 4.0f;

	/** Follower carrot and path recompute rate (F5). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "10", ClampMax = "60", SafeMin = "20", SafeMax = "30", Latch = "Use", Owner = "Crowd", Unit = "PerSec"))
	float CrowdSteerPerSec = 30.0f;

	/** Async path starts per frame (F5 peak control). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "8", SafeMin = "2", SafeMax = "4", Latch = "Use", Owner = "Crowd", Unit = "Limit"))
	int32 CrowdPathStartsPerFrameLimit = 2;

	/** R8 share of no reaction. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "30", SafeMax = "50", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdReactNonePct = 40.0f;

	/** R8 share of a glance at the victim or strike point. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "20", SafeMax = "40", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdReactGlancePct = 30.0f;

	/** R8 share of a pause. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "8", SafeMax = "16", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdReactPausePct = 12.0f;

	/** R8 share of a walk-away reroute. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "4", SafeMax = "10", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdReactReroutePct = 7.0f;

	/** R8 glance length minimum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "10", SafeMin = "0.5", SafeMax = "2", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReactGlanceMinSec = 1.0f;

	/** R8 glance length maximum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "10", SafeMin = "2", SafeMax = "4", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReactGlanceMaxSec = 3.0f;

	/** R8 pause length minimum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.5", ClampMax = "15", SafeMin = "1", SafeMax = "3", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReactPauseMinSec = 2.0f;

	/** R8 pause length maximum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.5", ClampMax = "15", SafeMin = "4", SafeMax = "6", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReactPauseMaxSec = 5.0f;

	/** R8 panic run at most this long. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.5", ClampMax = "10", SafeMin = "2", SafeMax = "4", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdPanicRunMaxSec = 3.0f;

	/** Reaction delay minimum (logged player reaction times; provisional U(0.3, 1.2) s). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.05", ClampMax = "3", SafeMin = "0.2", SafeMax = "0.4", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReactDelayMinSec = 0.3f;

	/** Reaction delay maximum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.05", ClampMax = "3", SafeMin = "1", SafeMax = "1.5", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdReactDelayMaxSec = 1.2f;

	/** C1 leg length minimum (10 m). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "200", ClampMax = "6000", SafeMin = "800", SafeMax = "1200", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdLegMinCm = 1000.0f;

	/** C1 leg length maximum (30 m). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "200", ClampMax = "6000", SafeMin = "2500", SafeMax = "3500", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdLegMaxCm = 3000.0f;

	/** C2 short pause minimum (short share = 100 - CrowdLongPausePct). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.5", ClampMax = "4", SafeMin = "0.8", SafeMax = "1.5", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdShortPauseMinSec = 1.0f;

	/** C2 short pause maximum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.5", ClampMax = "4", SafeMin = "2.5", SafeMax = "3.5", Latch = "Action", Owner = "Crowd", Unit = "Sec"))
	float CrowdShortPauseMaxSec = 3.0f;

	/** F4 regulator cap on the long-pause share. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "50", ClampMax = "100", SafeMin = "80", SafeMax = "90", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdLongPauseMaxPct = 90.0f;

	/** A long pause faces a stall, mural or view within this radius. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "100", ClampMax = "600", SafeMin = "250", SafeMax = "350", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdPauseContextRadiusCm = 300.0f;

	/** G2 nearest moving pawn within this range. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "200", ClampMax = "1500", SafeMin = "500", SafeMax = "700", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdGlanceRangeCm = 600.0f;

	/** G2 hold minimum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.1", ClampMax = "3", SafeMin = "0.3", SafeMax = "0.5", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdGlanceHoldMinSec = 0.4f;

	/** G2 hold maximum (T13 starts above 2.5 s). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.5", ClampMax = "6", SafeMin = "2", SafeMax = "2.5", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdGlanceHoldMaxSec = 2.5f;

	/** G2 glance interval minimum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "30", SafeMin = "3", SafeMax = "5", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdGlanceIntervalMinSec = 4.0f;

	/** G2 glance interval maximum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "30", SafeMin = "8", SafeMax = "12", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdGlanceIntervalMaxSec = 10.0f;

	/** G3 goods hold minimum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "10", SafeMin = "0.8", SafeMax = "1.5", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdGoodsHoldMinSec = 1.0f;

	/** G3 goods hold maximum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "10", SafeMin = "3", SafeMax = "5", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdGoodsHoldMaxSec = 4.0f;

	/** G3 downward head pitch minimum (degrees below level). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "30", SafeMin = "8", SafeMax = "12", Latch = "Use", Owner = "Crowd", Unit = "Deg"))
	float CrowdGoodsPitchMinDeg = 10.0f;

	/** G3 downward head pitch maximum (inside LookPitchLimitDeg). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "30", SafeMin = "18", SafeMax = "20", Latch = "Use", Owner = "Crowd", Unit = "Deg"))
	float CrowdGoodsPitchMaxDeg = 20.0f;

	/** G4 salient motion within this range. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "500", ClampMax = "3200", SafeMin = "1000", SafeMax = "2000", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdSalientRangeCm = 1500.0f;

	/** G4 glance probability (run, traversal). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "40", SafeMax = "60", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdSalientGlancePct = 50.0f;

	/** G5 partner-look share minimum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "55", SafeMax = "65", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdPartnerLookMinPct = 60.0f;

	/** G5 partner-look share maximum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100", SafeMin = "75", SafeMax = "85", Latch = "Use", Owner = "Crowd", Unit = "Pct"))
	float CrowdPartnerLookMaxPct = 80.0f;

	/** G6 long hold minimum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "10", SafeMin = "2.5", SafeMax = "3", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdHeadHoldMinSec = 2.5f;

	/** G6 long hold maximum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "10", SafeMin = "5", SafeMax = "7", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdHeadHoldMaxSec = 6.0f;

	/** C3 partner within this range. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "50", ClampMax = "400", SafeMin = "120", SafeMax = "180", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdConverseRangeCm = 150.0f;

	/** C3 both face each other within this angle. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "10", ClampMax = "90", SafeMin = "50", SafeMax = "70", Latch = "Use", Owner = "Crowd", Unit = "Deg"))
	float CrowdConverseFaceDeg = 60.0f;

	/** C3 unanswered initiator turns away after at least this. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.5", ClampMax = "10", SafeMin = "1.5", SafeMax = "2.5", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdConverseTurnAwayMinSec = 2.0f;

	/** C3 unanswered initiator turns away after at most this. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.5", ClampMax = "10", SafeMin = "4", SafeMax = "6", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdConverseTurnAwayMaxSec = 5.0f;

	/** C4 neighbour within this range. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "50", ClampMax = "500", SafeMin = "150", SafeMax = "250", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdGroupDriftRangeCm = 200.0f;

	/** C4 neighbour heading within this angle. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "5", ClampMax = "90", SafeMin = "20", SafeMax = "40", Latch = "Use", Owner = "Crowd", Unit = "Deg"))
	float CrowdGroupDriftHeadingDeg = 30.0f;

	/** C6 idle gaze beyond this re-centres the body. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "30", ClampMax = "120", SafeMin = "60", SafeMax = "80", Latch = "Use", Owner = "Crowd", Unit = "Deg"))
	float CrowdRecentreAngleDeg = 70.0f;

	/** C6 after this long beyond the angle. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.1", ClampMax = "3", SafeMin = "0.4", SafeMax = "0.7", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdRecentreDelaySec = 0.5f;

	/** R3.3 predicted closest approach below this triggers avoidance. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "30", ClampMax = "300", SafeMin = "80", SafeMax = "120", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdAvoidTriggerCm = 100.0f;

	/** R3.3 lateral carrot offset. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "10", ClampMax = "150", SafeMin = "40", SafeMax = "60", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdAvoidOffsetCm = 50.0f;

	/** R3.3 widened offset when blocked. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "20", ClampMax = "300", SafeMin = "80", SafeMax = "120", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdAvoidWideOffsetCm = 100.0f;

	/** R3.3 yield jitter minimum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "2", SafeMin = "0.15", SafeMax = "0.25", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdYieldJitterMinSec = 0.2f;

	/** R3.3 yield jitter maximum. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "2", SafeMin = "0.5", SafeMax = "0.7", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdYieldJitterMaxSec = 0.6f;

	/** Blocked this long widens the offset. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.1", ClampMax = "3", SafeMin = "0.4", SafeMax = "0.7", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdBlockedWidenSec = 0.5f;

	/** Blocked this long repaths once. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "10", SafeMin = "2.5", SafeMax = "4", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdBlockedRepathSec = 3.0f;

	/** Progress monitor: less than this net progress... (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "10", ClampMax = "200", SafeMin = "40", SafeMax = "60", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdProgressMinCm = 50.0f;

	/** ...in this window counts as blocked. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "15", SafeMin = "4", SafeMax = "6", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdProgressWindowSec = 5.0f;

	/** Planted and tending pawns are static obstacles of this radius plus margin (SCL R16). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "20", ClampMax = "150", SafeMin = "40", SafeMax = "60", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdStaticObstacleCm = 50.0f;

	/** R3.2 per-pawn lane offset half-width (+-0.5 m). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "150", SafeMin = "40", SafeMax = "60", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdLaneOffsetCm = 50.0f;

	/** R3.2 pure-pursuit carrot minimum distance. (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "50", ClampMax = "400", SafeMin = "120", SafeMax = "180", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdCarrotMinCm = 150.0f;

	/** R3.2 carrot distance = max(CrowdCarrotMinCm, v x this). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "3", SafeMin = "0.8", SafeMax = "1.2", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdCarrotLeadSec = 1.0f;

	/** A rejected target is not retried for this long (R3). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "60", SafeMin = "8", SafeMax = "15", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdRetryBlockSec = 10.0f;

	/** Dry-run reach margin (R3). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "50", SafeMin = "5", SafeMax = "15", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdDryRunReachCm = 10.0f;

	/** Dry-run facing margin (R3). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "45", SafeMin = "5", SafeMax = "15", Latch = "Use", Owner = "Crowd", Unit = "Deg"))
	float CrowdDryRunFacingDeg = 10.0f;

	/** An invalid path holds the last Move this long, then Move(0). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.1", ClampMax = "2", SafeMin = "0.4", SafeMax = "0.6", Latch = "Use", Owner = "Crowd", Unit = "Sec"))
	float CrowdPathInvalidHoldSec = 0.5f;

	/** Spatial hash cell size (2 m, ADR-0016). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "50", ClampMax = "800", SafeMin = "200", SafeMax = "200", Latch = "Use", Owner = "Crowd", Unit = "Cm"))
	float CrowdHashCellCm = 200.0f;

	/** Line-of-sight rays per frame (ADR-0016 section 5). (GDD OQ13 constant, registered by story cnb-003) */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "1", ClampMax = "64", SafeMin = "6", SafeMax = "10", Latch = "Use", Owner = "Crowd", Unit = "Limit"))
	int32 CrowdRaysPerFrameLimit = 8;
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
