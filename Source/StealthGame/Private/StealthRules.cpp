#include "StealthRules.h"

EStealthAssassinationOutcome FStealthRules::ResolveAssassination(bool bAttackerIsActive, EStealthTargetKind TargetKind, bool bTargetPrepared)
{
	if (!bAttackerIsActive)
	{
		return EStealthAssassinationOutcome::Rejected;
	}

	switch (TargetKind)
	{
	case EStealthTargetKind::Npc:
		return EStealthAssassinationOutcome::NpcKilled;
	case EStealthTargetKind::ActiveEnemy:
		return bTargetPrepared ? EStealthAssassinationOutcome::Reversed : EStealthAssassinationOutcome::EnemyEliminated;
	case EStealthTargetKind::InformantEnemy:
		return EStealthAssassinationOutcome::InformantKilled;
	case EStealthTargetKind::Teammate:
	default:
		return EStealthAssassinationOutcome::Rejected;
	}
}

bool FStealthRules::ShouldRemoveForNpcKills(int32 NpcKillCount, int32 NpcKillLimit)
{
	return NpcKillLimit > 0 && NpcKillCount >= NpcKillLimit;
}

bool FStealthRules::CanStartDefence(bool bIsActive, bool bIsRunning, bool bIsInTraversal, double Now, double LastActivationTime, float Cooldown)
{
	if (!bIsActive || bIsRunning || bIsInTraversal)
	{
		return false;
	}
	return DefenceCooldownRemaining(Now, LastActivationTime, Cooldown) <= 0.f;
}

float FStealthRules::DefenceCooldownRemaining(double Now, double LastActivationTime, float Cooldown)
{
	if (LastActivationTime < 0.0)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(LastActivationTime + Cooldown - Now));
}

bool FStealthRules::ShouldEndDefence(bool bIsRunning, bool bIsInTraversal, double Now, double ActivationTime, float Duration)
{
	return bIsRunning || bIsInTraversal || Now >= ActivationTime + Duration;
}

bool FStealthRules::CanCompleteObjective(uint8 Team, int32 CurrentStage, uint8 ObjectiveTeam, int32 ObjectiveStage)
{
	if (Team >= STEALTH_NUM_TEAMS || ObjectiveStage != CurrentStage)
	{
		return false;
	}
	if (ObjectiveStage == STEALTH_FINAL_STAGE)
	{
		// The final objective is shared by both teams.
		return ObjectiveTeam == STEALTH_NO_TEAM || ObjectiveTeam == Team;
	}
	// Preliminary objectives belong to one team only.
	return ObjectiveTeam == Team;
}

uint8 FStealthRules::EvaluateEliminationWinner(const int32 PlayersPerTeam[STEALTH_NUM_TEAMS], const int32 ActivePerTeam[STEALTH_NUM_TEAMS])
{
	for (int32 Loser = 0; Loser < STEALTH_NUM_TEAMS; ++Loser)
	{
		const int32 Other = 1 - Loser;
		const bool bLoserWipedOut = PlayersPerTeam[Loser] > 0 && ActivePerTeam[Loser] == 0;
		const bool bOtherAlive = ActivePerTeam[Other] > 0;
		if (bLoserWipedOut && bOtherAlive)
		{
			return static_cast<uint8>(Other);
		}
	}
	return STEALTH_NO_TEAM;
}

uint8 FStealthRules::ChooseTeamForNewPlayer(const int32 PlayersPerTeam[STEALTH_NUM_TEAMS], int32 MaxPlayersPerTeam)
{
	const uint8 Smaller = PlayersPerTeam[1] < PlayersPerTeam[0] ? 1 : 0;
	if (PlayersPerTeam[Smaller] >= MaxPlayersPerTeam)
	{
		return STEALTH_NO_TEAM;
	}
	return Smaller;
}

bool FStealthRules::IsRunningSpeed(float HorizontalSpeed, float RunSpeedThreshold)
{
	return HorizontalSpeed > RunSpeedThreshold;
}
