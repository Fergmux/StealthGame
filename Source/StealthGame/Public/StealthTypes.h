#pragma once

#include "CoreMinimal.h"
#include "StealthTypes.generated.h"

/** Team index meaning "no team" (NPCs, unassigned players, shared objectives). */
constexpr uint8 STEALTH_NO_TEAM = 255;

/** Number of competing teams. Fixed by the confirmed rules (two teams of five). */
constexpr int32 STEALTH_NUM_TEAMS = 2;

/** Objective stage index of the shared final objective. Stages 1 and 2 are preliminary. */
constexpr int32 STEALTH_FINAL_STAGE = 3;

/** Whether a player is still competing or has been removed to the informant role. */
UENUM(BlueprintType)
enum class EStealthLifeState : uint8
{
	Active,
	Informant
};

/** Lifecycle of one round. */
UENUM(BlueprintType)
enum class EStealthRoundPhase : uint8
{
	WaitingToStart,
	InProgress,
	RoundOver
};

/** Why a round ended. */
UENUM(BlueprintType)
enum class EStealthWinReason : uint8
{
	None,
	FinalObjective,
	Elimination
};

/** What kind of character an assassination targeted, as the server sees it. */
UENUM(BlueprintType)
enum class EStealthTargetKind : uint8
{
	Npc,
	ActiveEnemy,
	InformantEnemy,
	Teammate
};

/** Server-resolved result of an assassination attempt. */
UENUM(BlueprintType)
enum class EStealthAssassinationOutcome : uint8
{
	/** The attempt was not allowed (teammate, informant attacker, round not running...). */
	Rejected,
	/** An active enemy player was eliminated and becomes an informant. */
	EnemyEliminated,
	/** The defender was prepared: the attacker is eliminated instead. */
	Reversed,
	/** A civilian was killed; the attacker's NPC-kill count increases. */
	NpcKilled,
	/** An enemy informant was killed; they respawn at their team spawn. Not an NPC kill. */
	InformantKilled
};
