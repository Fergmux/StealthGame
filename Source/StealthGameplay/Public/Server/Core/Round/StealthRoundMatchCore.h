#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/Round/StealthRoundGrace.h"
#include "Server/Core/Round/StealthRoundLogArchive.h"
#include "Server/Core/Round/StealthRoundPhaseCore.h"
#include "Server/Core/Round/StealthRoundPreparing.h"
#include "Server/Core/Round/StealthRoundStart.h"
#include "Server/Core/Round/StealthRoundTeamRules.h"
#include "Server/Core/Round/StealthRoundTypes.h"
#include "Server/Core/Round/StealthRoundWin.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Templates/Function.h"

class FResolutionQueueCore;
class FRoundMatchCore;
class IRoundLogWriter;
class IStealthClock;
enum class ECommandRejectCode : uint8;

/**
 * The TimerFired(GraceExpiry) resolver (F4, R17.4; story rmf-040 creates the record and the timer, the full grace
 * timeline is stories rmf-060..071): at its slot the record must still exist under the captured generation and the
 * participant must still be away; then the guarded Disconnect transition is requested inline (its Roster event is
 * evaluated as an expiry by F2 in the same slot).
 */
class STEALTHGAMEPLAY_API FRoundGraceResolver final : public IQueueResolver
{
public:
	explicit FRoundGraceResolver(FRoundMatchCore& InCore) : Core(InCore) {}
	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;

private:
	FRoundMatchCore& Core;
};

/**
 * FRoundMatchCore: the plain-C++ Round match core URoundMatchStore holds (ADR-0005 section 2, ADR-0007 section 5;
 * GDD round-match-framework R3-R10, R22; stories rmf-018..028). It composes the phase machine, the team rules,
 * Start (validation, ordering and rollback) and the host-command entry point; every engine contact goes through the
 * injected gateways, the host sink, the Event Log writer and the clock, so tests run it on the recording fakes.
 *
 * Start (R5, decided rule C2): validate (FRoundStartValidator) -> (5b) CanRequestRoundTravel() (false: SessionRefused,
 * nothing changes) -> (6) SetAdmissionMode(RejoinOnly), SetTeamsLocked(true) -> (7) ResetAll(RoundsPlayedCount + 1)
 * -> (8) RequestRoundTravel -> (9) the phase becomes Preparing only when Session accepts the travel
 * (NotifyRoundTravelAccepted). A refusal at (8) rolls back: admission Open, teams unlocked, phase Lobby,
 * StartRolledBack logged, SessionRefused to the host. ResetAll is not undone and Reset never calls it (R10).
 *
 * Host commands (R22) share one entry point, ExecuteHostCommand, which checks the caller first (NotHost) and then the
 * phase matrix: reset/abort no-op in Lobby and Reset, abort in Preparing, Active only with bHostMayAbortActive,
 * Ended held until EndedMinDwellSec after the round end; swapteam/remove/preset Lobby only; debug commands go
 * through the real resolution queue as DebugItems (non-Shipping); dumplog only in Ended and Reset.
 *
 * Preparing (R6, stories rmf-031..040) runs in FRoundPreparingSequencer: the owners' events arrive through the
 * Notify* calls and Tick polls it. Every failure, the watchdog and a host abort in Preparing go through AbortPreparing
 * (story rmf-034): PreparingFailed(code) logged and archived, the R10 cleanup with RequestLobbyTravel, no result, no
 * score, RoundsPlayedCount unchanged, nothing released. At ActiveAtServerTime the Active-entry step runs (R7): the
 * GetUnboundActiveParticipants guard (rmf-041), then in one step ActiveAtServerTime 0, ActivePhaseStart, RoundsPlayedCount
 * + 1, the queue opened, grace records for participants Disconnected at P6 (rmf-040), OnRoundActive once and one Crowd
 * start (rmf-042).
 *
 * Wins (R15, F2; stories rmf-043..047): every applied single-participant Roster transition in Active is evaluated by
 * StealthRoundWin::EvaluateTransition and an objective completion by EvaluateObjectiveCompletion; the first outcome in
 * key order reaches TryTerminate, the single-assignment latch: it closes the queue (Latch(TerminalKey)), logs one
 * WinLatched, and for a win runs the round-end step (phase Ended, FRoundResult, MatchScore) in the same call; strike
 * staging of the round-end step is story rmf-053, so every win currently ends in its latch step. A void goes Active ->
 * Reset directly (called off). Later attempts return false and log LateWinSuppressed.
 */
class STEALTHGAMEPLAY_API FRoundMatchCore
{
public:
	/** Gateways.Session and Gateways.Roster are required; the others may be null until their systems exist. */
	FRoundMatchCore(const FRoundGateways& InGateways, IRoundHostSink* InHostSink, IRoundLogWriter* InLog, const IStealthClock* InClock);

	void SetParams(const FRoundRuleParams& InParams) { Params = InParams; }
	const FRoundRuleParams& GetParams() const { return Params; }

	/** Non-Shipping debug commands submit DebugItems here (null: they answer Unavailable). */
	void SetDebugQueue(FResolutionQueueCore* InQueue) { DebugQueue = InQueue; }
	/** The round's resolution queue (opened at Active entry, latched by TryTerminate); null: keys are adapter keys. */
	void SetRoundQueue(FResolutionQueueCore* InQueue) { RoundQueue = InQueue; }
	FResolutionQueueCore* GetRoundQueue() const { return RoundQueue; }
	/** The P6 spawn site (may be set after construction; the composition root registers it with the gateways). */
	void SetStartSpawner(IRoundStartSpawner* InSpawner);
	/** The writer Round and its queue append through (forwards to the injected log and keeps this round's slice). */
	IRoundLogWriter* GetRoundLogWriter() { return &LogRecorder; }
	/** The match-scoped archive of round slices (server only; survives travel with the store). */
	const FRoundLogSliceRecorder& GetLogArchive() const { return LogRecorder; }
	/** The resolver of TimerFired(GraceExpiry) (the store registers it on the round queue). */
	IQueueResolver* GetGraceResolver() { return &GraceResolver; }

	/** R7: raised once per round in the Active-entry step, after the state changes and before the Crowd start. */
	TFunction<void(uint32 /*RoundEpoch*/, double /*ActivePhaseStart*/)> OnRoundActive;
	/** F6 at freeze chose T_needed (the store logs a warning). */
	TFunction<void(const StealthRoundFormulas::FWatchdogBound&)> OnWatchdogWarning;
	/** P3: the Event Log header is due (the store writes it to FRoundEventLog). */
	TFunction<void(uint64 /*TuningHash*/)> OnWriteLogHeader;
	/** dumplog's action when it is allowed (the store prints the round log). */
	void SetDumpLogHandler(TFunction<void()> InHandler) { DumpLogHandler = MoveTemp(InHandler); }

	FRoundPhaseCore& GetPhaseCore() { return PhaseCore; }
	const FRoundPhaseCore& GetPhaseCore() const { return PhaseCore; }
	ERoundPhase GetPhase() const { return PhaseCore.GetPhase(); }
	bool AreTeamsLockedByRound() const { return bTeamsLocked; }
	bool IsTravelPending() const { return bTravelPending; }
	bool IsResetHeld() const { return bResetHeld; }

	// --- Match ---------------------------------------------------------------------------------------------------

	/** The match is created: Roster BeginMatch(TeamSize) once (R3), then Unknown -> Lobby. */
	ERosterResult OpenMatch();

	// --- Roster events (R3) ------------------------------------------------------------------------------------------

	ERosterTeam OnParticipantRegistered(FParticipantId Id);
	void OnParticipantReclaimed(FParticipantId Id) { TeamRules.OnParticipantReclaimed(Id); }

	// --- Start (R5) -------------------------------------------------------------------------------------------------

	/** Validates and, when accepted, runs steps 5b-8. The refusal (if any) also reaches the host sink. */
	FStartRefusal Start(const FRoundCommandCaller& Caller);

	/**
	 * Session accepted the round travel: the phase becomes Preparing (step 9) and the readiness sequence begins with
	 * FRoundParams.Seed (P1). False when no travel was pending.
	 */
	bool NotifyRoundTravelAccepted(int32 Seed = 0);

	// --- Preparing (R6): the owners' events ------------------------------------------------------------------------

	void NotifyTravelFailed() { Preparing.OnTravelFailed(); }
	void NotifyTuningReady(uint64 TuningHash) { Preparing.OnTuningReady(TuningHash); }
	void NotifyTuningFailed() { Preparing.OnTuningFailed(); }
	void NotifyTuningSnapshotFault() { Preparing.OnTuningSnapshotFault(); }
	void NotifyLayoutReady(uint32 LayoutId, int32 Seed) { Preparing.OnLayoutReady(LayoutId, Seed); }
	void NotifyLayoutAborted() { Preparing.OnLayoutAborted(); }
	bool NotifyParticipantTravelComplete(const FRoundTravelAck& Ack) { return Preparing.OnParticipantTravelComplete(Ack); }
	bool NotifyParticipantWorldReady(const FRoundTravelAck& Ack) { return Preparing.OnParticipantWorldReady(Ack); }
	const FRoundPreparingSequencer& GetPreparing() const { return Preparing; }

	/**
	 * Session's connection facts (R17): Disconnected records DisconnectedAt (and the Kicked flag, F2 Kick); in Active it
	 * opens the grace record and timer. Connected opens a closed spawn gate and closes a grace record (the rejoin).
	 */
	void OnParticipantConnectionChanged(FParticipantId Participant, ERoundConnectionState State, bool bKicked, double AtServerTime);

	// --- Active entry (R7) -----------------------------------------------------------------------------------------

	/**
	 * The Active-entry step now (the sequencer calls it at ActiveAtServerTime; tests and the forced driver call it
	 * directly): the unbound guard (abort BindFailed, Active never entered), then the one step. False when not entered.
	 */
	bool EnterActive();
	/** R6: commands are rejected until Active (NotNow); unset when the phase admits commands. */
	TOptional<ECommandRejectCode> AdmitCommand() const;
	double GetActivePhaseStart() const { return ActivePhaseStart; }
	int32 GetActiveEntryCount() const { return ActiveEntryCount; }
	const FRoundGraceTable& GetGraceTable() const { return Grace; }

	// --- Wins and the latch (R15, F2) --------------------------------------------------------------------------------

	/** The single-assignment latch (see the class comment). bAlreadyLogged: the deciding resolver wrote WinLatched. */
	bool TryTerminate(const FRoundTerminalOutcome& Outcome, FQueueKey Key, bool bAlreadyLogged = false);
	const FRoundTerminalLatch& GetLatch() const { return Latch; }
	bool IsTerminated() const { return Latch.IsTerminated(); }

	/** Objectives' resolver: the final completion re-validated (R15); TryTerminate(Win(Team, ObjectiveWin), Key). */
	bool DeclareObjectiveWin(uint8 Team, FQueueKey Key);
	/** An objective completion through F2 ObjectiveWin (a preliminary or invalid one never ends the round). */
	bool SubmitObjectiveCompletion(const FRoundObjectiveCompletion& Completion);
	/** An applied single-participant Roster transition (or a batch event, never evaluated): F2 in the causing slot. */
	void OnRosterTransition(const FRoundTransitionEvent& Event);
	/** F2's round-level inputs now (Roster counts, Connected Active counts, phase, latch). */
	FRoundWinContext MakeWinContext() const;
	/** The deciding key: the current queue slot's key, else an adapter key (prototype event sources, outside a slot). */
	FQueueKey DecisionKey();

	/** The legacy driver (tests, dev): TryTerminate(Win(WinningTeam, ObjectiveWin|Elimination)) with an adapter key. */
	bool EndRound(uint8 WinningTeam, uint8 PublicReason);
	/** The DebugItem effect (FRoundDebugResolver::OnDebug): debugwin latches DebugForced, debugeliminate requests the transition. */
	void ApplyDebugItem(const FDebugPayload& Payload, FQueueKey Key);

	/** Adapter keys use Seqs from here up so they never collide with a stamped queue Seq of the same tick. */
	static constexpr uint64 AdapterKeySeqBase = 1ull << 62;

	// --- Host commands (R22) ----------------------------------------------------------------------------------------

	ERoundCommandResult ExecuteHostCommand(const FRoundCommandCaller& Caller, ERoundHostCommand Command, const FRoundCommandArgs& Args = FRoundCommandArgs());

	/** Runs a held reset when the Ended dwell has elapsed, and completes Reset -> Lobby. Call once per server frame. */
	void Tick();

private:
	friend class FRoundGraceResolver;

	double Now() const;
	ERoundCommandResult ExecuteReset(ERoundHostCommand Command);
	/** R10 Reset; bFromPreparing adds the Preparing-abort cleanup (pawns destroyed, deal discarded, lobby travel). */
	bool EnterReset(bool bCalledOff, ERoundEndReason Reason, uint8 AbortCode, bool bFromPreparing);
	/** The one Preparing abort path (R8, story rmf-034); bHost for the host abort (bCalledOff, EndReason Aborted). */
	void AbortPreparing(ERoundPreparingAbortCode Code, bool bHost);
	void RunRoundEndStep(const FRoundTerminalOutcome& Outcome);
	void OpenGraceRecord(FParticipantId Participant, double DisconnectedAt, const FEpochToken& Epoch);
	void LogEntry(FRoundLogEntry& Entry);
	ERoundCommandResult SubmitDebugItem(ERoundHostCommand Command, const FRoundCommandArgs& Args);
	FRoundStartInputs GatherStartInputs(bool bIsHost) const;
	void ReportStartRefusal(const FStartRefusal& Refusal);
	void LogStartRolledBack();

	FRoundGateways Gateways;
	IRoundHostSink* HostSink = nullptr;
	FRoundLogSliceRecorder LogRecorder;
	const IStealthClock* Clock = nullptr;
	FResolutionQueueCore* DebugQueue = nullptr;
	FResolutionQueueCore* RoundQueue = nullptr;
	TFunction<void()> DumpLogHandler;

	FRoundRuleParams Params;
	FRoundPhaseCore PhaseCore;
	FRoundTeamRules TeamRules;
	FRoundPreparingSequencer Preparing;
	FRoundTerminalLatch Latch;
	FRoundGraceTable Grace;
	FRoundGraceResolver GraceResolver{ *this };

	uint32 NextAckNonce = 0;
	double ActivePhaseStart = 0.0;
	int32 ActiveEntryCount = 0;
	uint64 AdapterSeq = 0;
	/** The epoch of the round being prepared or played (captured at P6 and Active entry). */
	FEpochToken RoundEpochToken;
	TSet<uint32> KickedParticipants;
	TMap<uint32, double> LastDisconnectedAt;

	bool bTeamsLocked = false;
	bool bTravelPending = false;
	bool bResetHeld = false;
	bool bResetPendingLobby = false;
	double EndedAtServerTime = 0.0;
};

/**
 * The DebugItem resolver (R22, ADR-0002 section 2a): every stealth.round.debugwin / debugeliminate consumes a queue
 * Seq, is logged with the Debug flag (so F7 excludes it) and then calls OnDebug, which applies the effect. Compiled
 * in every configuration (the item type exists in every layout) but only non-Shipping producers submit items.
 */
class STEALTHGAMEPLAY_API FRoundDebugResolver final : public IQueueResolver
{
public:
	/** Command values carried in FDebugPayload::Command. */
	static constexpr uint8 CommandDebugWin = 1;
	static constexpr uint8 CommandDebugEliminate = 2;

	FRoundDebugResolver(IRoundLogWriter& InLog, const IStealthClock& InClock) : Log(InLog), Clock(InClock) {}

	/** Applies the debug effect after the entry is logged (the store wires it to EndRound / the Roster transition). */
	TFunction<void(const FDebugPayload& /*Payload*/, FQueueKey /*Key*/)> OnDebug;

	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;

private:
	IRoundLogWriter& Log;
	const IStealthClock& Clock;
};
