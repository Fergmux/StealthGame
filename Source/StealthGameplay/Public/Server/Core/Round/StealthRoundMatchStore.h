#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundMatchCore.h"
#include "Server/Core/Round/StealthRoundPublicStateMirror.h"
#include "Server/EventLog/StealthRoundEventLog.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/Roster/StealthRosterDeps.h"
#include "Server/StealthLogFaultSink.h"
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

/** The round queue's epoch source: the Roster's current (MatchId, RoundEpoch) through Round's Roster gateway. */
class STEALTHGAMEPLAY_API FRoundStoreEpochSource final : public IQueueEpochSource
{
public:
	IRoundRosterGateway* Roster = nullptr;
	virtual FEpochToken GetCurrentEpoch() const override { return Roster != nullptr ? Roster->GetEpochToken() : FEpochToken(); }
};

/** The round queue's connection generations: Session's, through Round's Session gateway. */
class STEALTHGAMEPLAY_API FRoundStoreGenerationSource final : public IQueueConnectionGenerationSource
{
public:
	IRoundSessionGateway* Session = nullptr;
	virtual uint32 GetConnectionGeneration(FParticipantId Participant) const override { return Session != nullptr ? Session->GetConnectionGeneration(Participant) : 0; }
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRoundPhaseChanged, ERoundPhase /*Old*/, ERoundPhase /*New*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRoundActive, uint32 /*RoundEpoch*/, double /*ActivePhaseStart*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundHostMessage, const FString& /*Message*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundEnded, const FRoundResult& /*Result*/);

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
	public IStealthMatchStore, public IRoundLogAccessGate
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

	/**
	 * Builds the match core over Gateways (Session and Roster required), the round's resolution queue (sub-seeds from
	 * SubSeeds; null: no queue, debug commands answer Unavailable and decision keys are adapter keys) with the debug and
	 * grace resolvers, and the Event Log. Owner is held weakly.
	 */
	void RegisterGateways(const FRoundGateways& Gateways, UObject* Owner, const IServerSubSeedSource* SubSeeds = nullptr);
	/** The P6 spawn site (the composition root's adapter over UStealthPawnFactory). */
	void SetStartSpawner(IRoundStartSpawner* Spawner);
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
	/** Session accepted the round travel (or, without travel, the composition root confirms it): Preparing with Seed. */
	bool NotifyRoundTravelAccepted(int32 Seed = 0);
	/** The forced Active-entry step (dev and harness; the sequencer enters Active on its own). */
	bool EnterActive();
	/** The legacy win driver (harness): TryTerminate(Win) with an adapter key. */
	bool EndRound(uint8 WinningTeam, uint8 PublicReason);

	// --- Preparing owner events (R6; adapters forward them until Session, TDC and LSC facades exist) ----------------

	void NotifyTuningReady(uint64 TuningHash);
	void NotifyTuningFailed();
	void NotifyLayoutReady(uint32 LayoutId, int32 Seed);
	void NotifyLayoutAborted();
	bool NotifyParticipantTravelComplete(const FRoundTravelAck& Ack);
	bool NotifyParticipantWorldReady(const FRoundTravelAck& Ack);
	/** This Preparing's acknowledgement nonce (Session stamps acknowledgements with it). */
	FRoundAckEpoch GetPreparingAckEpoch() const;

	// --- Win sources (R15; prototype adapters until Objectives and Assassination route through the queue) -----------

	/** An objective completion with its re-validated tender: F2 ObjectiveWin (a stage completion never ends the round). */
	bool SubmitObjectiveCompletion(const FRoundObjectiveCompletion& Completion);
	/** R6: commands are rejected until Active (NotNow); unset when admitted. */
	TOptional<ECommandRejectCode> AdmitCommand() const;
	/** True in Active before the latch (the prototype's "round in progress"). */
	bool IsRoundLive() const;
	/** The round queue (null before RegisterGateways or without sub-seeds). */
	FResolutionQueueCore* GetRoundQueue() const { return Queue.Get(); }

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
	/** R7 OnRoundActive(RoundEpoch, ActivePhaseStart): once per round, in the Active-entry step. */
	FOnRoundActive OnRoundActive;
	/** R9 (e) OnRoundEnded(Result): once per round, in the round-end step (after EndRoundAll and the held replies). */
	FOnRoundEnded OnRoundEnded;
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

	// --- IRoundLogAccessGate ---------------------------------------------------------------------------------------
	/** The log's read and disk paths open only once the round is terminated (Ended or Reset). */
	virtual bool IsLogReleasable() const override;

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

	void WriteLogHeader(uint64 TuningHash);

	TUniquePtr<FRoundMatchCore> Core;
	FStealthLogFaultSink QueueFaults;
	FRoundStoreEpochSource EpochSource;
	FRoundStoreGenerationSource GenerationSource;
	TUniquePtr<FRoundEventLog> EventLog;
	TUniquePtr<FResolutionQueueCore> Queue;
	TUniquePtr<FRoundDebugResolver> DebugResolver;
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
