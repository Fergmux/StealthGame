#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StealthTypes.h"
#include "Server/Roster/StealthRosterCallTypes.h"
#include "View/StealthRoundPublicState.h"
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
	/** Registers the new GameState with Round's public-state mirror (re-seeded before its first net update, R20). */
	virtual void InitGameState() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** No-op (ADR-0005 / ADR-0010): pawns come only from UStealthPawnFactory at round start. */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;

	// ---- Round lifecycle (driven by the production URoundMatchStore, decision D53) -------------

	/** Asks Round to start (stealth.round.start as the host). Refusals reach the host's HUD. */
	void RequestRoundStart();

	/** Harness/dev entry kept for the spike scenarios: the same as RequestRoundStart. */
	void StartRound() { RequestRoundStart(); }

	/** Asks Round to reset after the end-of-round hold (stealth.round.reset as the host). */
	void RequestRoundReset();

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

	/** Round's phase listener: Preparing runs the round setup, Reset cleans up, Lobby schedules the next start. */
	void HandleRoundPhaseChanged(ERoundPhase OldPhase, ERoundPhase NewPhase);

	/** Preparing (adapter until the Preparing barrier stories): city, objectives, player pawns through the factory, then Active. */
	void RunPreparing();

	/** Reset entered from Preparing or Active (an abort): clean up and tell everyone. */
	void HandleRoundCalledOff();

	/** Spawns Player's pawn at its team start through UStealthPawnFactory (bound to its participant). */
	APawn* SpawnPlayerPawn(APlayerController* Player);
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

	/**
	 * Kills the player's pawn and makes them an informant who respawns at the team spawn. Reason is the Roster
	 * transition (Assassinated, ReversedAgainst); None for an informant killed again (not a transition, D10).
	 */
	void EliminatePlayer(AStealthCharacter* Victim, ERosterReason Reason);

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
	FDelegateHandle RoundPhaseHandle;
	FTimerHandle RoundRestartTimer;
	FTimerHandle CrowdSpawnTimer;
	double NavWaitStartTime = 0.0;
	TArray<FTimerHandle> PendingTimers;
};
