#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StealthTypes.h"
#include "StealthGameMode.generated.h"

class AStealthCharacter;
class AStealthGameState;
class AStealthObjective;
class AStealthPlayerController;
class AStealthPlayerState;

/**
 * Server-only authority for the round: team assignment, spawns, the NPC crowd, assassination and
 * defence resolution, NPC-kill removal, informants, objectives and both victory conditions.
 * All rule decisions are delegated to FStealthRules.
 *
 * Team spawns: place APlayerStart actors with PlayerStartTag "Team0" / "Team1".
 * Crowd area: an actor tagged "CrowdCenter" (falls back to the world origin) and UStealthSettings::CrowdRadius.
 */
UCLASS()
class STEALTHGAME_API AStealthGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AStealthGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;

	// ---- Round lifecycle ---------------------------------------------------------------

	/** Starts (or restarts) a round now: resets state, respawns players, spawns the crowd. */
	void StartRound();

	/** Ends the current round without a winner and returns to waiting. */
	void AbortRound();

	/** Moves a player to the other team and respawns them. */
	void SwitchTeam(AStealthPlayerController* Player);

	// ---- Requests from characters (already on the server) -------------------------------

	void HandleAssassinationRequest(AStealthCharacter* Attacker, AStealthCharacter* Target, const FVector& StrikePoint);
	void HandleDefenceRequest(AStealthCharacter* Defender);
	void HandleInteractRequest(AStealthCharacter* Character, bool bWantsInteract);
	void HandlePing(AStealthCharacter* Sender, const FVector& Location);

	/** Called by an objective when Completer finished interacting with it. */
	void OnObjectiveCompleted(AStealthObjective* Objective, AStealthCharacter* Completer);

	bool IsRoundInProgress() const;

	/** Development test driver behind AStealthPlayerController::StealthBot (no-op in Shipping). */
	void DebugBotAction(int32 PlayerIndex, const FString& Action);

protected:
	AStealthGameState* GetStealthGameState() const;

	void TryScheduleRoundStart();
	void EndRound(uint8 WinningTeam, EStealthWinReason Reason);
	void CleanupRoundActors();
	void SpawnCrowd();

	/** Polls until the nav mesh has rebuilt for the new layout, then validates it and spawns the crowd. */
	void SpawnCrowdWhenNavReady();

	/**
	 * Next appearance from a shuffled deck holding every pool entry equally often, shared by players
	 * and NPCs. Keeps model counts balanced so no appearance is statistically more likely to be a
	 * player (Pillar 1). Returns INDEX_NONE when the pool is empty.
	 */
	int32 DrawAppearance();

	/** Remaining appearances in the current deck; refilled and reshuffled when empty and each round. */
	TArray<int32> AppearanceDeck;
	AStealthCharacter* SpawnNpc();
	void ScheduleNpcReplacement();
	bool FindCrowdPoint(FVector& OutLocation) const;
	FVector GetCrowdCenter() const;

	/** Kills the player's pawn and makes them an informant who respawns at the team spawn. */
	void EliminatePlayer(AStealthCharacter* Victim);

	/** Removes a player from active play without killing their pawn (NPC-kill penalty). */
	void ExilePlayer(AStealthCharacter* Offender);

	void RespawnInformant(TWeakObjectPtr<AController> Controller);
	void CheckEliminationVictory();

	void MessagePlayer(const AStealthCharacter* Character, const FText& Text, bool bDanger = false, float Duration = 4.f) const;
	void MessageController(AController* Controller, const FText& Text, bool bDanger = false, float Duration = 4.f) const;
	void MessageTeam(uint8 Team, const FText& Text, bool bDanger = false, float Duration = 4.f) const;
	void MessageAll(const FText& Text, bool bDanger = false, float Duration = 5.f) const;
	FText GetTeamName(uint8 Team) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AStealthCharacter>> CrowdNpcs;

	UPROPERTY(Transient)
	TSubclassOf<AStealthCharacter> CharacterClass;

	FTimerHandle RoundStartTimer;
	FTimerHandle RoundRestartTimer;
	FTimerHandle CrowdSpawnTimer;
	double NavWaitStartTime = 0.0;
	TArray<FTimerHandle> PendingTimers;
};
