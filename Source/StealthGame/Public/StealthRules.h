#pragma once

#include "CoreMinimal.h"
#include "StealthTypes.h"

/**
 * Pure, engine-independent PROTOTYPE game rules still used until their epics retire them (objective stages and the
 * elimination winner: Objectives and Round; team choice and the run threshold: Session and SCL), unit-tested without a
 * world in Private/Tests/StealthRulesTests.cpp. The assassination and defence rules were retired by story asn-027 (ported
 * to StealthGame.Assassination.PrototypeParity); the NPC-kill removal rule by story nkp-021 (the production FPenaltyCore
 * strike rule, StealthGame.Penalty.StrikeRuleMatrix / ExileOnLthNpcKill and StealthGame.Penalty.PIE.PrototypeParityExile).
 * Source of the rules: GAME_BUILD_HANDOFF.md "Confirmed game rules" and the user's decisions recorded in
 * production/session-state/active.md (2026-10-24).
 */
struct STEALTHGAME_API FStealthRules
{
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
