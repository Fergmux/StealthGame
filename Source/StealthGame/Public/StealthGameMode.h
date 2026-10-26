#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StealthTypes.h"
#include "Server/Core/Round/StealthRoundGateways.h"
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
	/** False (ADR-0005 / ADR-0010): the engine restart path never spawns a pawn. */
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
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

	/** True while Round's phase is Active and the round is not latched (the production rules decide the round). */
	bool IsRoundInProgress() const;

	// ---- Round adapters (the composition root's engine side until the LSC, Crowd and Session facades exist) ----

	/** Round's RequestLayout (R6 P3/P4): builds the prototype city for Seed; NotifyLayoutReady once the nav mesh rebuilt. */
	void BuildRoundLayout(int32 Seed);

	/** Round's P6 spawner: one start pawn through UStealthPawnFactory (an NPC, or a participant's bound pawn). */
	ERoundSpawnResult SpawnStartPawn(const FRoundSpawnEntry& Entry, const FEpochToken& ExpectedEpoch);

	/** The participant became Connected: its Ai-driven start pawn is replaced by a player-possessed one. */
	bool OpenSpawnGate(FParticipantId Participant);

	/** R10 (4) after a Preparing abort: every pawn of the round destroyed (players and crowd). */
	int32 DestroyRoundPawns();

	/** Development test driver behind AStealthPlayerController::StealthBot (no-op in Shipping). */
	void DebugBotAction(int32 PlayerIndex, const FString& Action);

protected:
	AStealthGameState* GetStealthGameState() const;

	void TryScheduleRoundStart();

	/** Ended entered: the prototype end-of-round presentation from Round's FRoundResult (the production rules decided it). */
	void HandleRoundEnded();

	/** Active entered (R7): the prototype HUD state and the start message. */
	void HandleRoundActive();

	/** Round's phase listener: Preparing runs the round setup, Reset cleans up, Lobby schedules the next start. */
	void HandleRoundPhaseChanged(ERoundPhase OldPhase, ERoundPhase NewPhase);

	/** Preparing entered: clears the previous round's actors; Round's sequence then drives layout and spawn (R6). */
	void RunPreparing();

	/** Reset entered from Preparing or Active (an abort): clean up and tell everyone. */
	void HandleRoundCalledOff();

	/** Spawns Player's pawn at its team start through UStealthPawnFactory (bound to its participant). */
	APawn* SpawnPlayerPawn(APlayerController* Player);
	void CleanupRoundActors();
	void SpawnCrowd();

	/** Polls until the nav mesh has rebuilt for the new layout, then validates it and reports the layout ready to Round. */
	void PollLayoutNavReady();

	/** A team start for Team (free starts first). */
	AActor* FindTeamStart(uint8 Team) const;

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

	void MessagePlayer(const AStealthCharacter* Character, const FText& Text, bool bDanger = false, float Duration = 4.f) const;
	void MessageController(AController* Controller, const FText& Text, bool bDanger = false, float Duration = 4.f) const;
	void MessageTeam(uint8 Team, const FText& Text, bool bDanger = false, float Duration = 4.f) const;
	void MessageAll(const FText& Text, bool bDanger = false, float Duration = 5.f) const;
	FText GetTeamName(uint8 Team) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AStealthCharacter>> CrowdNpcs;

	/** The next start placement of the round's spawn list (a placement whose spawn collides is skipped, never retried). */
	int32 StartPlacementCursor = 0;

	UPROPERTY(Transient)
	TSubclassOf<AStealthCharacter> CharacterClass;

	FTimerHandle RoundStartTimer;
	FDelegateHandle RoundPhaseHandle;
	FTimerHandle RoundRestartTimer;
	FTimerHandle CrowdSpawnTimer;
	double NavWaitStartTime = 0.0;
	/** The seed of the layout being built (reported with NotifyLayoutReady). */
	int32 PendingLayoutSeed = 0;
	/** The start pawns of the round's participants (P6). */
	TMap<uint32, TWeakObjectPtr<APawn>> RoundPlayerPawns;
	TArray<FTimerHandle> PendingTimers;
};
