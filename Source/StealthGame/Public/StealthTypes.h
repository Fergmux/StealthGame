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
