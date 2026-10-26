#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Session/StealthSessionCore.h"
#include "Server/StealthLogFaultSink.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "StealthSessionService.generated.h"

class URosterStore;

/** OpenSession inputs (ADR-0005 Key Interfaces FSessionOpenParams). */
struct FSessionOpenParams
{
	/** Roster.BeginMatch(TeamSize) (TeamSize latched by Round). */
	int32 TeamSize = 5;
	/**
	 * Whether OpenSession runs Roster.BeginMatch itself. The D53 composition root opens the Roster match through
	 * Round's OpenMatch (Round latches TeamSize and enters Lobby), so it passes false.
	 */
	bool bBeginRosterMatch = true;
};

/** Optional overrides of USessionService's engine adapters (tests only; production passes none). */
struct FSessionServiceInitOverrides
{
	/** Forces the authority answer (a client process is false). */
	TOptional<bool> bAuthority;
	/** Replaces the production fault sink. Must outlive the service. */
	IStealthFaultSink* FaultSink = nullptr;
	/** Replaces the OS entropy source for every OpenSession. Must outlive the service. */
	IEntropySource* Entropy = nullptr;
	/** The Roster store to fan out to (tests build stores by hand; production finds the game instance's). */
	URosterStore* Roster = nullptr;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnStealthSessionEnded, ESessionEndReason /*Reason*/);

/**
 * USessionService: the game-instance facade over FSessionCore (ADR-0005 section 2; story st-014). It owns the session
 * lifetime: OpenSession draws ServerSecret and MatchNonce from the OS CSPRNG (failing closed, "could not host"), checks
 * that every match store is empty (a store that survives into a new session is a defect, Guideline 12), and begins the
 * Roster match; EndSession runs EndMatch on Session, Roster and every registered store (Round, Tuning) on every exit
 * path: host quit, return to the main menu, OnSessionEnding, UGameInstance::Shutdown (Deinitialize) and the cleanup
 * of this game instance's last networked game world (not the old world of a travel). Server-only: mutating calls
 * check authority and the game thread; on a client process the service exists and stays empty.
 *
 * Seeds (ADR-0006): GetServerSubSeed / DrawRoundSeed / PublicSubSeed go through the core's FStealthServerSeeds, whose
 * misuse branch refuses calls without authority, off the game thread or inside FPublicOnlyScope.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHFOUNDATION_API USessionService : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** The UGameInstance::Shutdown exit path. */
	virtual void Deinitialize() override;

	/** Binds the adapters (Initialize calls it with no overrides; tests call it directly). */
	void InitializeService(const FSessionServiceInitOverrides& Overrides = FSessionServiceInitOverrides());

	/** The service of World's game instance, or null. */
	static USessionService* Get(const UWorld* World);

	// --- Lifetime -----------------------------------------------------------------------------------------------------

	/** Host: opens the session (no-op Opened when already open). CouldNotHost: nothing opens, the fault sink is told. */
	EStealthSessionOpenResult OpenSession(const FSessionOpenParams& Params);
	/** Every exit path lands here: EndMatch on Session, Roster and each registered store (idempotent). */
	void EndSession(ESessionEndReason Reason);

	void NotifyHostQuit() { EndSession(ESessionEndReason::HostQuit); }
	void NotifyReturnToMainMenu() { EndSession(ESessionEndReason::ReturnToMainMenu); }
	void NotifySessionEnding() { EndSession(ESessionEndReason::SessionEnding); }
	/** UGameInstance::Shutdown (Deinitialize calls it). */
	void HandleGameInstanceShutdown() { EndSession(ESessionEndReason::GameInstanceShutdown); }
	/** FWorldDelegates::OnWorldCleanup (bound in InitializeService): this game instance's networked game world. */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	/** A round travel is in progress: the old world's cleanup is then not an exit path (forwarded to the Roster). */
	void SetTravelInProgress(bool bInProgress);

	bool IsSessionOpen() const { return Core.IsOpen(); }

	/**
	 * True when Session, Roster and every registered store hold no match state. OutNonEmpty (optional) receives the
	 * names of the stores that do. A Tuning store not yet registered is not checked (its epic registers it).
	 */
	bool AreAllStoresEmpty(TArray<FName>* OutNonEmpty = nullptr) const;

	/** The answer of the last OpenSession's emptiness check (true when every store was empty at its start). */
	bool WereStoresEmptyAtLastOpen() const { return bStoresEmptyAtLastOpen; }

	/** Registers a match store (Round, Tuning). Owner is held weakly. */
	void RegisterMatchStore(IStealthMatchStore* Store, UObject* Owner);
	void UnregisterMatchStore(UObject* Owner);

	/** Raised after every EndSession fan-out. */
	FOnStealthSessionEnded OnSessionEnded;

	// --- Seeds (ADR-0006) -------------------------------------------------------------------------------------------

	/** F5 DrawRoundSeed (redraw on used and golden seeds); 0 when no session is open. */
	int32 DrawRoundSeed(uint32 RoundIndex);
	/** Server-only sub-seed (misuse branch: no authority, off the game thread, FPublicOnlyScope). */
	uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const;
	/** The seed-core seam handed to plain cores (the resolution queue, the Roster). */
	const IServerSubSeedSource& GetSubSeedSource() const { return Core.GetSeeds(); }

	/** The core, read-only (tests and dumps). */
	const FSessionCore& GetCore() const { return Core; }

private:
	bool HasAuthority() const;
	IStealthFaultSink& GetFaultSink();
	URosterStore* GetRoster() const;

	FSessionCore Core;
	FStealthLogFaultSink LogFaultSink;
	IStealthFaultSink* FaultSinkOverride = nullptr;
	IEntropySource* EntropyOverride = nullptr;
	TOptional<bool> AuthorityOverride;
	TWeakObjectPtr<URosterStore> RosterOverride;
	struct FRegisteredStore
	{
		IStealthMatchStore* Store = nullptr;
		TWeakObjectPtr<UObject> Owner;
	};
	TArray<FRegisteredStore> Stores;
	FDelegateHandle WorldCleanupHandle;
	bool bTravelInProgress = false;
	bool bStoresEmptyAtLastOpen = true;
	bool bServiceInitialized = false;
};
