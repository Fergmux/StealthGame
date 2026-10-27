#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Server/ActionHub.h"
#include "Server/AttackResolverCore.h"
#include "Server/Roster/StealthRosterStore.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "EncounterService.generated.h"

class AController;
class FResolutionQueueCore;
class IBushSpotQuery;
class IStealthFaultSink;
class IRoundLogWriter;
class IServerSubSeedSource;
class IStealthClock;
class UActionHub;
class URosterStore;
class UWorld;

/**
 * Production seam adapters the service binds (ADR-0007 thin facade; each one adapts one engine owner and adds no rule).
 * Declared here so a test or the composition root can reuse one alone.
 */

/** IEncounterQueue over the round's FResolutionQueueCore (URoundMatchStore::GetRoundQueue()). */
class STEALTHGAMEPLAY_API FEncounterQueueAdapter final : public IEncounterQueue
{
public:
	explicit FEncounterQueueAdapter(FResolutionQueueCore* InQueue = nullptr) : Queue(InQueue) {}
	void SetQueue(FResolutionQueueCore* InQueue) { Queue = InQueue; }

	virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) override;
	virtual void SetVoidCode(uint32 OwnerCode) override;
	virtual bool StageWrite(const FStagedWrite& Write) override;

private:
	FResolutionQueueCore* Queue = nullptr;
};

/** IEncounterDefenceStore over UDefenceStateComponent (server only; created on the first defence write). */
class STEALTHGAMEPLAY_API FEncounterComponentDefenceStore final : public IEncounterDefenceStore
{
public:
	explicit FEncounterComponentDefenceStore(UWorld* InWorld = nullptr) : World(InWorld) {}

	virtual FDefenceStateCore* Find(const APawn* Pawn) override;
	virtual FDefenceStateCore* FindOrAdd(const APawn* Pawn) override;
	/** Every UDefenceStateComponent of the world's pawns back to none (Round Reset, story asn-019). */
	virtual void ResetAll() override;

private:
	TWeakObjectPtr<UWorld> World;
};

/**
 * IEncounterReplySink over the world's UActionHub reply-sink table (ADR-0011 section 8; story asn-015): appended inside the
 * slot, flushed once per connection at drain end by the composition root's reply step (URoundMatchStore::OnRoundQueueDrained).
 */
class STEALTHGAMEPLAY_API FEncounterHubReplySink final : public IEncounterReplySink
{
public:
	explicit FEncounterHubReplySink(UActionHub* InHub = nullptr) : Hub(InHub) {}

	virtual void AppendReply(FReplySinkId Sink, FQueueKey SlotKey, const FCommandReply& Reply) override;

private:
	TWeakObjectPtr<UActionHub> Hub;
};

/**
 * IEncounterOwnerCooldown over UDefenceOwnerComponent on the pawn's controller (ADR-0018 section 1.5; story asn-016):
 * a pawn driven by a controller without the component (AI) publishes nothing; ResetAll clears every controller's value.
 */
class STEALTHGAMEPLAY_API FEncounterOwnerCooldownPublisher final : public IEncounterOwnerCooldown
{
public:
	explicit FEncounterOwnerCooldownPublisher(UWorld* InWorld = nullptr) : World(InWorld) {}

	virtual void PublishCooldownEnd(APawn* Pawn, double CooldownEndServerTime) override;
	virtual void ResetAll() override;

private:
	TWeakObjectPtr<UWorld> World;
};

/** BodyReleased(Body, FormerController): the controller that held the body until its release (story asn-019). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEncounterBodyReleased, APawn* /*Body*/, AController* /*FormerController*/);

/**
 * IEncounterBodyHandOver over UStealthPawnFactory (ADR-0010 ReleaseBody; story asn-019). Release: the body's capsule stops
 * blocking pawn channels, then the factory lets its controller go (crowd controller destroyed, PlayerController left
 * pawnless, never Spectating) and OnReleased fires with the former controller (the informant respawn takeover and
 * every ownership change follow it). Teardown: released first if still possessed, then destroyed. Any other actor: the
 * same calls through the engine (a world without a factory, e.g. a bare test world).
 */
class STEALTHGAMEPLAY_API FEncounterPawnFactoryHandOver final : public IEncounterBodyHandOver
{
public:
	virtual void ReleaseBody(APawn* Body) override;
	virtual void RequestTeardown(APawn* Body) override;

	FOnEncounterBodyReleased OnReleased;
};

/**
 * IEncounterPawnQuery over the world's AStealthPawns: capsule location, actor yaw, velocity, the SCL baseline braking of
 * the current gait, IsRunning / in traversal. SpawnSerial stand-in: the pawn's UObject unique id (allocation order) until
 * SCL / the pawn factory (ADR-0010) publishes a spawn serial; the tie rule needs only a value every pawn has.
 */
class STEALTHGAMEPLAY_API FEncounterWorldPawnQuery final : public IEncounterPawnQuery
{
public:
	explicit FEncounterWorldPawnQuery(UWorld* InWorld = nullptr) : World(InWorld) {}
	void SetWorld(UWorld* InWorld) { World = InWorld; }

	virtual void GetPawns(TArray<FEncounterPawnFacts>& OutPawns) const override;
	virtual bool GetFacts(const APawn* Pawn, FEncounterPawnFacts& OutFacts) const override;

	/** The facts of one live AStealthPawn (false for any other pawn or a pending-kill one). */
	static bool MakeFacts(const APawn* Pawn, FEncounterPawnFacts& OutFacts);

private:
	TWeakObjectPtr<UWorld> World;
};

/**
 * IEncounterRosterGateway over URosterStore. With no store (a bare test world) every pawn is Unbound: Npc, legality Active,
 * no teammates, no transition (the Roster's own answers for an Unbound pawn, R10).
 */
class STEALTHGAMEPLAY_API FEncounterRosterStoreGateway final : public IEncounterRosterGateway
{
public:
	explicit FEncounterRosterStoreGateway(URosterStore* InStore = nullptr) : Store(InStore) {}
	void SetStore(URosterStore* InStore) { Store = InStore; }

	virtual FRosterVictimKind ResolveVictimKind(const APawn* Pawn) override;
	virtual bool AreTeammates(const APawn* A, const APawn* B) override;
	virtual bool IsLegalityActive(const APawn* Pawn) override;
	virtual void RequestElimination(const APawn* Pawn, ERosterReason Reason, double RetireAtServerTime) override;
	virtual FParticipantId GetParticipant(const APawn* Pawn) override;
	virtual uint32 GetTeamIndex(const APawn* Pawn) override;

private:
	TWeakObjectPtr<URosterStore> Store;
};

/** IEncounterActionEvents over UActionHub's OnActionStarted / Cancelled / Completed (AStealthPawn only). */
class STEALTHGAMEPLAY_API FEncounterHubEvents final : public IEncounterActionEvents
{
public:
	explicit FEncounterHubEvents(UActionHub* InHub = nullptr) : Hub(InHub) {}
	void SetHub(UActionHub* InHub) { Hub = InHub; }

	virtual void OnActionStarted(APawn* Pawn, EActionType Type) override;
	virtual void OnActionCancelled(APawn* Pawn, EActionType Type, ECancelReason Reason) override;
	virtual void OnActionCompleted(APawn* Pawn, EActionType Type) override;

private:
	TWeakObjectPtr<UActionHub> Hub;
};

/**
 * IEncounterPawnStateWriter over the Shared Pawn (stories asn-012, asn-013): FActionState through the pawn's
 * UPawnCommandComponent::WriteActionState (the only writer; it assigns Seq, forces the update and sets the SCL pose, so
 * Brace plants the pawn through FActionState, SCL BracePlanted), and the death write's pawn half through
 * AStealthPawn::ApplyDeathWrite. Any other pawn class: nothing is written.
 */
class STEALTHGAMEPLAY_API FEncounterPawnStateWriter final : public IEncounterPawnStateWriter
{
public:
	virtual void WriteActionState(APawn* Pawn, const FActionStateWrite& Write) override;
	virtual EActionPose GetPose(const APawn* Pawn) const override;
	virtual bool WriteDeath(APawn* Pawn, const FDeathState& State) override;
};

/**
 * CanViewerKnow stand-in until the Visibility Policy (IVP story 010): a pawn is knowable to the attacker unless the bush
 * query says it is concealed (the prototype's per-viewer hide). Null bush query: every pawn is knowable.
 */
class STEALTHGAMEPLAY_API FEncounterConcealmentVisibility final : public IEncounterVisibilityQuery
{
public:
	explicit FEncounterConcealmentVisibility(const IBushSpotQuery* InBushes = nullptr) : Bushes(InBushes) {}
	void SetBushes(const IBushSpotQuery* InBushes) { Bushes = InBushes; }

	virtual bool CanViewerKnow(const APawn* Viewer, const APawn* Pawn) override;

private:
	const IBushSpotQuery* Bushes = nullptr;
};

/** What the composition root hands the service (World, Clock and Queue required; the rest optional). */
struct FEncounterServiceInit
{
	UWorld* World = nullptr;
	const IStealthClock* Clock = nullptr;
	/** The round queue (URoundMatchStore::GetRoundQueue()). */
	FResolutionQueueCore* Queue = nullptr;
	/** The session's server sub-seeds (rule 2's BushSpotPick). */
	const IServerSubSeedSource* SubSeeds = nullptr;
	/** The frozen tuning (default: the schema defaults, the stand-in until the TDC store feeds features). */
	FEncounterParams Params = FEncounterParams::Defaults();
	/** Round's Event Log writer (FRoundMatchCore::GetRoundLogWriter()). */
	IRoundLogWriter* Log = nullptr;
	/** Concealment (or its prototype stand-in in the composition root); null: no zones, nobody concealed. */
	IBushSpotQuery* Bushes = nullptr;
	/** Overrides the CanViewerKnow stand-in (the Visibility Policy when it exists). */
	IEncounterVisibilityQuery* Visibility = nullptr;
	IEncounterCardLedger* Cards = nullptr;
	/** Overrides the world's Roster store (null: URosterStore::Get(World)). */
	URosterStore* Roster = nullptr;
	/** Overrides the world's action hub (null: UActionHub::Get(World)). */
	UActionHub* Hub = nullptr;
	/** Penalty's tally (story asn-015; the composition root's stand-in until the Penalty epic). Null: a fault per Attack. */
	IStrikeTallySource* Tally = nullptr;
	/** Concealment's body removal observer (story asn-019; a stand-in until Concealment). Null: a fault per body. */
	IBodyRemovalObserver* BodyObserver = nullptr;
	/** The fault sink of the missing-seam reports (null: none reported). */
	IStealthFaultSink* Faults = nullptr;
};

/**
 * UEncounterService: the Assassination feature's server-only facade (ADR-0007 thin facade over FAttackResolverCore; ADR-0018
 * section 2.1; stories asn-005 to asn-011). It owns the core, the strike-role table inside it and the production adapters,
 * exposes OnPawnKilled and the resolver to register, and adds no rule of its own. It holds no integer or float state
 * (Assassination keeps no strike counter, ASN-R11: StealthGame.Assassination.KillEventMatrix reflects it).
 *
 * Registration seam (the integration stories asn-012 / 013 wire it from UStealthCompositionRoot):
 *   UEncounterService* Encounters = NewObject<UEncounterService>(Root);
 *   FEncounterServiceInit Init; Init.World = World; Init.Clock = Clock; Init.Queue = Store->GetRoundQueue();
 *   Init.SubSeeds = SessionSeeds; Init.Log = MatchCore.GetRoundLogWriter(); Init.Bushes = PrototypeBushQuery;
 *   Encounters->Initialize(Init);
 *   Encounters->SetFallbackResolvers(PrototypeInteractHideDispatch, ObjectivesHoldResolver);   // stand-in dispatch
 *   Encounters->RegisterOn(*Store->GetRoundQueue());   // PawnCommand + HoldTimer resolvers, DeathWrite + CounterPose appliers
 *   ... the pawns' command cores submit PawnCommand items to that queue (IPawnCommandQueue adapter); the composition
 *       root calls Release() when Round's EndMatch destroys the queue (StealthCompositionRoot.cpp, stories asn-012/013).
 */
UCLASS(NotBlueprintable, NotBlueprintType)
class STEALTHGAMEPLAY_API UEncounterService : public UObject
{
	GENERATED_BODY()

public:
	/** Builds the adapters and the core. Returns false (nothing built) without a world, clock or queue. */
	bool Initialize(const FEncounterServiceInit& Init);
	/** Unregisters from the queue it registered on and releases the core. */
	void Shutdown();
	bool IsInitialized() const { return Core.IsValid(); }

	/**
	 * Registers the core for EQueueItemType::PawnCommand and HoldTimer, for TimerFired(BodyRelease, BodyRemoval) (story
	 * asn-019) and its applier for the DeathWrite and CounterPose staged writes (story asn-013); false when any of them
	 * already has an owner.
	 */
	bool RegisterOn(FResolutionQueueCore& Queue);
	void UnregisterFrom(FResolutionQueueCore& Queue);

	/**
	 * The queue (and the match core whose log writer the service holds) was destroyed (Round's EndMatch): drops the
	 * registration record without touching the queue and releases the core. Initialize again for the next match.
	 */
	void Release();

	/** The queue this service is registered on (null: none). */
	FResolutionQueueCore* GetRegisteredQueue() const { return RegisteredQueue; }

	/** The stand-in dispatch for Interact / Hide / Traverse and the non-Prepare holds (until IIF stage 2). */
	void SetFallbackResolvers(IQueueResolver* Commands, IQueueResolver* Holds);

	/** The core (valid between Initialize and Shutdown). */
	FAttackResolverCore* GetCore() const { return Core.Get(); }
	/** The resolver to register by hand (the core itself). */
	IQueueResolver* GetResolver() const { return Core.Get(); }

	/** OnPawnKilled(Victim, Killer, VictimKind, Cause), fired inline in the slot (null before Initialize). */
	FOnPawnKilled* GetOnPawnKilled() const { return Core.IsValid() ? &Core->OnPawnKilled : nullptr; }
	/** OnDeathWriteApplied(Victim, DeathServerTime, bDiedConcealed), fired by the staging pass (null before Initialize). */
	FOnDeathWriteApplied* GetOnDeathWriteApplied() const { return Core.IsValid() ? &Core->OnDeathWriteApplied : nullptr; }
	/** OnCounterPoseApplied(Defender, ContactServerTime) (null before Initialize). */
	FOnCounterPoseApplied* GetOnCounterPoseApplied() const { return Core.IsValid() ? &Core->OnCounterPoseApplied : nullptr; }
	bool IsDead(const APawn* Pawn) const { return Core.IsValid() && Core->IsDead(Pawn); }
	TOptional<double> GetScheduledDeathServerTime(const APawn* Pawn) const { return Core.IsValid() ? Core->GetScheduledDeathServerTime(Pawn) : TOptional<double>(); }

	/**
	 * Round Reset (story asn-019, called from Round's Reset step by the composition root): every strike role Ready, every
	 * defence component and owner cooldown none, pending writes forgotten, every body torn down, body items voided.
	 */
	void ResetRound();

	/** BodyReleased(Body, FormerController) (null before Initialize): the game mode's informant respawn waits for it. */
	FOnEncounterBodyReleased* GetOnBodyReleased() const { return HandOver.IsValid() ? &HandOver->OnReleased : nullptr; }

	/** Unregisters before the object goes (the queue holds raw resolver pointers). */
	virtual void BeginDestroy() override;

private:
	TUniquePtr<FEncounterQueueAdapter> QueueAdapter;
	TUniquePtr<FEncounterComponentDefenceStore> DefenceStore;
	TUniquePtr<FEncounterWorldPawnQuery> PawnQuery;
	TUniquePtr<FEncounterRosterStoreGateway> RosterGateway;
	TUniquePtr<FEncounterHubEvents> HubEvents;
	TUniquePtr<FEncounterPawnStateWriter> PawnStateWriter;
	TUniquePtr<FEncounterConcealmentVisibility> DefaultVisibility;
	TUniquePtr<FEncounterHubReplySink> ReplySink;
	TUniquePtr<FEncounterOwnerCooldownPublisher> OwnerCooldown;
	TUniquePtr<FEncounterPawnFactoryHandOver> HandOver;
	TUniquePtr<FAttackResolverCore> Core;
	FResolutionQueueCore* RegisteredQueue = nullptr;
	bool bRegisteredCommands = false;
	bool bRegisteredHolds = false;
	bool bRegisteredDeathWrite = false;
	bool bRegisteredCounterPose = false;
	bool bRegisteredBodyRelease = false;
	bool bRegisteredBodyRemoval = false;
};
