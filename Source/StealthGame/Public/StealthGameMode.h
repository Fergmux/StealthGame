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
class AStealthPawn;
class UEncounterService;
enum class EVictimKind : uint8;
enum class EKillCause : uint8;
enum class EActionType : uint8;
class AStealthObjective;
class AStealthPlayerController;
class AStealthPlayerState;

/**
 * Server-only prototype authority for the round's presentation and stand-ins: team assignment, spawns, the NPC crowd,
 * informants, objectives and the end-of-round messages. Strikes, braces and reversals resolve only in Assassination's
 * queue slot (FAttackResolverCore); story asn-027 retired the prototype resolver. The remaining prototype rules are
 * FStealthRules'.
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

	/** The prototype objective handler (the composition root's Interact stand-in calls it inside the queue slot). */
	void HandleInteractRequest(AStealthCharacter* Character, bool bWantsInteract);

	// ---- Encounters (Assassination stories asn-012 / asn-013 play integration) ----------------------------------

	/**
	 * Binds this world's prototype presentation to Assassination's production slot (the composition root calls it once
	 * the service is registered on Round's queue). Strikes and braces resolve only there; this side keeps the prototype
	 * follow-ons: the life state and NPC replacement at the applied death write, the informant respawn after the body's
	 * release, the reveal of a strike from a bush, and the prototype NPC-kill exile (Penalty stand-in). The fall and the
	 * brace tell are presentation of FDeathState / FActionState (UEncounterPresentationSubsystem, AStealthCharacter).
	 */
	void BindEncounterService(UEncounterService& Service);

	/**
	 * STAND-IN (NPC-Kill Penalty stories nkp-009..015 not built; story nkp-021 retires it): the prototype follow-ons of the
	 * PRODUCTION exile (story nkp-005), called by the composition root's IPenaltyPawnGateway right after the factory's
	 * hand-over commit released Body from FormerController: the prototype life state Informant, the objective interaction
	 * dropped, and the prototype informant respawn (a fresh factory pawn after InformantRespawnDelay; the production
	 * respawn is Round's OnInformantAwaitingBody and the takeover of an unseen crowd pawn, stories nkp-009 and nkp-012).
	 */
	void HandleProductionExile(AStealthCharacter* Body, AController* FormerController);
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

	/** OnPawnKilled (in the slot, server state only): records the kill for its death write; the NPC-kill exile stand-in. */
	void HandleEncounterPawnKilled(APawn* Victim, APawn* Killer, EVictimKind Kind, EKillCause Cause);
	/** The applied death write: the prototype life state and the NPC replacement (the fall is the presentation's). */
	void HandleEncounterDeathWrite(APawn* Victim, double DeathServerTime, bool bDiedConcealed);
	/** The body's release (story asn-019, DeathServerTime + DeathFallSec): the informant respawn takeover follows it. */
	void HandleEncounterBodyReleased(APawn* Body, AController* FormerController);
	/** Hub OnActionStarted: an Attack reveals a striker concealed in a bush (the prototype concealment rule). */
	void HandleHubActionStarted(AStealthPawn* Pawn, EActionType Type);
	void UnbindEncounters();

	/**
	 * Removes a player from active play without killing their pawn (the PROTOTYPE NPC-kill penalty). Reached only when the
	 * production Penalty service is not bound (story nkp-005 made its exile live); story nkp-021 deletes it.
	 */
	void ExilePlayer(AStealthCharacter* Offender);

	/** Respawns an informant at its team spawn; the released body (still lying until its removal) drops its binding first. */
	void RespawnInformant(TWeakObjectPtr<AController> Controller, TWeakObjectPtr<APawn> ReleasedBody);

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

	/** A kill decided in a slot whose death write has not been applied yet (server only). */
	struct FPendingEncounterKill
	{
		TWeakObjectPtr<APawn> Killer;
		EVictimKind Kind;
		EKillCause Cause;
	};
	TMap<TWeakObjectPtr<APawn>, FPendingEncounterKill> PendingEncounterKills;
	TWeakObjectPtr<UEncounterService> BoundEncounters;
	FDelegateHandle PawnKilledHandle;
	FDelegateHandle DeathWriteHandle;
	FDelegateHandle BodyReleasedHandle;
	FDelegateHandle HubStartedHandle;
	/** StealthBot commands carry the controller's own sink so owner replies reach it; their seqs start here. */
	uint32 BotCommandSeq = 0x40000000u;
};
