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
#include "Server/Queue/StealthStagedWriteApplier.h"
#include "Templates/Function.h"

class FResolutionQueueCore;
class FRoundMatchCore;
class IRoundLogWriter;
class IStealthClock;
enum class ECommandRejectCode : uint8;

/** Why Session reports a participant Disconnected (R17.2; a supersede is a loss at the supersede instant, ruling D24). */
enum class ERoundDisconnectReason : uint8
{
	/** A socket close or quit. */
	Quit,
	/** Session's Hard timeout of a silent connection. */
	HardTimeout,
	/** Disconnected(Kicked): the host kick takes the same grace path; the record carries Kicked (R15 (c)). */
	Kicked,
	/** OnParticipantDisconnected(Superseded), raised when Session accepts a superseding connection (D24). */
	Superseded,
	/** Session's F7 travel deadline missed in Preparing. */
	TravelTimeout,
};

/** The viewer class Round supplies to the Visibility Policy (R9 (e): every viewer is PostRound from the round-end step). */
enum class ERoundViewerClass : uint8
{
	InRound,
	PostRound,
};

/** FConnectionTransitionPayload::Transition values Round submits (R18: an informant returner, resolved in Round's slot). */
namespace RoundConnectionTransition
{
	inline constexpr uint8 InformantReadmitted = 1;
}

/**
 * The TimerFired(GraceExpiry) resolver (F4, R17.4; stories rmf-040, rmf-060): at its slot the record must still exist
 * under the captured generation and the participant must still be away; then the guarded Disconnect transition is
 * requested inline (its Roster event is evaluated as an expiry by F2 in the same slot) and, when Applied, the pawn is
 * released to the crowd in the same call stack and GraceExpired is logged. Every other Roster answer changes nothing
 * and is logged Voided(TimerFired, code), never GraceExpired.
 */
class STEALTHGAMEPLAY_API FRoundGraceResolver final : public IQueueResolver
{
public:
	explicit FRoundGraceResolver(FRoundMatchCore& InCore) : Core(InCore) {}
	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;

private:
	FRoundMatchCore& Core;
};

/** The ConnectionTransition resolver: an informant returner's re-admission inside Round's slot (R18, story rmf-062). */
class STEALTHGAMEPLAY_API FRoundConnectionResolver final : public IQueueResolver
{
public:
	explicit FRoundConnectionResolver(FRoundMatchCore& InCore) : Core(InCore) {}
	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;

private:
	FRoundMatchCore& Core;
};

/** Round's ITerminalDecision: the queue routes the staged RoundEndStep write here (ADR-0002 section 8.4). */
class STEALTHGAMEPLAY_API FRoundEndStepRunner final : public ITerminalDecision
{
public:
	explicit FRoundEndStepRunner(FRoundMatchCore& InCore) : Core(InCore) {}
	virtual void RunRoundEndStep(const FRoundEndPayload& Payload) override;

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
 * WinLatched and, for a win, computes TeamSummary and stages the round-end step (R9; stories rmf-052..055): inside a
 * queue slot one RoundEndStep write the queue binds to the deciding strike's death write or consumes inline at slot
 * end; outside a slot every win runs it at once (strikes resolve in the queue, Assassination story asn-013). Between the two steps nothing observable changes. The round-end step, in fixed order:
 * (a)+(c) FRoundResult with TeamSummary, MatchScore + 1 and phase Ended in one public write; (b) EndRoundAll; (d) held
 * NotNow replies; (e) OnRoundEnded, every viewer PostRound; (f) Crowd Halt and one stop request per live pawn bounded by
 * RunStopSec; (g) frozen holds cancel; (h) the Ended dwell counts from here, parked host commands are held. A void goes
 * Active -> Reset directly (called off). Later attempts return false and log LateWinSuppressed.
 *
 * Disconnects (R17, R18; stories rmf-060..065): Session's facts arrive through OnParticipantUnresponsive / Responsive /
 * Disconnected / Rejoined; Round requests takeovers and hand-backs through IRoundPawnControlGateway, keeps one grace
 * record per participant, converts at GraceExpiry through the queue, and re-admits an informant returner through
 * IInformantBodyProvider inside its slot. Reset (R10, story rmf-058) destroys the round's player pawns before
 * RequestLobbyTravel and enters Lobby when every participant is terminal or TravelLoadTimeoutSec has passed.
 * OnSessionEnding (story rmf-059) stops all processing with no phase change.
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
	void SetRoundQueue(FResolutionQueueCore* InQueue);
	FResolutionQueueCore* GetRoundQueue() const { return RoundQueue; }
	/** The P6 spawn site (may be set after construction; the composition root registers it with the gateways). */
	void SetStartSpawner(IRoundStartSpawner* InSpawner);
	/** The writer Round and its queue append through (forwards to the injected log and keeps this round's slice). */
	IRoundLogWriter* GetRoundLogWriter() { return &LogRecorder; }
	/** The match-scoped archive of round slices (server only; survives travel with the store). */
	const FRoundLogSliceRecorder& GetLogArchive() const { return LogRecorder; }
	/** The resolver of TimerFired(GraceExpiry) (the store registers it on the round queue). */
	IQueueResolver* GetGraceResolver() { return &GraceResolver; }
	/** The resolver of ConnectionTransition (the store registers it on the round queue). */
	IQueueResolver* GetConnectionResolver() { return &ConnectionResolver; }
	/** Round's ITerminalDecision (SetRoundQueue registers it on the round queue). */
	ITerminalDecision* GetTerminalDecision() { return &EndStepRunner; }

	/** R9 (e): fired once per round in the round-end step, after EndRoundAll and the held replies. */
	TFunction<void(const FRoundResult& /*Result*/)> OnRoundEnded;

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

	// --- Disconnect policy (R17, R18; stories rmf-060..065) ------------------------------------------------------

	/** R17.1: silence >= SessionSoftTimeoutSec. With bHandOverOnUnresponsive an Active participant's pawn is taken over. */
	void OnParticipantUnresponsive(FParticipantId Participant, double AtServerTime);
	/** R17.1: the silence ended before a disconnect: control returns by the reverse blend. */
	void OnParticipantResponsive(FParticipantId Participant);
	/** R17.2: take over at once (unless already AI-driven) and start the grace record (Active only). */
	void OnParticipantDisconnected(FParticipantId Participant, ERoundDisconnectReason Reason, double AtServerTime);
	/** R17.3 / R18: a rejoin completed (after Session's acknowledgement). Never changes the Roster state. */
	void OnParticipantRejoined(FParticipantId Participant);
	/** R18: ShouldAdmitExpired(P) = P.InRound AND Phase in {Preparing, Active, Ended}. */
	bool ShouldAdmitExpired(FParticipantId Participant) const;
	/** True while the participant's bound pawn is AI-driven by a takeover Round requested. */
	bool IsAiDriven(FParticipantId Participant) const { return AiDriven.Contains(Participant.Value); }

	// --- Session end and travel (R8, R10, R22; stories rmf-058, rmf-059) -------------------------------------------

	/** R8: a terminal event, not a phase: SessionEnded logged, processing stops, nothing released, no phase change. */
	void OnSessionEnding();
	bool HasSessionEnded() const { return bSessionEnded; }
	/** Session cancelled the round travel (after a host abort before P1, Reset runs now with 0 pawns). */
	void NotifyRoundTravelCancelled();
	/** R10 (7): a participant completed the lobby travel (terminal). */
	void NotifyParticipantLobbyTravelComplete(FParticipantId Participant);
	/** R10 (7): every participant completed the lobby travel (Session's OnLobbyTravelComplete). */
	void NotifyLobbyTravelComplete();
	/** True after a host abort while the round travel had not reported P1. */
	bool IsAbortBeforeP1Pending() const { return bAbortBeforeP1; }

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
	/** Between the latch step and the round-end step (a strike-decided win waits for its death write). */
	bool IsRoundEndPending() const { return bRoundEndPending; }
	/** The round-end step ran this round (Ended, or Reset after Ended). */
	bool HasRoundEndStepRun() const { return bRoundEndStepRan; }
	/** R9 (e): every viewer is PostRound from the round-end step on (the Visibility Policy's input). */
	ERoundViewerClass GetViewerClass(FParticipantId Viewer) const { return bRoundEndStepRan ? ERoundViewerClass::PostRound : ERoundViewerClass::InRound; }
	/** The round-end step now (the queue's terminal decision, the adapter delay, or the inline latch step). */
	void RunRoundEndStep();
	/** The queue's RoundEndStep write (the terminal key must be this round's). */
	void OnQueueRoundEndStep(const FRoundEndPayload& Payload);

	/** Objectives' resolver: the final completion re-validated (R15); TryTerminate(Win(Team, ObjectiveWin), Key). */
	bool DeclareObjectiveWin(uint8 Team, FQueueKey Key);
	/** F2 Valid(i)'s completer clauses for Completer on Team now (R13, R15; story rmf-049). */
	ERoundCompletionValidity ValidateFinalCompleter(FParticipantId Completer, uint8 Team, const FRoundCompleterControl& Control) const;
	/**
	 * The final completion with its completer (story rmf-049): valid -> DeclareObjectiveWin; otherwise one
	 * Voided(FinalCompletion, code) entry (code = ERoundCompletionValidity in D1), no latch, the round stays Active.
	 */
	bool SubmitFinalCompletion(FParticipantId Completer, uint8 Team, const FRoundCompleterControl& Control, bool bSiteLive, FQueueKey Key);
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
	friend class FRoundConnectionResolver;

	double Now() const;
	ERoundCommandResult ExecuteReset(ERoundHostCommand Command, const FRoundCommandCaller& Caller);
	/** R9 (a) TeamSummary in the latch step (Roster states before EndRoundAll; this round's KillResolved rows). */
	void ComputeTeamSummary(const FRoundTerminalOutcome& Outcome, FRoundTeamSummary OutSummary[2]) const;
	/** The rostered member, or null (a copy kept in MemberScratch). */
	const FRoundRosterMember* FindMember(FParticipantId Participant) const;
	/** Logs the WinLatched-style reason entry of a host abort (the host's id in P0). */
	void LogHostAbort(const FRoundCommandCaller& Caller);
	/** Re-admission of an informant returner (R18), from the ConnectionTransition slot or directly. */
	bool ReadmitInformant(FParticipantId Participant);
	/** Enters Preparing for a round aborted before P1 and runs the host abort (Reset with 0 pawns). */
	void RunAbortBeforeP1();
	/** Reset -> Lobby when every participant is terminal or the lobby travel bound has passed (R10 (7)). */
	bool IsLobbyExitDue() const;
	/** R10 Reset; bFromPreparing adds the Preparing-abort cleanup (pawns destroyed, deal discarded, lobby travel). */
	bool EnterReset(bool bCalledOff, ERoundEndReason Reason, uint8 AbortCode, bool bFromPreparing);
	/** The one Preparing abort path (R8, story rmf-034); bHost for the host abort (bCalledOff, EndReason Aborted). */
	void AbortPreparing(ERoundPreparingAbortCode Code, bool bHost);
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
	FRoundConnectionResolver ConnectionResolver{ *this };
	FRoundEndStepRunner EndStepRunner{ *this };

	uint32 NextAckNonce = 0;
	double ActivePhaseStart = 0.0;
	int32 ActiveEntryCount = 0;
	uint64 AdapterSeq = 0;
	/** The epoch of the round being prepared or played (captured at P6 and Active entry). */
	FEpochToken RoundEpochToken;
	TSet<uint32> KickedParticipants;
	TMap<uint32, double> LastDisconnectedAt;
	/** Participants whose bound pawn Round handed to the crowd AI (takeover requested, not handed back). */
	TSet<uint32> AiDriven;
	/** The server tick of each participant's last disconnect (the same-tick completion case). */
	TMap<uint32, uint64> DisconnectedTick;
	/** R18 InRound: rostered in the round at its start (Preparing entry, Active entry). */
	TSet<uint32> InRoundParticipants;
	/** R10 (7): participants that completed the lobby travel. */
	TSet<uint32> LobbyArrived;
	mutable TArray<FRoundRosterMember> MemberScratch;
	mutable FRoundRosterMember FoundMember;

	/** The latch step's TeamSummary, published by the round-end step. */
	FRoundTeamSummary LatchedSummary[2];
	/** The participant whose transition is being evaluated (the fallback binding's deciding victim). */
	FParticipantId DecidingParticipant;
	double ResetEnteredAt = 0.0;
	bool bRoundEndPending = false;
	bool bRoundEndStepRan = false;
	bool bApplyingDebugItem = false;
	bool bSessionEnded = false;
	bool bAbortBeforeP1 = false;
	/** The host whose abort is being run (R22: its id is logged with the abort). */
	FParticipantId AbortingHost;

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
