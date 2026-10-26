#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/Round/StealthRoundPhaseCore.h"
#include "Server/Core/Round/StealthRoundStart.h"
#include "Server/Core/Round/StealthRoundTeamRules.h"
#include "Server/Core/Round/StealthRoundTypes.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Templates/Function.h"

class FResolutionQueueCore;
class IRoundLogWriter;
class IStealthClock;

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
 * Preparing -> Active (EnterActive) and the round end (EndRound) are driven by the composition root until the
 * Preparing barrier (rmf-032..041) and the latch (rmf-043..053) stories land.
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

	/** Session accepted the round travel: the phase becomes Preparing (step 9). False when no travel was pending. */
	bool NotifyRoundTravelAccepted();

	// --- Phase drivers until the Preparing / latch stories land -----------------------------------------------------

	/** Preparing -> Active. */
	bool EnterActive();
	/** Active -> Ended with the public result (WinningTeam 0/1, PublicReason 1 objective, 2 elimination). */
	bool EndRound(uint8 WinningTeam, uint8 PublicReason);

	// --- Host commands (R22) ----------------------------------------------------------------------------------------

	ERoundCommandResult ExecuteHostCommand(const FRoundCommandCaller& Caller, ERoundHostCommand Command, const FRoundCommandArgs& Args = FRoundCommandArgs());

	/** Runs a held reset when the Ended dwell has elapsed, and completes Reset -> Lobby. Call once per server frame. */
	void Tick();

private:
	double Now() const;
	ERoundCommandResult ExecuteReset(ERoundHostCommand Command);
	bool EnterReset(bool bCalledOff);
	ERoundCommandResult SubmitDebugItem(ERoundHostCommand Command, const FRoundCommandArgs& Args);
	FRoundStartInputs GatherStartInputs(bool bIsHost) const;
	void ReportStartRefusal(const FStartRefusal& Refusal);
	void LogStartRolledBack();

	FRoundGateways Gateways;
	IRoundHostSink* HostSink = nullptr;
	IRoundLogWriter* Log = nullptr;
	const IStealthClock* Clock = nullptr;
	FResolutionQueueCore* DebugQueue = nullptr;
	TFunction<void()> DumpLogHandler;

	FRoundRuleParams Params;
	FRoundPhaseCore PhaseCore;
	FRoundTeamRules TeamRules;

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
