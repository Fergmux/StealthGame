#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "StealthTypes.h"
#include "StealthGameState.generated.h"

/** Round state replicated to every client. Contains no per-player identity information. */
UCLASS()
class STEALTHGAME_API AStealthGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AStealthGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	EStealthRoundPhase RoundPhase = EStealthRoundPhase::WaitingToStart;

	/** Current objective stage per team: 1, 2, then STEALTH_FINAL_STAGE. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	TArray<int32> TeamStage;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	uint8 WinningTeam = STEALTH_NO_TEAM;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	EStealthWinReason WinReason = EStealthWinReason::None;

	/** Server time at which the pending round start or restart happens (for the HUD countdown). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	double PhaseEndServerTime = 0.0;

	/** Seed of the current round; every machine builds the city layout from it. */
	UPROPERTY(ReplicatedUsing = OnRep_RoundSeed, BlueprintReadOnly, Category = "Stealth")
	int32 RoundSeed = 0;

	UFUNCTION(BlueprintPure, Category = "Stealth")
	int32 GetTeamStage(uint8 Team) const { return TeamStage.IsValidIndex(Team) ? TeamStage[Team] : 0; }

	/** Server: puts both teams back at stage 1 and clears the result. */
	void ResetRoundState(int32 NewSeed);

	/** Builds the city for RoundSeed on this machine (no-op without an AStealthCityGenerator). */
	void BuildCityForSeed() const;

protected:
	UFUNCTION()
	void OnRep_RoundSeed();
};
