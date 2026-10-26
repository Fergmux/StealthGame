#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

struct FStealthServerTuning;

/**
 * The Crowd soft-reference tuning keys (GDD Tuning Knobs CrowdMotionProfileAsset, CrowdFullStateTreeAsset,
 * CrowdRestrictedStateTreeAsset; ADR-0016 sections 3 and 4.5, ADR-0004; story cnb-003). The numeric Crowd keys are
 * schema rows of FCrowdTuning_Server; these three are not, because the schema has no SoftRef row type yet
 * (StealthTuningTypeTag::SoftRef is reserved, the generator rejects it). Until the tuning epic adds it they live here:
 * one ServerOnly value per round, validated by StealthCrowdTuning::ValidateAssetKeys when the Director freezes its
 * snapshot, and folded into the Crowd round hash as F-2 SoftRef entries. Never on the client face.
 */
struct STEALTHGAMEPLAY_API FCrowdAssetKeys
{
	/** UCrowdMotionProfileData: device mix, quantisation, view-envelope pushes, turn-rate and glance distributions. */
	FSoftObjectPath CrowdMotionProfileAsset{ TEXT("/Game/StealthGame/AI/DA_CrowdMotionProfile_Default.DA_CrowdMotionProfile_Default") };
	/** The unbound-NPC StateTree (the GDD States table) on the crowd schema. */
	FSoftObjectPath CrowdFullStateTreeAsset{ TEXT("/Game/StealthGame/AI/ST_CrowdFull.ST_CrowdFull") };
	/** The participant-bound restricted subset (Routing, Pausing, Halted). */
	FSoftObjectPath CrowdRestrictedStateTreeAsset{ TEXT("/Game/StealthGame/AI/ST_CrowdRestricted.ST_CrowdRestricted") };
};

/** Result of one asset key check. */
enum class ECrowdAssetCheck : uint8
{
	Valid,
	/** Null path, or no asset loads from it. */
	Missing,
	/** The asset is not of the required class. */
	WrongClass,
	/** A StateTree that is not compiled against the crowd schema with ScheduledTickPolicy = Allowed. */
	WrongSchema,
};

namespace StealthCrowdTuning
{
	/** The class path the motion profile key requires. */
	inline const TCHAR* MotionProfileClassPath() { return TEXT("/Script/StealthGameplay.CrowdMotionProfileData"); }
	/** The class path the StateTree keys require. */
	inline const TCHAR* StateTreeClassPath() { return TEXT("/Script/StateTreeModule.StateTree"); }

	/**
	 * Loads Path synchronously and checks its class against RequiredClassPath. A StateTree key additionally needs the
	 * crowd schema (the schema check is added by story cnb-012 through SetStateTreeSchemaCheck).
	 */
	STEALTHGAMEPLAY_API ECrowdAssetCheck CheckAsset(const FSoftObjectPath& Path, const TCHAR* RequiredClassPath, bool bStateTree);

	/** A predicate the StateTree layer registers: true when a loaded UStateTree is on the crowd schema. */
	using FStateTreeSchemaCheck = bool (*)(const UObject* StateTree);
	STEALTHGAMEPLAY_API void SetStateTreeSchemaCheck(FStateTreeSchemaCheck Check);

	/**
	 * Validates the three keys (the OnTuningReady check). False when any key is Missing, WrongClass or WrongSchema;
	 * OutProblems (optional) receives one line per failed key.
	 */
	STEALTHGAMEPLAY_API bool ValidateAssetKeys(const FCrowdAssetKeys& Keys, TArray<FString>* OutProblems = nullptr);

	/**
	 * The per-round tuning hash with the Crowd soft references: F-2 over every schema key of Values plus the three asset
	 * keys as SoftRef entries (FTuningHasher::HashEntries), so a changed Crowd rate or asset changes the hash.
	 */
	STEALTHGAMEPLAY_API uint64 ComputeRoundTuningHash(const FStealthServerTuning& Values, const FCrowdAssetKeys& Keys, int32 SchemaVersion);
}
