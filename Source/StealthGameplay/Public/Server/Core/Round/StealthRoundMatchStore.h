#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundMatchCore.h"
#include "Server/Core/Round/StealthRoundPublicStateMirror.h"
#include "Server/Roster/StealthRosterDeps.h"
#include "Server/StealthClock.h"
#include "Server/Session/StealthSessionCore.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/Function.h"
#include "Tickable.h"

#include "StealthRoundMatchStore.generated.h"

class FOutputDevice;
class URosterStore;
struct FRosterEvent;

class URoundMatchStore;

/** URoundMatchStore's clock: the game instance's world time (process time before a world exists). */
class STEALTHGAMEPLAY_API FRoundStoreWorldClock final : public IStealthClock
{
public:
	explicit FRoundStoreWorldClock(const URoundMatchStore& InStore) : Store(InStore) {}
	virtual double GetServerTimeSeconds() const override;
	virtual uint64 GetServerTickIndex() const override { return GFrameCounter; }

private:
	const URoundMatchStore& Store;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRoundPhaseChanged, ERoundPhase /*Old*/, ERoundPhase /*New*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundHostMessage, const FString& /*Message*/);

/**
 * URoundMatchStore: the game-instance facade over FRoundMatchCore (ADR-0005 section 2: one of the four stores, it
 * survives travel; ADR-0007 plain core behind a facade). Server-authoritative; on a client it holds no core.
 *
 * Wiring (by the composition root, weakly): RegisterGateways with the six Round gateways, RegisterPublicStateSink
 * with the current GameState in InitGameState, SetHostParticipant / SetLocalParticipantResolver from Session. The
 * store itself subscribes to URosterStore's events (OnParticipantRegistered -> R3 assignment) and registers as the
 * Roster's IRosterRoundPhase input (decision D43).
 *
 * Host commands (R22): stealth.round.start, reset, abort, swapteam <participant> [<participant>], remove
 * <participant>, preset <name> <teamsize>, and in development builds debugwin <team>, debugeliminate
 * <participant>, dumplog. They are registered with IConsoleManager here and nowhere else (one process-wide
 * registration shared by every game instance, routed by the executing world), and every handler calls the one
 * host-checked entry point (ExecuteCommand). A command reaching the server through a remote client's ServerExec
 * runs inside FRoundRemoteExecScope and is NotHost.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API URoundMatchStore : public UGameInstanceSubsystem, public FTickableGameObject, public IRoundHostSink, public IRosterRoundPhase,
	public IStealthMatchStore
{
	GENERATED_BODY()

public:
	URoundMatchStore();
	virtual ~URoundMatchStore() override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The store of World's game instance, or null. */
	static URoundMatchStore* Get(const UWorld* World);

	// --- Wiring --------------------------------------------------------------------------------------------------

	/** Builds the match core over Gateways (Session and Roster required). Owner is held weakly. */
	void RegisterGateways(const FRoundGateways& Gateways, UObject* Owner);
	/** True once gateways are registered (server only). */
	bool HasMatchCore() const { return Core.IsValid(); }

	void SetRuleParams(const FRoundRuleParams& Params);
	/** Registers the current GameState and re-seeds it with the whole public struct (InitGameState, story rmf-029). */
	void RegisterPublicStateSink(IRoundPublicStateSink* Sink, UObject* Owner);
	/** The participant id of the host (Session: the listen host's own participant). */
	void SetHostParticipant(FParticipantId HostId) { HostParticipant = HostId; }
	FParticipantId GetHostParticipant() const { return HostParticipant; }
	/** Session's map from World's first local controller to its participant (for the console caller). */
	void SetLocalParticipantResolver(TFunction<FParticipantId(const UWorld*)> Resolver) { LocalParticipantResolver = MoveTemp(Resolver); }

	// --- Match flow ------------------------------------------------------------------------------------------------

	/** Creates the match: Roster BeginMatch(TeamSize), phase Lobby. */
	ERosterResult OpenMatch();
	/** Session accepted the round travel (or, without travel, the composition root confirms it): Preparing. */
	bool NotifyRoundTravelAccepted();
	/** Preparing -> Active (composition-root driver until the Preparing barrier stories). */
	bool EnterActive();
	/** Active -> Ended with the public result. */
	bool EndRound(uint8 WinningTeam, uint8 PublicReason);

	/** The one host-checked entry point (console and host panel). */
	ERoundCommandResult ExecuteCommand(const FRoundCommandCaller& Caller, ERoundHostCommand Command, const FRoundCommandArgs& Args = FRoundCommandArgs());
	/** The console path: builds the caller from World (net mode, local controller, participant) then ExecuteCommand. */
	ERoundCommandResult ExecuteConsoleCommand(const UWorld* World, ERoundHostCommand Command, const FRoundCommandArgs& Args);
	/** The caller a console command executed in World would have. */
	FRoundCommandCaller MakeConsoleCaller(const UWorld* World) const;

	ERoundPhase GetPhase() const;
	/** The match core (server; null on a client or before RegisterGateways). */
	FRoundMatchCore* GetMatchCore() const { return Core.Get(); }
	const FRoundPublicStateMirror& GetMirror() const { return Mirror; }

	FOnRoundPhaseChanged OnPhaseChanged;
	/** Host-only messages (refusals and command results) for the host's HUD and log. */
	FOnRoundHostMessage OnHostMessage;

	/** The frozen R22 command names, in ERoundHostCommand order. */
	static const TCHAR* const CommandNames[];
	static constexpr int32 CommandCount = 9;

	// --- IRoundHostSink ---------------------------------------------------------------------------------------------
	virtual void OnStartRefused(const FStartRefusal& Refusal) override;
	virtual void OnCommandRefused(ERoundHostCommand Command, ERoundCommandResult Result, uint8 Detail) override;

	// --- IRosterRoundPhase (decision D43) --------------------------------------------------------------------------
	virtual bool IsRoundPhaseActive() const override;

	// --- IStealthMatchStore (session-travel st-014: Session's EndMatch fan-out) ------------------------------------
	virtual FName GetMatchStoreName() const override { return TEXT("Round"); }
	/** Releases the match core (phase, score, Event Log) and the public mirror; the next world start re-registers. */
	virtual void EndMatch(ESessionEndReason Reason) override;
	/** True when no match is open (no core, or a core that has not opened a match). */
	virtual bool IsMatchStateEmpty() const override;

	// --- FTickableGameObject -------------------------------------------------------------------------------------------
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }

private:
	void HandleRosterEvent(const FRosterEvent& Event);
	void HandlePhaseChanged(ERoundPhase Old, ERoundPhase New);
	void PublishPublicState();
	static void RegisterConsoleCommands();
	static void UnregisterConsoleCommands();
	static void HandleConsole(ERoundHostCommand Command, const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);

	TUniquePtr<FRoundMatchCore> Core;
	TWeakObjectPtr<UObject> GatewayOwner;
	FRoundRuleParams PendingParams;
	FRoundPublicStateMirror Mirror;
	FRoundStoreWorldClock Clock{ *this };
	TWeakObjectPtr<URosterStore> RosterStore;
	FDelegateHandle RosterEventHandle;
	FParticipantId HostParticipant;
	TFunction<FParticipantId(const UWorld*)> LocalParticipantResolver;
};

/**
 * Marks a console command as arriving from a remote client's ServerExec (AStealthPlayerController::ConsoleCommand
 * on a non-local controller). Round's host check refuses every command run inside it with NotHost.
 */
struct STEALTHGAMEPLAY_API FRoundRemoteExecScope
{
	FRoundRemoteExecScope();
	~FRoundRemoteExecScope();

	/** True while any remote exec scope is open on this thread. */
	static bool IsActive();
};
