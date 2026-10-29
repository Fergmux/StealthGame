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

/**
 * Assassination & Defence Reversal keys a client needs (GDD assassination-defence-reversal.md Tuning Knobs, audience All;
 * story asn-001). Presentation timings and limits every client needs to play the clips or show its own owner feedback;
 * one value for every pawn, no per-kind variant. The ServerOnly keys are FAssassinationTuning_Server. Cross-field
 * invariants I-ASN-1 to I-ASN-8, I-ASN-5w, W-ASN-9 to W-ASN-11: Server/StealthAssassinationInvariants.h.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FAssassinationTuning_Public
{
	GENERATED_BODY()

	/** F1 R: reach from the attacker's capsule axis to the victim's capsule surface, cm (hard max 250 by I-ASN-1). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "100", ClampMax = "250", SafeMin = "150", SafeMax = "250", Latch = "Use", Owner = "Assassination", Unit = "Cm"))
	float AssassinationRangeCm = 200.0f;

	/** F1 Theta: widest bearing from the attacker's body facing a strike reaches (the 60 degree side variant plus the warp turn, I-ASN-8). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "60", ClampMax = "105", SafeMin = "80", SafeMax = "90", Latch = "Use", Owner = "Assassination", Unit = "Deg"))
	float StrikeBearingLimitDeg = 90.0f;

	/** Cap of the defence's protected window: a held Prepare protects for at most this long (F2, TDC I-1). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "3.0", ClampMax = "5.0", SafeMin = "3.0", SafeMax = "5.0", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float DefenceDurationSec = 4.0f;

	/** Defence cooldown measured from activation (Prepare Begin execution), used or unused (F3 C). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "30", ClampMax = "120", SafeMin = "45", SafeMax = "75", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float DefenceCooldownSec = 60.0f;

	/** F1 T_v: execution-to-contact time of the standard strike (I-ASN-2, 3, 4, 6, 7). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "0.25", ClampMax = "0.80", SafeMin = "0.30", SafeMax = "0.50", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float StrikeContactSec = 0.40f;

	/** F1 X_max: maximum warp translation of the lunge (warp-scale cap 1.6 x about 150 cm of authored root motion; I-ASN-1, I-ASN-7). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "150", ClampMax = "240", SafeMin = "200", SafeMax = "240", Latch = "Use", Owner = "Assassination", Unit = "Cm"))
	float StrikeWarpLimitCm = 240.0f;

	/** F1 g: authored capsule gap at contact. */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "5", ClampMax = "40", SafeMin = "10", SafeMax = "25", Latch = "Use", Owner = "Assassination", Unit = "Cm"))
	float StrikeContactGapCm = 15.0f;

	/** Presentation cap on the lunge's warp turn (I-ASN-8). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "10", ClampMax = "45", SafeMin = "30", SafeMax = "35", Latch = "Use", Owner = "Assassination", Unit = "Deg"))
	float StrikeWarpYawLimitDeg = 30.0f;

	/** F3 Lambda: attacker lockout after any executed Attack (I-ASN-2, I-ASN-6, W-ASN-10). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "1.2", ClampMax = "3.0", SafeMin = "1.5", SafeMax = "2.0", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float AttackAttemptLockoutSec = 1.75f;

	/** Length of the defender's counter after a reversal; not protection (I-ASN-2). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "0.8", ClampMax = "2.0", SafeMin = "1.0", SafeMax = "1.5", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float CounterDurationSec = 1.2f;

	/** Fall length of every victim kind; earliest informant takeover is DeathServerTime + this (I-ASN-5, I-ASN-5w, W-ASN-11). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "0.8", ClampMax = "2.0", SafeMin = "1.1", SafeMax = "1.6", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float DeathFallSec = 1.2f;

	/** How long a body lingers after its fall, one value for every victim kind (I-ASN-5w, W-ASN-11). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "2", ClampMax = "12", SafeMin = "4", SafeMax = "8", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float BodyLingerSec = 6.0f;

	/** Client wait for a body to leave view before a one-frame cut (VA-ASN-2; I-ASN-5w, W-ASN-11). */
	UPROPERTY(EditAnywhere, Category = "Assassination", meta = (ClampMin = "0", ClampMax = "2.5", SafeMin = "1.5", SafeMax = "2.5", Latch = "Action", Owner = "Assassination", Unit = "Sec"))
	float BodyRemovalGraceSec = 2.0f;
};

/** NPC-Kill Penalty & Informants keys a client needs. */
USTRUCT()
struct STEALTHFOUNDATION_API FPenaltyTuning_Public
{
	GENERATED_BODY()

	/** Removal on the Nth NPC kill (hard minimum 1 only in a bTestAllowsStrikeLimitOne preset, applied by F-1). */
	UPROPERTY(EditAnywhere, Category = "Penalty", meta = (ClampMin = "2", ClampMax = "5", SafeMin = "2", SafeMax = "4", Latch = "Round", Owner = "Penalty", Unit = "Limit"))
	int32 NpcKillStrikeLimit = 3;

	/** F2 D: time from the loss (kill, exile, disconnect release) to the takeover attempt; the same for every cause (PEN-R8, story nkp-009). */
	UPROPERTY(EditAnywhere, Category = "Penalty", meta = (ClampMin = "5", ClampMax = "60", SafeMin = "10", SafeMax = "20", Latch = "Round", Owner = "Penalty", Unit = "Sec"))
	float InformantRespawnDelaySec = 15.0f;

	/** Pause between the WrongTarget plate and the exile card entrance, the exile's one clock (PEN-R16, W-NKP-5; story nkp-020 registered it, the HUD and Camera read it from story 017). */
	UPROPERTY(EditAnywhere, Category = "Penalty", meta = (ClampMin = "0.0", ClampMax = "2.0", SafeMin = "0.75", SafeMax = "0.95", Latch = "Action", Owner = "Penalty", Unit = "Sec"))
	float ExileBeatSec = 0.8f;

	/** Fallback for the exile clock: Camera fades anyway when the card has not entered this long after the StrikeLimit state arrived (PEN-R16, W-NKP-5; story nkp-020). */
	UPROPERTY(EditAnywhere, Category = "Penalty", meta = (ClampMin = "1.0", ClampMax = "4.0", SafeMin = "2.0", SafeMax = "2.5", Latch = "Action", Owner = "Penalty", Unit = "Sec"))
	float ExileCardTimeoutSec = 2.0f;
};

/**
 * Objectives Chain keys a client needs (GDD objectives-chain.md Tuning Knobs, audience All; ADR-0020 section 8; story oc-001):
 * the owner's ring prediction reads them on the client face. ObjectiveInteractSec is the TDC canonical row (story tdc-040);
 * TendCueDelaySec and FixtureInteractSideCount are Objectives' own All keys. The eight ServerOnly keys are
 * FObjectivesTuning_Server (Server/StealthTuningSubstructs.h), never nameable here.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FObjectivesTuning_Public
{
	GENERATED_BODY()

	/** Hold-to-interact time at an objective in seconds: one chain-wide D for every court (OC-14; never per court, ADR-0020 5.2). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "2.0", ClampMax = "6.0", SafeMin = "2.9", SafeMax = "4.5", Latch = "Action", Owner = "Objectives", Unit = "Sec"))
	float ObjectiveInteractSec = 3.0f;

	/** Time after Begin before the owner's ring turns Marigold at an own live site (OC-6; invariant I-OC3). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "0.5", ClampMax = "2.0", SafeMin = "0.8", SafeMax = "1.5", Latch = "Action", Owner = "Objectives", Unit = "Sec"))
	float TendCueDelaySec = 1.0f;

	/** Tend stand points per court fixture, one per face (OC-8); a fixture with every point claimed rejects a Begin (OC-7). */
	UPROPERTY(EditAnywhere, Category = "Objectives", meta = (ClampMin = "1", ClampMax = "4", SafeMin = "3", SafeMax = "4", Latch = "Round", Owner = "Objectives", Unit = "Count"))
	int32 FixtureInteractSideCount = 4;
};

/**
 * Concealment (Bushes) keys a client needs (GDD concealment-bushes.md Tuning Knobs, audience All; ADR-0017 section 3; story
 * con-001): every client plays the hide clips and runs the body-hidden latch from these, so they replicate with the snapshot.
 * F3 (the timing chain) is not a set of independent ranges: it is the ADR-0004 Check rows I-CON-F3a..f
 * (Server/StealthConcealmentInvariants.h), run at preset load. No key names a dwell, capacity, stay, reveal or timeout (P4).
 * The four ServerOnly keys are FConcealmentTuning_Server (Server/StealthTuningSubstructs.h), never nameable here.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FConcealmentTuning_Public
{
	GENERATED_BODY()

	/** Hide(Enter) transition length: execution to the End item (CON-R5; F3a, F3e). */
	UPROPERTY(EditAnywhere, Category = "Concealment", meta = (ClampMin = "0.6", ClampMax = "1.5", SafeMin = "0.8", SafeMax = "1.3", Latch = "Round", Owner = "Concealment", Unit = "Sec"))
	float BushEnterDurSec = 0.9f;

	/** Execution to the Commit item and the leaf-plane swap; the clip's swap notify equals it within one frame (F3a, F3d, F3e). */
	UPROPERTY(EditAnywhere, Category = "Concealment", meta = (ClampMin = "0.3", ClampMax = "0.8", SafeMin = "0.46", SafeMax = "0.65", Latch = "Round", Owner = "Concealment", Unit = "Sec"))
	float BushEnterSwapSec = 0.5f;

	/** Hide(Exit) transition length: execution to the End item (CON-R8; F3e). */
	UPROPERTY(EditAnywhere, Category = "Concealment", meta = (ClampMin = "0.6", ClampMax = "1.5", SafeMin = "0.8", SafeMax = "1.3", Latch = "Round", Owner = "Concealment", Unit = "Sec"))
	float BushExitDurSec = 0.9f;

	/** Exit execution to the outward leaf crossing (the Cross item; the body shows from here; F3b, F3e). */
	UPROPERTY(EditAnywhere, Category = "Concealment", meta = (ClampMin = "0.3", ClampMax = "0.8", SafeMin = "0.35", SafeMax = "0.7", Latch = "Round", Owner = "Concealment", Unit = "Sec"))
	float BushExitCrossSec = 0.45f;

	/** Strike-from-bush execution to its leaf crossing (CON-R9; F3c, F3f). The clip is Assassination's. */
	UPROPERTY(EditAnywhere, Category = "Concealment", meta = (ClampMin = "0.3", ClampMax = "0.6", SafeMin = "0.3", SafeMax = "0.5", Latch = "Round", Owner = "Concealment", Unit = "Sec"))
	float BushStrikeEmergeSec = 0.35f;

	/** Enter reach: horizontal capsule-centre distance to the planter's outer edge on an open side (CON-R3; F3d). */
	UPROPERTY(EditAnywhere, Category = "Concealment", meta = (ClampMin = "20", ClampMax = "150", SafeMin = "30", SafeMax = "70", Latch = "Round", Owner = "Concealment", Unit = "Cm"))
	float BushEntryReachCm = 60.0f;

	/** Rustle pulse amplitude (presentation, VA-2): passes the 20 m read test and fails at twice the value. */
	UPROPERTY(EditAnywhere, Category = "Concealment", meta = (ClampMin = "4", ClampMax = "20", SafeMin = "8", SafeMax = "12", Latch = "Use", Owner = "Concealment", Unit = "Cm"))
	float BushRustleAmplitudeCm = 10.0f;
};

/**
 * Camera System keys (GDD camera-system.md Tuning Knobs, owner Camera, audience Client: one value for every client, no NPC or
 * player variant; ADR-0015 sections 2 and 7; story cs-001). Owner-local presentation on the client face. The keys marked *
 * in the GDD (FpFovVDeg, TpFovVDeg, StickDeadZoneFrac, StickCurveExp, StickYawRateDegPerSec, StickPitchRateDegPerSec) are
 * local user settings, not tuning keys: their hard ranges and defaults are FStealthCameraUserSettings
 * (View/Tuning/StealthCameraParams.h, TDC CR-14). The invariants I-CS-1..5 are TDC Check rows
 * (Server/StealthCameraInvariants.h). The camera cores read these through the plain FStealthCameraParams, never this UStruct.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FCameraTuning_Public
{
	GENERATED_BODY()

	/** OQ2 fallback (CS-R3.5): the view may lead the visible head by at most this. Disabled by default: the sentinel 0 (35 when enabled). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "15", ClampMax = "70", SafeMin = "25", SafeMax = "45", Latch = "Round", Owner = "Camera", Unit = "Deg", Sentinel = "0"))
	float ViewHeadLeadMaxDeg = 0.0f;

	/** Local view and aim pitch limit downward, both modes (CS-R3.1; SCL D6). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "20", ClampMax = "75", SafeMin = "50", SafeMax = "65", Latch = "Round", Owner = "Camera", Unit = "Deg"))
	float ViewPitchDownLimitDeg = 60.0f;

	/** Local view and aim pitch limit upward, both modes (CS-R3.1). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "20", ClampMax = "60", SafeMin = "30", SafeMax = "45", Latch = "Round", Owner = "Camera", Unit = "Deg"))
	float ViewPitchUpLimitDeg = 40.0f;

	/** Elastic overshoot of the target past the yaw limit (F1); must be >= 2 (I-CS-5) so SCL R5.5 sees the crossing. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "2", ClampMax = "10", SafeMin = "3", SafeMax = "8", Latch = "Use", Owner = "Camera", Unit = "Deg"))
	float CameraEdgePushDeg = 5.0f;

	/** Raw outward look rate that counts as a push at the limit (F1). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "5", ClampMax = "40", SafeMin = "10", SafeMax = "25", Latch = "Use", Owner = "Camera", Unit = "PerSec"))
	float EdgePushMinRateDegPerSec = 15.0f;

	/** A push stays latched this long across a gap in outward input (mouse re-centre; F1). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.05", ClampMax = "0.3", SafeMin = "0.1", SafeMax = "0.2", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float EdgePushLatchSec = 0.15f;

	/** Look gain at the hard limit (the soft edge's floor; F1). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.05", ClampMax = "0.3", SafeMin = "0.08", SafeMax = "0.15", Latch = "Use", Owner = "Camera", Unit = "Gain"))
	float CameraEdgeSoftMinGain = 0.1f;

	/** Rate the target relaxes back to the limit when the push ends (F1). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "20", ClampMax = "200", SafeMin = "40", SafeMax = "120", Latch = "Use", Owner = "Camera", Unit = "PerSec"))
	float EdgePushRelaxDegPerSec = 60.0f;

	/** Yaw width of the soft edge inside the limit (F1); 0 is a hard wall. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "15", SafeMin = "6", SafeMax = "12", Latch = "Use", Owner = "Camera", Unit = "Deg"))
	float CameraEdgeSoftYawDeg = 10.0f;

	/** Pitch width of the soft edge inside the limits (F1). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "10", SafeMin = "3", SafeMax = "6", Latch = "Use", Owner = "Camera", Unit = "Deg"))
	float CameraEdgeSoftPitchDeg = 5.0f;

	/** Pivot smoothing lag (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "0.25", SafeMin = "0.06", SafeMax = "0.14", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float CameraPivotLagSec = 0.1f;

	/** Pivot smoothing lag during traversal (the GDD row's parenthesised value; F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "0.25", SafeMin = "0.06", SafeMax = "0.14", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float CameraPivotLagTraversalSec = 0.12f;

	/** Vertical pivot speed cap (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "100", ClampMax = "600", SafeMin = "200", SafeMax = "400", Latch = "Use", Owner = "Camera", Unit = "CmPerSec"))
	float PivotMaxVertCmPerSec = 300.0f;

	/** Third-person boom length (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "100", ClampMax = "250", SafeMin = "150", SafeMax = "200", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float BoomLengthCm = 180.0f;

	/** Shoulder (lateral) offset (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "60", SafeMin = "35", SafeMax = "50", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float ShoulderOffsetCm = 45.0f;

	/** Boom height above the pivot; must be <= 25 (I-CS-2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "25", SafeMin = "5", SafeMax = "20", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float BoomHeightOffsetCm = 15.0f;

	/** Guard radius of the boom collision (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1", ClampMax = "5", SafeMin = "2", SafeMax = "2", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float CameraGuardRadiusCm = 2.0f;

	/** Probe radius of the boom collision (F2); below 12 the near plane clips walls. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "10", ClampMax = "20", SafeMin = "12", SafeMax = "16", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float CameraProbeRadiusCm = 14.0f;

	/** Hold before a compressed boom recovers (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "0.5", SafeMin = "0.1", SafeMax = "0.2", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float BoomRecoverHoldSec = 0.15f;

	/** Boom recovery time constant (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.1", ClampMax = "0.6", SafeMin = "0.25", SafeMax = "0.4", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float BoomRecoverTauSec = 0.3f;

	/** Boom recovery speed cap (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "100", ClampMax = "800", SafeMin = "200", SafeMax = "450", Latch = "Use", Owner = "Camera", Unit = "CmPerSec"))
	float BoomRecoverCmPerSec = 300.0f;

	/** Shoulder squeeze-in time (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.05", ClampMax = "0.5", SafeMin = "0.1", SafeMax = "0.2", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float ShoulderSqueezeInSec = 0.15f;

	/** Shoulder squeeze-out time (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.1", ClampMax = "1", SafeMin = "0.3", SafeMax = "0.5", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float ShoulderSqueezeOutSec = 0.4f;

	/** Free boom under which the own head hides (F2). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "30", ClampMax = "100", SafeMin = "50", SafeMax = "80", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float BoomCloseCm = 60.0f;

	/** Boom inside a bush, straight back (CS-R11.3); <= 70 and <= BoomLengthCm (I-CS-4). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "70", SafeMin = "40", SafeMax = "60", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float ConcealedBoomCm = 60.0f;

	/** First-person lens forward of the eye (CS-R5a). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "20", SafeMin = "5", SafeMax = "12", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float FpForwardOffsetCm = 8.0f;

	/** D6: the mask turns off below 25 cm of free boom and on again above 25 + this (35 cm at the default). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "5", ClampMax = "30", SafeMin = "8", SafeMax = "15", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float CameraMaskOffHysteresisCm = 10.0f;

	/** Near clip plane. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1", ClampMax = "15", SafeMin = "3", SafeMax = "8", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float CameraNearClipCm = 5.0f;

	/** Eye-mask cone half-angle, horizontal (F3); >= FovHMaxDeg / 2 (I-CS-3). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "40", ClampMax = "70", SafeMin = "54", SafeMax = "60", Latch = "Round", Owner = "Camera", Unit = "Deg"))
	float EyeConeHalfHDeg = 55.0f;

	/** Eye-mask cone half-angle, vertical (F3); >= FovVMaxDeg / 2 (I-CS-3). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "25", ClampMax = "50", SafeMin = "38", SafeMax = "42", Latch = "Round", Owner = "Camera", Unit = "Deg"))
	float EyeConeHalfVDeg = 38.0f;

	/** Consecutive failed eye tests before a pawn fades out (F3). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1", ClampMax = "4", SafeMin = "2", SafeMax = "3", Latch = "Use", Owner = "Camera", Unit = "Count"))
	int32 EyeMaskHideFailCount = 2;

	/** Eye-mask fade time (F3). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.05", ClampMax = "0.3", SafeMin = "0.08", SafeMax = "0.12", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float EyeMaskFadeSec = 0.1f;

	/** Eye-mask near band (trace rate EyeMaskNearHz; F3). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1000", ClampMax = "4000", SafeMin = "1000", SafeMax = "4000", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float EyeMaskNearCm = 2000.0f;

	/** Eye-mask far band edge (beyond it EyeMaskBeyondHz; F3). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "3000", ClampMax = "10000", SafeMin = "3000", SafeMax = "10000", Latch = "Use", Owner = "Camera", Unit = "Cm"))
	float EyeMaskRangeCm = 6000.0f;

	/** Eye-test rate inside EyeMaskNearCm (F3). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "30", ClampMax = "60", SafeMin = "45", SafeMax = "60", Latch = "Use", Owner = "Camera", Unit = "Hz"))
	float EyeMaskNearHz = 60.0f;

	/** Eye-test rate up to EyeMaskRangeCm (F3). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "10", ClampMax = "30", SafeMin = "12", SafeMax = "20", Latch = "Use", Owner = "Camera", Unit = "Hz"))
	float EyeMaskFarHz = 15.0f;

	/** Eye-test rate beyond EyeMaskRangeCm (F3; the line-of-sight test still runs). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "2", ClampMax = "15", SafeMin = "4", SafeMax = "8", Latch = "Use", Owner = "Camera", Unit = "Hz"))
	float EyeMaskBeyondHz = 5.0f;

	/** 1P/3P toggle blend (F4); <= CameraToggleLockoutSec - 1/60 s (I-CS-1, reject, no runtime clamp). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "0.28", SafeMin = "0.15", SafeMax = "0.25", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float CameraToggleBlendSec = 0.2f;

	/** Shoulder swap blend. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "0.5", SafeMin = "0.2", SafeMax = "0.35", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float ShoulderSwapBlendSec = 0.25f;

	/** Fade to and from the neutral view when no pawn is possessed (CS-R13). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "1.5", SafeMin = "0.3", SafeMax = "0.8", Latch = "Use", Owner = "Camera", Unit = "Sec"))
	float NoPawnFadeSec = 0.5f;

	/** Vertical FOV setting minimum (F5). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "45", ClampMax = "90", SafeMin = "55", SafeMax = "55", Latch = "Round", Owner = "Camera", Unit = "Deg"))
	float FovVMinDeg = 55.0f;

	/** Vertical FOV setting maximum (F5); raises everyone's periphery equally. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "45", ClampMax = "90", SafeMin = "75", SafeMax = "75", Latch = "Round", Owner = "Camera", Unit = "Deg"))
	float FovVMaxDeg = 75.0f;

	/** Horizontal FOV cap for any aspect (fairness; F5). */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "90", ClampMax = "120", SafeMin = "100", SafeMax = "110", Latch = "Round", Owner = "Camera", Unit = "Deg"))
	float FovHMaxDeg = 108.0f;

	/** Stick outer zone (F6): deflection at or above it is full deflection. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.9", ClampMax = "1", SafeMin = "0.95", SafeMax = "0.99", Latch = "Use", Owner = "Camera", Unit = "Frac"))
	float StickOuterZoneFrac = 0.98f;
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
