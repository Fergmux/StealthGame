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
