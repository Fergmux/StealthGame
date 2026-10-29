#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/BodyRemovalObserver.h"
#include "Server/BushSpotQuery.h"
#include "Server/ConcealmentCore.h"
#include "Server/Core/CommandPredicate.h"
#include "Server/CrowdPerceptionFilter.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Subsystems/WorldSubsystem.h"

#include "ConcealmentService.generated.h"

class FResolutionQueueCore;
class IQueueEpochSource;
class UActionHub;
class URosterStore;
struct FRosterEvent;

/** What the composition root injects at Bind (every pointer outlives the binding). */
struct FConcealmentServiceInit
{
	const IStealthClock* Clock = nullptr;
	/** Round's queue: the TimerFired(ConcealmentTransition) resolver registers here; transition items are scheduled here. */
	FResolutionQueueCore* Queue = nullptr;
	/** The epoch stamped on items scheduled outside an item's own slot context (the Attack break). */
	const IQueueEpochSource* Epoch = nullptr;
	/** The hub: the Hide predicate, the reply sinks, OnActionStarted (null: the world's). */
	UActionHub* Hub = nullptr;
	/** The Roster (relations, legality, rebind and reset events; null: the world's). */
	URosterStore* Roster = nullptr;
	/** The Visibility Policy's cull input (null: no server cull, the client-local hide only). */
	IConcealmentCullSource* Cull = nullptr;
	/** The owner/team flag writers (null: no flags). */
	IConcealmentFlagSink* Flags = nullptr;
	IRoundLogWriter* Log = nullptr;
	IStealthFaultSink* Faults = nullptr;
	/** Optional: replaces the engine pawn adapter (tests). */
	IConcealmentPawnWorld* PawnWorld = nullptr;
	FConcealmentParams Params;
};

/**
 * UConcealmentService (ADR-0017 sections 1-3, 7; stories con-003..con-018, con-024): the server-only world subsystem facade
 * over FConcealmentCore. It is
 *   - the Hide resolver: PawnCommand HideEnter / HideExit (reached through the IIF fallback dispatch) and
 *     TimerFired(ConcealmentTransition), registered on Round's queue (a post-latch exemption: in-flight transitions keep
 *     resolving after the latch and write only Concealment state, the cull record and the owner/team flags);
 *   - the HideEnter / HideExit ICommandPredicate (registered with the hub by the composition root);
 *   - Crowd's ICrowdPerceptionFilter (IsPerceivable = not Concealed, IsSteeringObstacle = Outside; no viewer input, no
 *     occupancy query);
 *   - Assassination's IBushSpotQuery (spot zones, occupants minus the attacker's allies, IsConcealed, footprints) and
 *     IBodyRemovalObserver (the killed-concealed flags clear at the body's removal).
 * Created on server worlds only. It adds no tick, timer, actor or replicated state.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API UConcealmentService : public UWorldSubsystem, public IQueueResolver, public ICommandPredicate,
	public ICrowdPerceptionFilter, public IBushSpotQuery, public IBodyRemovalObserver
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** The service of World (server worlds), or null. */
	static UConcealmentService* Get(const UWorld* World);

	/** Binds the seams and registers the timer resolver on Init.Queue. Returns false (unbound) without a clock or queue. */
	bool Bind(const FConcealmentServiceInit& Init);
	/** Drops the registration and the core (the queue is gone or the world is ending). */
	void Unbind();
	bool IsBound() const { return Core.IsValid(); }

	/** The layout's bushes (the server's UBushRuntimeSubsystem build). */
	void SetBushes(TConstArrayView<FBushRuntime> Bushes);

	/** The core (tests, dumps); null when unbound. */
	FConcealmentCore* GetCore() const { return Core.Get(); }

	// ---- Events the composition root forwards ---------------------------------------------------------------------

	/** Assassination's OnPawnKilled (the outcome slot). */
	void HandlePawnKilled(APawn* Victim);
	/** Assassination's death write (the staging pass): the un-cull of a killed-concealed victim. */
	void HandleDeathWrite(APawn* Victim);
	/** A participant was bound to a new pawn (flags carried for its old killed-concealed body clear). */
	void HandleParticipantRebound(FParticipantId Participant, const APawn* NewPawn);

	// ---- Server queries --------------------------------------------------------------------------------------------

	bool IsInBushOrTransition(const APawn* Pawn) const;
	bool IsConcealedFrom(FParticipantId Viewer, const APawn* Pawn) const;
	EConcealState GetState(const APawn* Pawn) const;

	// IQueueResolver
	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;

	// ICommandPredicate
	virtual FPredicateResult CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const override;

	// ICrowdPerceptionFilter
	virtual bool IsPerceivable(const APawn* Pawn) const override;
	virtual bool IsSteeringObstacle(const APawn* Pawn) const override;

	// IBushSpotQuery
	virtual bool FindSpotZone(const FVector& WorldPoint, uint16& OutBushId) const override;
	virtual void GetSpotOccupants(uint16 BushId, const APawn* Attacker, TArray<APawn*>& OutOccupants) const override;
	virtual bool IsConcealed(const APawn* Pawn) const override;
	virtual bool GetFootprint(uint16 BushId, FBox2D& OutFootprint) const override;
	virtual void RemoveOccupant(APawn* Pawn) override;
	virtual void OnBushStrike(uint16 BushId, const APawn* Attacker, int32 CandidatesBeforeReach, int32 CandidatesAfterReach, FQueueKey Key) override;
	virtual void OnStrikeKilled(const APawn* Attacker) override;

	// IBodyRemovalObserver
	virtual void OnBodyRemovalCommitted(APawn* Body) override;

private:
	void HandleActionStarted(class AStealthPawn* Pawn, EActionType Type);
	void HandleRosterEvent(const FRosterEvent& Event);
	/** Roster ReleaseToCrowd (story con-032): the pawn is an NPC now. */
	void HandlePawnReleased(APawn* Pawn, FParticipantId FormerParticipant);

	struct FAdapters;
	TSharedPtr<FAdapters> Adapters;
	TUniquePtr<FConcealmentCore> Core;
	FResolutionQueueCore* RegisteredQueue = nullptr;
	const IQueueEpochSource* Epoch = nullptr;
	TWeakObjectPtr<UActionHub> Hub;
	TWeakObjectPtr<URosterStore> Roster;
	FDelegateHandle ActionStartedHandle;
	FDelegateHandle RosterEventHandle;
	FDelegateHandle ReleasedHandle;
};
