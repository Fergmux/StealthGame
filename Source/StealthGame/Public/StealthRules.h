#pragma once

#include "CoreMinimal.h"
#include "StealthTypes.h"

/**
 * Pure, engine-independent game rules. Every server decision about assassination,
 * defence, NPC-kill removal, objective stages and victory goes through these functions,
 * so they can be unit-tested without a world (see Private/Tests/StealthRulesTests.cpp).
 * Source of the rules: GAME_BUILD_HANDOFF.md "Confirmed game rules" and the user's
 * decisions recorded in production/session-state/active.md (2026-10-24).
 */
struct STEALTHGAME_API FStealthRules
{
	/**
	 * Resolves an assassination attempt.
	 * @param bAttackerIsActive  Attacker is an active (non-informant) player.
	 * @param TargetKind         What the target is.
	 * @param bTargetPrepared    Target is an active player holding the defensive preparation.
	 */
	static EStealthAssassinationOutcome ResolveAssassination(bool bAttackerIsActive, EStealthTargetKind TargetKind, bool bTargetPrepared);

	/** True when this NPC-kill count removes the player (removal ON the limit-th kill). */
	static bool ShouldRemoveForNpcKills(int32 NpcKillCount, int32 NpcKillLimit);

	/**
	 * Whether a player may start the defensive preparation now.
	 * The cooldown is measured from the previous activation, including unused attempts.
	 * @param LastActivationTime  Server time of the previous activation; negative if never used.
	 */
	static bool CanStartDefence(bool bIsActive, bool bIsRunning, bool bIsInTraversal, double Now, double LastActivationTime, float Cooldown);

	/** Seconds until the defence can be used again (0 when ready). */
	static float DefenceCooldownRemaining(double Now, double LastActivationTime, float Cooldown);

	/** Whether an active defence must stop this frame. */
	static bool ShouldEndDefence(bool bIsRunning, bool bIsInTraversal, double Now, double ActivationTime, float Duration);

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
