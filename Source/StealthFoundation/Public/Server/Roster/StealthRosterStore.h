#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Server/Roster/StealthRosterCore.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "UObject/ObjectKey.h"

#include "StealthRosterStore.generated.h"

class APawn;
class FRosterPushStep;
class IEntropySource;
class URosterTeamViewComponent;
struct FRosterStoreAdapters;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRosterStoreEvent, const FRosterEvent& /*Event*/);

/** Optional overrides of URosterStore's engine adapters (tests only; production passes none). */
struct FRosterStoreInitOverrides
{
	/** Forces the authority answer (a client process is false). */
	TOptional<bool> bAuthority;
	/** Replaces the production fault sink. Must outlive the store. */
	IStealthFaultSink* FaultSink = nullptr;
	/** Replaces the OS entropy source of the session secret. Used only during initialisation. */
	IEntropySource* Entropy = nullptr;
	/** Replaces the world clock. Must outlive the store. */
	IStealthClock* Clock = nullptr;
};

/**
 * URosterStore: the game-instance facade over the plain-C++ FRosterCore (ADR-0005 section 2; ADR-0007 facade
 * split; story psr-061, decision D43). Exactly one core per game instance; it survives seamless and hard travel
 * with the game instance.
 *
 * The facade only adapts engine objects to the core's interfaces: authority from the game instance's net mode (a
 * client process has none), game thread from IsInGameThread(), pawn liveness from the pawn object, the pawn handle
 * from the pawn's object key plus a per-pawn spawn generation, the server clock from the game instance's world, the
 * Foundation fault sink, the session secret's sub-seeds for IRosterRandom (ADR-0006 domains 15 PublicHandle and 17
 * MatchId), and Round's phase through IRosterRoundPhase (registered weakly by the composition root; none
 * registered = not Active). It adds no rule of its own.
 *
 * Match lifetime: OpenSession runs BeginMatch(TeamSize) with Round's latched TeamSize; EndMatch runs on every
 * session exit path (NotifyHostQuit, NotifyReturnToMainMenu, NotifySessionEnding, game-instance shutdown and the
 * cleanup of this game instance's last game world), so a former host keeps no record, binding or MatchId.
 *
 * On a client process the store exists but stays empty and every query answers the client value and faults.
 * Events are re-broadcast to server-only native listeners through OnRosterEvent, and drive the C-TeamView push
 * step, which the store flushes once per frame.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHFOUNDATION_API URosterStore : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	URosterStore();
	/** Declared so the generated vtable helper constructor is compiled where the adapters are complete. */
	URosterStore(FVTableHelper& Helper);
	virtual ~URosterStore() override;

	// --- Subsystem lifetime --------------------------------------------------------------------------------------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** The UGameInstance::Shutdown exit path: EndMatch, then the core is released. */
	virtual void Deinitialize() override;

	/** Builds the core and its adapters (Initialize calls it with no overrides; tests call it directly). */
	void InitializeStore(const FRosterStoreInitOverrides& Overrides = FRosterStoreInitOverrides());
	/** Ends the match and releases the core (Deinitialize calls it). */
	void ShutdownStore();
	bool IsStoreInitialized() const;

	/** The store of World's game instance, or null. */
	static URosterStore* Get(const UWorld* World);

	// --- Match lifetime (ADR-0005 section 2) ----------------------------------------------------------------------

	/** Session.OpenSession: BeginMatch(TeamSize), TeamSize latched by Round. */
	ERosterResult OpenSession(int32 TeamSize);
	/** True between a successful OpenSession and the next exit path. */
	bool IsMatchOpen() const;

	/** Exit path: the host quits. */
	void NotifyHostQuit();
	/** Exit path: return to the main menu. */
	void NotifyReturnToMainMenu();
	/** Exit path: Session's OnSessionEnding. */
	void NotifySessionEnding();
	/** Exit path: UGameInstance::Shutdown (Deinitialize runs it before releasing the core). */
	void HandleGameInstanceShutdown();
	/** Exit path: FWorldDelegates::OnWorldCleanup of this game instance's last game world (bound in Initialize). */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	/** Session marks a seamless/hard round travel in progress: the old world's cleanup is then not an exit path. */
	void SetTravelInProgress(bool bInProgress) { bTravelInProgress = bInProgress; }

	// --- Weak registrations ----------------------------------------------------------------------------------------

	/** Registers Round's phase input. Owner is held weakly: when it dies the phase reads not-Active (and faults once). */
	void RegisterRoundPhase(IRosterRoundPhase* RoundPhase, UObject* Owner);
	void UnregisterRoundPhase(UObject* Owner);

	/** Raised (server only) for every applied Roster event, in EventSeq order. */
	FOnRosterStoreEvent OnRosterEvent;

	// --- Pawn handles ----------------------------------------------------------------------------------------------

	/** The core's handle for Pawn: its object key plus its current spawn generation. Null pawn -> null handle. */
	FRosterPawnHandle MakePawnHandle(const APawn* Pawn);
	/** Bumps Pawn's spawn generation (the pawn factory calls it at each spawn, ADR-0010). */
	void NotifyPawnSpawned(const APawn* Pawn);
	/** The engine pawn behind Handle, or null when unknown or gone. */
	APawn* ResolvePawn(const FRosterPawnHandle& Handle) const;

	// --- Typed server calls (forward to FRosterCore; see StealthRosterCore.h for each contract) -------------------

	FRosterRegisterResult RegisterParticipant(const FIdentityKey& Key, const FString& DisplayName);
	ERosterResult RemoveParticipant(FParticipantId Id);
	ERosterResult SetTeamSize(int32 TeamSize);
	ERosterResult SetTeamsLocked(bool bLocked);
	ERosterResult AssignTeams(TConstArrayView<FRosterTeamAssignment> Batch);
	ERosterResult NotifyConnectionChanged(FParticipantId Id, ERosterConnection Projection, bool bKicked = false);
	ERosterResult BindPawn(FParticipantId Id, APawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch = {});
	ERosterResult ReleaseToCrowd(APawn* Pawn);
	/** FRosterCore::RetireBinding (story psr-020): the informant-kill retire, no lifecycle transition, no event. */
	ERosterResult RetireBinding(APawn* Pawn, int64 RetireAtServerTimeMs, ERosterReason Reason = ERosterReason::InformantKilled);
	ERosterResult ClearBinding(APawn* Pawn, EEndPlayReason::Type Reason);
	ERosterResult InvalidateAllBindings();
	ERosterResult RequestTransition(FParticipantId Id, EParticipantState NewState, ERosterReason Reason, const FRosterTransitionOptions& Options = {});
	ERosterResult ResetAll(uint32 RoundIndex);

	FRosterVictimKind ResolveVictimKind(APawn* Pawn);
	bool IsPawnBound(APawn* Pawn);
	EParticipantState GetLegalityClass(APawn* Pawn);
	int32 CountActive(int32 Team) const;
	int32 CountInformant(int32 Team) const;
	int32 CountRoster(int32 Team) const;
	bool IsTeamEliminated(int32 Team) const;
	APawn* GetPawn(FParticipantId Id) const;
	FParticipantId GetParticipant(APawn* Pawn);
	void GetUnboundActiveParticipants(TArray<FParticipantId>& OutIds) const;

	/** Server-internal reads (no authority check: callers are server code already). */
	const FParticipantRecord* FindRecord(FParticipantId Id) const;
	void GetParticipantIds(TArray<FParticipantId>& OutIds) const;
	FRosterEpochToken GetEpochToken() const;
	/** The core, read-only (tests and dumps). Valid only while IsStoreInitialized(). */
	const FRosterCore& GetCore() const;

	// --- C-TeamView (story psr-062) ---------------------------------------------------------------------------------

	/** Binds Id's controller-owned view component; the push step is its only writer. */
	void RegisterTeamView(FParticipantId Id, URosterTeamViewComponent* View);
	void UnregisterTeamView(FParticipantId Id);
	/** Runs the push step now (the tick calls it once per frame); returns the components written. */
	int32 FlushTeamViews();
	/** The push step's reconnect flag of Id (dumps and tests). */
	bool IsTeammateReconnecting(FParticipantId Id) const;

	// --- FTickableGameObject ------------------------------------------------------------------------------------------

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }

private:
	/** Every exit path lands here (ADR-0005 Implementation Guideline 12). */
	void EndMatchOnExitPath();
	FRosterCore& CoreRef() const;

	TUniquePtr<FRosterStoreAdapters> Adapters;
	TUniquePtr<FRosterCore> Core;
	TUniquePtr<FRosterPushStep> PushStep;

	/** Spawn generation per pawn object (ADR-0010 factory input; bumped at each spawn). */
	TMap<FObjectKey, uint32> PawnGenerations;
	/** Handle object key -> pawn, for liveness and resolution. */
	TMap<uint64, TWeakObjectPtr<APawn>> PawnsByKey;

	FDelegateHandle WorldCleanupHandle;
	bool bMatchOpen = false;
	bool bTravelInProgress = false;
	uint64 LastFlushFrame = MAX_uint64;

	friend struct FRosterStoreAdapters;
};
