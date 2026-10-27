#pragma once

#include "CoreMinimal.h"
#include "StealthTypes.h"

/**
 * Pure, engine-independent PROTOTYPE game rules still used until their epics retire them (NPC-kill removal: Penalty;
 * objective stages and the elimination winner: Objectives and Round; team choice and the run threshold: Session and
 * SCL), unit-tested without a world in Private/Tests/StealthRulesTests.cpp. The assassination and defence rules were
 * retired by story asn-027: they are re-expressed on Assassination's production cores (FAttackResolverCore,
 * FDefenceStateCore) and their reference tests ported to StealthGame.Assassination.PrototypeParity.
 * Source of the rules: GAME_BUILD_HANDOFF.md "Confirmed game rules" and the user's decisions recorded in
 * production/session-state/active.md (2026-10-24).
 */
struct STEALTHGAME_API FStealthRules
{
	/** True when this NPC-kill count removes the player (removal ON the limit-th kill). */
	static bool ShouldRemoveForNpcKills(int32 NpcKillCount, int32 NpcKillLimit);

	/** Whether a team at CurrentStage may complete an objective of ObjectiveStage owned by ObjectiveTeam. */
	static bool CanCompleteObjective(uint8 Team, int32 CurrentStage, uint8 ObjectiveTeam, int32 ObjectiveStage);

	/**
	 * Evaluates the elimination win. Returns the winning team, or STEALTH_NO_TEAM if none.
	 * A team only loses by elimination if it has at least one player and none of them is active,
	 * while the other team still has an active player. Single-team test sessions never end by elimination.
	 */
	static uint8 EvaluateEliminationWinner(const int32 PlayersPerTeam[STEALTH_NUM_TEAMS], const int32 ActivePerTeam[STEALTH_NUM_TEAMS]);

	/** Index of the team that a newly joining player should be placed on (fewest players first). */
	static uint8 ChooseTeamForNewPlayer(const int32 PlayersPerTeam[STEALTH_NUM_TEAMS], int32 MaxPlayersPerTeam);

	/** True if a speed (cm/s, horizontal) counts as running for the defence and informant rules. */
	static bool IsRunningSpeed(float HorizontalSpeed, float RunSpeedThreshold);
};
