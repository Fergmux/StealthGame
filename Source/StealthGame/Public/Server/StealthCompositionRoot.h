#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthLogFaultSink.h"
#include "Server/Core/Round/StealthCrowdControl.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "View/CourtFixtureId.h"
#include "View/StealthRoundPublicState.h"

#include "StealthCompositionRoot.generated.h"

class APlayerController;
class AStealthGameMode;
class URosterStore;
class URoundMatchStore;
class URoundRosterStoreGateway;
struct FRosterEvent;

/**
 * The minimal adapters of the D53 integration batch (decision D53): the Round gateways whose producing systems do not
 * exist yet (Session & Travel, Tuning dry run, Character Pool, Level Socket Contract, Crowd), and the pawn factory's
 * hooks over the prototype (appearance on AStealthCharacter, the prototype NPC controller) and the real Roster.
 *
 * Session adapter: no travel happens yet, so RequestRoundTravel draws the round seed from USessionService (F5, over the
 * OS-CSPRNG session secret, stories st-009/010/014) and confirms the travel on the next tick; the connection state is Connected for a
 * participant with a live controller, else the Roster projection. Each adapter is replaced by its system's facade
 * when that epic lands.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAME_API UStealthMatchAdapters : public UObject, public IRoundSessionGateway, public IRoundTuningGateway,
	public IRoundCharacterPoolGateway, public IRoundLevelSocketGateway, public ICrowdControl, public IStealthPawnFactoryHooks,
	public IRoundStartSpawner, public IRoundPawnControlGateway, public IRoundObjectiveProgress
{
	GENERATED_BODY()

public:

	// IRoundSessionGateway
	virtual bool CanRequestRoundTravel() const override;
	virtual ERoundTravelRequestResult RequestRoundTravel(FName MapId, FName PresetId, uint32 RoundIndex) override;
	/** No map travel exists: the same world hosts the lobby, so every participant completes the lobby travel on the next tick. */
	virtual void RequestLobbyTravel() override;
	/** The travel is accepted on the next tick anyway (no map travel): the abort then runs at P1 (Round's Edge Cases rule). */
	virtual void CancelRoundTravel() override {}
	virtual void SetAdmissionMode(ERoundAdmissionMode Mode) override { AdmissionMode = Mode; }
	virtual ERoundConnectionState GetConnectionState(FParticipantId Participant) const override;
	virtual void KickParticipant(FParticipantId Participant) override;
	virtual uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const override;
	virtual uint32 GetConnectionGeneration(FParticipantId Participant) const override;

	// IRoundTuningGateway
	/** No TDC freeze exists yet: tuning is ready on the next tick, and (no travel) every connected player acknowledges then. */
	virtual void SignalPreparingEntered() override;
	virtual ERoundPresetPreview PreviewPreset(FName PresetId) const override { return ERoundPresetPreview::Unavailable; }

	// IRoundCharacterPoolGateway
	virtual ERoundAllocateResult AllocateAppearances(TConstArrayView<FParticipantId> Players, int32 NpcCount) override { return ERoundAllocateResult::Ok; }
	virtual uint16 GetAssignedAppearance(FParticipantId Participant) const override { return 0; }
	virtual void DiscardDeal() override {}

	// IRoundLevelSocketGateway
	/** The prototype city for Seed (AStealthGameMode::BuildRoundLayout), reported ready once the nav mesh has rebuilt. */
	virtual void RequestLayout(int32 Seed, uint64 TuningHash) override;
	virtual ERoundPlacementResult RequestStartPlacement(TConstArrayView<FParticipantId> Team0, TConstArrayView<FParticipantId> Team1, int32 NpcCount) override
	{
		return ERoundPlacementResult::Ok;
	}
	/** The prototype layout has no zones: the share check has nothing to assert. */
	virtual void GetPlacementZones(TArray<FRoundPlacementZone>& OutZones) const override { OutZones.Reset(); }

	// IRoundStartSpawner (over UStealthPawnFactory through the prototype game mode)
	virtual ERoundSpawnResult SpawnStartPawn(const FRoundSpawnEntry& Entry, const FEpochToken& ExpectedEpoch) override;
	/** No client world-ready report exists yet: every connected player reports world-ready on the next tick. */
	virtual void OnStartSpawnComplete(int32 ExpectedPawnCount) override;
	virtual bool OpenSpawnGate(FParticipantId Participant) override;
	virtual int32 DestroyRoundPawns() override;
	/** R9 (f): every living prototype character, classified by its controller and the round's player-pawn table. */
	virtual void GetLivePawns(TArray<FRoundLivePawn>& OutPawns) const override;

	// ICrowdControl
	/** R7: releases every prototype NPC controller held since its spawn in Preparing. */
	virtual void StartRound() override;
	/** R9 (f): the prototype crowd holds (every NPC controller stops issuing moves). */
	virtual void Halt() override;
	/** R9 (f): the prototype stop: the pawn stops its movement and is held in place (human and AI pawns alike). */
	virtual void RequestPawnStop(const FRoundLivePawn& Pawn, double BoundSec) override;
	virtual void DriveBoundPawn(AStealthPawn* Pawn) override {}
	virtual void ReleaseControl(AStealthPawn* Pawn) override {}
	virtual void AdoptReleasedPawn(AStealthPawn* Pawn, const FCrowdAdoptParams& Params) override {}

	// IStealthPawnFactoryHooks
	virtual void ApplyAppearance(AStealthPawn* Pawn, int32 AppearanceId) override;
	virtual ERosterResult BindPawn(FParticipantId Participant, AStealthPawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch) override;
	virtual AController* CreateAiController(AStealthPawn* Pawn) override;

	// IRoundPawnControlGateway (the prototype releases a leaver's pawn at logout: no bound pawn survives to take over)
	virtual bool RequestTakeover(FParticipantId Participant) override { return false; }
	virtual bool RequestHandBack(FParticipantId Participant) override { return false; }
	virtual FPawnHandle GetBoundPawn(FParticipantId Participant) const override;

	// IRoundObjectiveProgress (the prototype stage counter: stage 1, 2, then the final)
	virtual int32 GetCompletedPreliminaryStages(int32 Team) const override;

	/** Acknowledges for every connected player (travel complete, or the world-ready stage) with this Preparing's nonce. */
	void AcknowledgeConnectedPlayers(bool bWorldReady);

	/** The seed RequestRoundTravel drew for the round being started (0 before the first start). */
	int32 GetLastRoundSeed() const { return LastRoundSeed; }
	ERoundAdmissionMode GetAdmissionMode() const { return AdmissionMode; }

	/** The crowd area the prototype NPC controller wanders (set by the game mode). */
	FVector CrowdCenter = FVector::ZeroVector;
	float CrowdRadius = 3000.f;
	/** The graybox plaza radius the crowd types as Market (the city generator's PlazaRadius default; crowd story cnb-017). */
	float CrowdMarketRadius = 1300.f;

private:
	UWorld* GetGameWorld() const;

	/** Session's service (seeds and the round seed draw, stories st-009..014). */
	class USessionService* GetSession() const;
	int32 LastRoundSeed = 0;
	ERoundAdmissionMode AdmissionMode = ERoundAdmissionMode::Open;
};

/**
 * The composition root's game-instance part (ADR-0007 section 4; decision D53): builds the Round gateways (the real
 * URosterStore through URoundRosterStoreGateway, plus UStealthMatchAdapters), registers them with URoundMatchStore,
 * wires the pawn factory, and keeps Session's controller <-> participant map until the Session epic owns it.
 * Server-authoritative: a client game instance builds nothing.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAME_API UStealthCompositionRoot : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UStealthCompositionRoot* Get(const UWorld* World);

	/** Server world start (AStealthGameMode::InitGame): rule params, OpenMatch on first use, factory wiring. */
	void OnServerWorldStarting(AStealthGameMode* GameMode);

	/** PostLogin: registers the player's key with the Roster (Round assigns the team) and binds its C-TeamView. */
	FParticipantId RegisterPlayer(APlayerController* Player);
	/** Logout: Lobby removes the record; otherwise the projection becomes Disconnected. */
	void UnregisterPlayer(APlayerController* Player);

	/** Session's map. */
	FParticipantId GetParticipant(const APlayerController* Player) const;
	APlayerController* GetController(FParticipantId Participant) const;
	/** Every mapped (participant, controller) pair with a live controller. */
	void GetConnectedPlayers(TArray<TPair<FParticipantId, APlayerController*>>& Out) const;

	UStealthMatchAdapters* GetAdapters() const { return Adapters; }
	/** The current server world's prototype game mode (the adapters' engine side), or null. */
	AStealthGameMode* GetGameMode() const { return GameMode.Get(); }
	URosterStore* GetRosterStore() const;
	URoundMatchStore* GetRoundStore() const;

	/** A one-line production state summary (dev HUD line and StealthDumpProd). Server: full; client: empty. */
	FString DescribeProductionState(const UWorld* World) const;

	/** Assassination's server facade, registered on Round's queue for the current server world (stories asn-012/013). */
	class UEncounterService* GetEncounterService() const { return Encounters; }

	/** Faults reported to the composition root's command-path sink (the Objectives, Penalty and Encounter seams' sink). */
	int32 GetCommandFaultCount() const { return CommandFaults.GetReportCount(); }

	/** The Objectives service of the current server world (stories oc-005..oc-009), or null. */
	class UObjectivesService* GetObjectivesService() const { return Objectives.Get(); }

	/**
	 * Preparing (ADR-0020 section 2): Objectives selects and validates the one cooked chain. False: the round must fail
	 * Preparing with ObjectiveChainInvalid. True when no Objectives service is wired in this world.
	 */
	bool ValidateObjectivesChain() const;

	/** OnLayoutReady (the prototype layout's nav-ready step): Objectives binds the chain to the site assignment (story oc-005). */
	void HandleObjectivesLayoutReady(uint32 LayoutId, int32 Seed);

	/** The Interact hold stand-in of the current server world (IIF stage 2 replaces it; tests and dumps). */
	const class FInteractHoldStandIn* GetInteractHolds() const { return InteractHolds.Get(); }

#if !UE_BUILD_SHIPPING
	/** DEV ONLY (StealthBot): the live court of Participant's team, from the server store. Unset when none. */
	FCourtFixtureId FindObjectivesLiveCourtForDev(FParticipantId Participant) const;
	/** DEV ONLY (StealthBot, tests): the stand-in court actor of an id (L_Prototype's objective actors), or null. */
	const AActor* FindObjectivesCourtActorForDev(FCourtFixtureId Court) const;
#endif

private:
	void HandleRosterEvent(const FRosterEvent& Event);
	/** Builds the encounter service for World and registers it on Round's current queue (strikes resolve in its slot). */
	void WireEncounters(AStealthGameMode* InGameMode);
	/** Round's queue is about to be destroyed: the service forgets it without touching it. */
	void HandleRoundQueueReleasing();
	/** Round's Reset step (story asn-019): the encounter service clears its round (roles, stamps, writes, bodies). */
	void HandleRoundPhaseChanged(ERoundPhase OldPhase, ERoundPhase NewPhase);
	/** The reply step (story asn-015): after each drain, one batched ClientCommandReplies per connection. */
	void HandleRoundQueueDrained();
	void MirrorTeamsToPrototype();
	/** Builds and binds the Objectives service, its adapters and the Interact hold stand-in (stories oc-005..oc-009). */
	void WireObjectives(AStealthGameMode* InGameMode);
	/** Round's R7 Active-entry step: Objectives' HandleRoundActive (story oc-005). */
	void HandleRoundActive(uint32 RoundEpoch, double ActivePhaseStart);

	UPROPERTY(Transient)
	TObjectPtr<UStealthMatchAdapters> Adapters;

	UPROPERTY(Transient)
	TObjectPtr<URoundRosterStoreGateway> RosterGateway;

	/** Assassination's Prepare predicate (story asn-003), registered weakly on the server world's action hub. */
	UPROPERTY(Transient)
	TObjectPtr<class UAssassinationPreparePredicate> PreparePredicate;

	/** Assassination's Attack predicate (story asn-008: always Allow), registered weakly on the server world's action hub. */
	UPROPERTY(Transient)
	TObjectPtr<class UAssassinationAttackPredicate> AttackPredicate;

	TWeakObjectPtr<AStealthGameMode> GameMode;
	TMap<TWeakObjectPtr<APlayerController>, FParticipantId> ParticipantByController;

	/** Assassination's server facade (stories asn-005 to asn-013), wired per server world. */
	UPROPERTY(Transient)
	TObjectPtr<class UEncounterService> Encounters;

	/** Command-path adapters for the current server world (StealthCommandWiring.h). */
	TSharedPtr<class FStealthWorldCommandClock> CommandClock;
	TSharedPtr<class FStealthRoundCommandQueue> CommandQueue;
	TSharedPtr<class FStealthRoundEpochSource> CommandEpoch;
	TSharedPtr<class FStealthPrototypeBushSpotQuery> PrototypeBushes;
	TSharedPtr<class FStealthPrototypeCommandFallback> PrototypeFallback;
	/** Stand-ins of the Penalty tally and the Concealment removal observer (stories asn-015, asn-019). */
	TSharedPtr<class FStealthPrototypeBodyRemovalObserver> PrototypeBodyObserver;
	/** NPC-Kill Penalty (stories nkp-001..005): the production service of the current server world (the encounter
	 *  service's tally source and the exile), and its pawn-level gateway (StealthPenaltyWiring.h; nkp-021 retires the
	 *  prototype follow-ons the gateway still calls). */
	TWeakObjectPtr<class UPenaltyService> Penalty;
	TSharedPtr<class FStealthPenaltyPawnGateway> PenaltyPawns;
	/**
	 * NPC-Kill Penalty respawns (stories nkp-009..012): Round's IInformantBodyProvider (registered once with the Round
	 * gateways, forwarding to the current world's service), the commit's gateways (pick / takeover / card stand-ins), and
	 * the T_due carrier adapter with its Roster owner-view feed.
	 */
	TSharedPtr<class FStealthInformantBodiesProvider> InformantBodies;
	TSharedPtr<class FStealthPenaltyRespawnGateway> PenaltyRespawns;
	TSharedPtr<class FStealthRosterOwnerViewFeed> OwnerViewFeed;
	TSharedPtr<class FPenaltyDueAdapter> DueAdapter;
	/** Objectives Chain (stories oc-005..oc-009): the current server world's service, its adapters and the Interact hold stand-in. */
	TWeakObjectPtr<class UObjectivesService> Objectives;
	TSharedPtr<class FStealthObjectivesAdapters> ObjectivesAdapters;
	TSharedPtr<class FInteractHoldStandIn> InteractHolds;
	FDelegateHandle RoundActiveHandle;
	/** The reply step's transport for the current server world. */
	TSharedPtr<class FStealthControllerReplyTransport> ReplyTransport;
	TWeakObjectPtr<UWorld> ReplyWorld;
	FStealthLogFaultSink CommandFaults;
	FDelegateHandle RosterEventHandle;
	FDelegateHandle QueueReleasingHandle;
	FDelegateHandle RoundPhaseHandle;
	FDelegateHandle QueueDrainedHandle;
};
