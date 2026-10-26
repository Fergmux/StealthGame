#include "Server/Core/Round/StealthRoundMatchCore.h"

#include "Server/Core/Round/StealthCrowdControl.h"
#include "Server/EventLog/StealthRoundLogTypes.h"
#include "Server/EventLog/StealthRoundLogWriter.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/StealthClock.h"
#include "View/CommandReplyTypes.h"

namespace RoundMatchCoreLocal
{
	int32 TeamIndex(ERosterTeam Team)
	{
		return Team == ERosterTeam::Team0 ? 0 : (Team == ERosterTeam::Team1 ? 1 : INDEX_NONE);
	}
}

FRoundMatchCore::FRoundMatchCore(const FRoundGateways& InGateways, IRoundHostSink* InHostSink, IRoundLogWriter* InLog, const IStealthClock* InClock)
	: Gateways(InGateways)
	, HostSink(InHostSink)
	, Clock(InClock)
	, PhaseCore(InGateways.Session)
	, TeamRules(*InGateways.Roster, InHostSink)
	, Preparing(InGateways, InClock)
{
	check(Gateways.Session != nullptr && Gateways.Roster != nullptr);
	LogRecorder.SetDownstream(InLog);

	FRoundPreparingSequencer::FCallbacks Callbacks;
	Callbacks.OnAbort = [this](ERoundPreparingAbortCode Code) { AbortPreparing(Code, /*bHost*/ false); };
	Callbacks.OnReadyForActive = [this]() { EnterActive(); };
	Callbacks.OnPublicFields = [this](int32 Expected, double ActiveAt) { PhaseCore.SetPreparingPublicFields(Expected, ActiveAt); };
	Callbacks.OnWatchdogWarning = [this](const StealthRoundFormulas::FWatchdogBound& Bound)
	{
		if (OnWatchdogWarning)
		{
			OnWatchdogWarning(Bound);
		}
	};
	Callbacks.OnWriteHeader = [this](uint64 TuningHash)
	{
		if (OnWriteLogHeader)
		{
			OnWriteLogHeader(TuningHash);
		}
	};
	Preparing.SetCallbacks(MoveTemp(Callbacks));
}

void FRoundMatchCore::SetStartSpawner(IRoundStartSpawner* InSpawner)
{
	Gateways.Spawner = InSpawner;
	Preparing.SetGateways(Gateways);
}

double FRoundMatchCore::Now() const
{
	return Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0;
}

void FRoundMatchCore::LogEntry(FRoundLogEntry& Entry)
{
	Entry.ServerTime = Now();
	Entry.RoundEpoch = RoundEpochToken.RoundEpoch;
	LogRecorder.Append(Entry);
}

// --- Match ---------------------------------------------------------------------------------------------------------

ERosterResult FRoundMatchCore::OpenMatch()
{
	const ERosterResult Result = Gateways.Roster->BeginMatch(Params.TeamSize);
	bTeamsLocked = false;
	bTravelPending = false;
	bResetHeld = false;
	bResetPendingLobby = false;
	Preparing.Cancel();
	Latch.Reset();
	Grace.Reset();
	KickedParticipants.Reset();
	LastDisconnectedAt.Reset();
	LogRecorder.ResetForNewMatch();
	ActivePhaseStart = 0.0;
	ActiveEntryCount = 0;
	RoundEpochToken = FEpochToken();
	// A second match on the same game instance (re-hosting) starts from a fresh Lobby: nothing carries over.
	PhaseCore.ResetForNewMatch();
	PhaseCore.RequestPhase(ERoundPhase::Lobby, Now());
	return Result;
}

ERosterTeam FRoundMatchCore::OnParticipantRegistered(FParticipantId Id)
{
	return TeamRules.OnParticipantRegistered(Id, PhaseCore.GetPhase(), PhaseCore.GetRoundsPlayedCount());
}

// --- Start ---------------------------------------------------------------------------------------------------------

FRoundStartInputs FRoundMatchCore::GatherStartInputs(bool bIsHost) const
{
	FRoundStartInputs Inputs;
	Inputs.bIsHost = bIsHost;
	Inputs.Phase = PhaseCore.GetPhase();
	Inputs.bTravelInProgress = bTravelPending;
	Inputs.Params = Params;
	// Checks 1 and 2 need no gateway: a refused caller or phase reads nothing else.
	if (!bIsHost || Inputs.Phase != ERoundPhase::Lobby || bTravelPending)
	{
		return Inputs;
	}
	TArray<FRoundRosterMember> Members;
	Gateways.Roster->GetRosteredParticipants(Members);
	for (const FRoundRosterMember& Member : Members)
	{
		FRoundStartMemberState& State = Inputs.Members.AddDefaulted_GetRef();
		State.Participant = Member.Participant;
		State.Team = Member.Team;
		State.Connection = Gateways.Session->GetConnectionState(Member.Participant);
	}
	Inputs.PresetPreview = Gateways.Tuning != nullptr ? Gateways.Tuning->PreviewPreset(Params.PresetId) : ERoundPresetPreview::Unavailable;
	return Inputs;
}

void FRoundMatchCore::ReportStartRefusal(const FStartRefusal& Refusal)
{
	if (HostSink != nullptr)
	{
		HostSink->OnStartRefused(Refusal);
	}
}

void FRoundMatchCore::LogStartRolledBack()
{
	FRoundLogEntry Entry;
	Entry.Type = ERoundLogType::StartRolledBack;
	Entry.Code = static_cast<uint16>(ERoundStartRefusalCode::SessionRefused);
	Entry.Data[0] = PhaseCore.GetRoundsPlayedCount() + 1;
	LogEntry(Entry);
}

FStartRefusal FRoundMatchCore::Start(const FRoundCommandCaller& Caller)
{
	const FStartRefusal Refusal = FRoundStartValidator::Validate(GatherStartInputs(Caller.IsHost()));
	if (Refusal.IsRefused())
	{
		ReportStartRefusal(Refusal);
		return Refusal;
	}

	// (5b) A side-effect-free query: false refuses before anything changes.
	if (!Gateways.Session->CanRequestRoundTravel())
	{
		const FStartRefusal Session = FStartRefusal::Make(ERoundStartRefusalCode::SessionRefused);
		ReportStartRefusal(Session);
		return Session;
	}

	// (6) Admission lock, (7) the once-per-round roster reset (before travel: Session rule 11.2), (8) travel.
	const uint32 RoundIndex = PhaseCore.GetRoundsPlayedCount() + 1;
	Gateways.Session->SetAdmissionMode(ERoundAdmissionMode::RejoinOnly);
	const ERosterResult Locked = Gateways.Roster->SetTeamsLocked(true);
	bTeamsLocked = true;
	const ERosterResult Reset = (Locked == ERosterResult::Applied || Locked == ERosterResult::NoOp_Unchanged)
		? Gateways.Roster->ResetAll(RoundIndex) : Locked;
	PhaseCore.SetCurrentRoundIndex(RoundIndex);
	bTravelPending = true;
	// A Roster refusal of the lock or the reset rolls back like a travel refusal (no travel against stale state).
	if (Reset != ERosterResult::Applied
		|| Gateways.Session->RequestRoundTravel(Params.MapId, Params.PresetId, RoundIndex) != ERoundTravelRequestResult::Accepted)
	{
		// Rollback (R5): admission Open, teams unlocked, phase stays Lobby; the reset round scope is left as it is.
		bTravelPending = false;
		Gateways.Session->SetAdmissionMode(ERoundAdmissionMode::Open);
		Gateways.Roster->SetTeamsLocked(false);
		bTeamsLocked = false;
		PhaseCore.SetCurrentRoundIndex(PhaseCore.GetRoundsPlayedCount());
		LogStartRolledBack();
		const FStartRefusal Session = FStartRefusal::Make(ERoundStartRefusalCode::SessionRefused);
		ReportStartRefusal(Session);
		return Session;
	}
	return FStartRefusal::Accepted();
}

bool FRoundMatchCore::NotifyRoundTravelAccepted(int32 Seed)
{
	if (!bTravelPending)
	{
		return false;
	}
	bTravelPending = false;
	if (!PhaseCore.RequestPhase(ERoundPhase::Preparing, Now()))
	{
		return false;
	}
	// R6: the readiness sequence with this Preparing's acknowledgement nonce (stale acks of earlier rounds drop).
	Latch.Reset();
	RoundEpochToken = FEpochToken();
	Grace.Reset();
	KickedParticipants.Reset();
	Preparing.Begin(Params, Seed, FRoundAckEpoch{ ++NextAckNonce });
	return true;
}

void FRoundMatchCore::OnParticipantConnectionChanged(FParticipantId Participant, ERoundConnectionState State, bool bKicked, double AtServerTime)
{
	if (!Participant.IsValid())
	{
		return;
	}
	if (State == ERoundConnectionState::Connected)
	{
		LastDisconnectedAt.Remove(Participant.Value);
		Preparing.OnParticipantConnected(Participant);
		if (PhaseCore.GetPhase() == ERoundPhase::Active && !Latch.IsTerminated())
		{
			// A rejoin inside grace keeps the Active role (R17.3): the record goes; its timer finds none and voids.
			Grace.Close(Participant);
		}
		return;
	}
	if (State != ERoundConnectionState::Disconnected)
	{
		return;
	}
	LastDisconnectedAt.Add(Participant.Value, AtServerTime);
	if (bKicked)
	{
		KickedParticipants.Add(Participant.Value);
	}
	// Ended/latched: a disconnect changes only the connection (R9). Preparing: the record is made at Active entry.
	if (PhaseCore.GetPhase() == ERoundPhase::Active && !Latch.IsTerminated())
	{
		if (Grace.Find(Participant) != nullptr)
		{
			// R17.2: a kick on a live record starts no second timer; the record is marked kicked, its expiry kept.
			Grace.MarkKicked(Participant, bKicked);
			return;
		}
		const bool bKnownEpoch = RoundEpochToken.MatchId != 0 || RoundEpochToken.RoundEpoch != 0;
		OpenGraceRecord(Participant, AtServerTime, bKnownEpoch ? RoundEpochToken : Gateways.Roster->GetEpochToken());
	}
}

// --- Active entry --------------------------------------------------------------------------------------------------

void FRoundMatchCore::OpenGraceRecord(FParticipantId Participant, double DisconnectedAt, const FEpochToken& Epoch)
{
	FRoundGraceRecord Record;
	Record.Participant = Participant;
	Record.GraceExpiry = StealthRoundFormulas::GraceExpiry(DisconnectedAt, ActivePhaseStart,
		StealthRoundFormulas::EffectiveGraceSec(Params.DisconnectGraceSec, Params.RejoinWindowSec));
	Record.ExpectedEpoch = Epoch;
	Record.ExpectedConnectionGeneration = Gateways.Session->GetConnectionGeneration(Participant);
	Record.bKicked = KickedParticipants.Contains(Participant.Value);
	if (RoundQueue != nullptr)
	{
		Record.TimerHandleId = RoundQueue->ScheduleAt(Record.GraceExpiry, FRoundGraceTable::MakeTimerItem(Record)).Id;
	}
	Grace.Open(Record);
}

bool FRoundMatchCore::EnterActive()
{
	if (PhaseCore.GetPhase() != ERoundPhase::Preparing)
	{
		return false;
	}
	// R7 / story rmf-041: Roster invariant I4 immediately before the step; a non-empty set is a defect and aborts
	// BindFailed with nothing of Active entry done (no stamp, no count, no queue, no OnRoundActive, no Crowd start).
	TArray<FParticipantId> Unbound;
	Gateways.Roster->GetUnboundActiveParticipants(Unbound);
	if (Unbound.Num() > 0)
	{
		AbortPreparing(ERoundPreparingAbortCode::BindFailed, /*bHost*/ false);
		return false;
	}

	// The one step (story rmf-042). State first: ActivePhaseStart, the queue opened (Seq restarts at 1), the latch clear.
	ActivePhaseStart = Now();
	Latch.Reset();
	if (RoundQueue != nullptr)
	{
		RoundQueue->ResetForNewRound();
	}
	// The phase write: ActiveAtServerTime 0 in FRoundPublicState, RoundsPlayedCount + 1 (RoundIndex is now this round).
	if (!PhaseCore.RequestPhase(ERoundPhase::Active, ActivePhaseStart))
	{
		return false;
	}
	// F4 / R17: every participant Session has Disconnected (or Dormant) at ActivePhaseStart converts at
	// max(DisconnectedAt, ActivePhaseStart) + G_eff, with the generation current now (a rejoin and re-drop in
	// Preparing leaves no stale timer; a drop between P6 and Active is included).
	RoundEpochToken = Preparing.GetSpawnEpoch();
	TArray<FRoundRosterMember> Members;
	Gateways.Roster->GetRosteredParticipants(Members);
	for (const FRoundRosterMember& Member : Members)
	{
		const ERoundConnectionState State = Gateways.Session->GetConnectionState(Member.Participant);
		if (State != ERoundConnectionState::Disconnected && State != ERoundConnectionState::Dormant)
		{
			continue;
		}
		if (RoundEpochToken.MatchId == 0 && RoundEpochToken.RoundEpoch == 0)
		{
			RoundEpochToken = Gateways.Roster->GetEpochToken();
		}
		const double* At = LastDisconnectedAt.Find(Member.Participant.Value);
		OpenGraceRecord(Member.Participant, At != nullptr ? *At : ActivePhaseStart, RoundEpochToken);
	}
	++ActiveEntryCount;
	if (OnRoundActive)
	{
		OnRoundActive(RoundEpochToken.RoundEpoch, ActivePhaseStart);
	}
	if (Gateways.Crowd != nullptr)
	{
		Gateways.Crowd->StartRound();
	}
	return true;
}

TOptional<ECommandRejectCode> FRoundMatchCore::AdmitCommand() const
{
	// R6: all commands are rejected until Active, human and AI alike. After the latch the queue answers Rejected_Latched.
	if (PhaseCore.GetPhase() != ERoundPhase::Active || Latch.IsTerminated())
	{
		return ECommandRejectCode::NotNow;
	}
	return TOptional<ECommandRejectCode>();
}

// --- Wins and the latch --------------------------------------------------------------------------------------------

FQueueKey FRoundMatchCore::DecisionKey()
{
	if (RoundQueue != nullptr && RoundQueue->IsInSlot())
	{
		return RoundQueue->CurrentKey();
	}
	const uint64 Tick = Clock != nullptr ? Clock->GetServerTickIndex() : 0;
	return FQueueKey(Tick, AdapterKeySeqBase + (++AdapterSeq));
}

bool FRoundMatchCore::TryTerminate(const FRoundTerminalOutcome& Outcome, FQueueKey Key, bool bAlreadyLogged)
{
	if (!Latch.TryTerminate(Outcome, Key, PhaseCore.GetPhase()))
	{
		FRoundLogEntry Late;
		Late.Type = ERoundLogType::LateWinSuppressed;
		Late.Key = Key;
		Late.Code = static_cast<uint16>(Outcome.EndReason);
		Late.Data[0] = Outcome.IsWin() ? Outcome.WinningTeam : GRoundLogNoTeam;
		LogEntry(Late);
		return false;
	}
	// The latch step: closes intake (every later item returns Rejected_Latched) in the same call.
	if (RoundQueue != nullptr && !RoundQueue->IsLatched())
	{
		RoundQueue->Latch(Key);
	}
	if (!bAlreadyLogged)
	{
		FRoundLogEntry Entry;
		Entry.Type = ERoundLogType::WinLatched;
		Entry.Key = Key;
		Entry.Code = static_cast<uint16>(Outcome.EndReason);
		Entry.Data[0] = Outcome.IsWin() ? Outcome.WinningTeam : GRoundLogNoTeam;
		LogEntry(Entry);
	}
	if (Outcome.IsWin())
	{
		// No strike staging yet (story rmf-053): the round-end step is the latch step.
		RunRoundEndStep(Outcome);
	}
	else
	{
		// R8 void: Active -> Reset directly, no winner, no score, called off.
		EnterReset(/*bCalledOff*/ true, Outcome.EndReason, 0, /*bFromPreparing*/ false);
	}
	return true;
}

void FRoundMatchCore::RunRoundEndStep(const FRoundTerminalOutcome& Outcome)
{
	FRoundResult Result;
	Result.WinningTeam = Outcome.WinningTeam;
	Result.PublicReason = Outcome.GetPublicReason();
	Result.RoundIndex = PhaseCore.GetCurrentRoundIndex();
	PhaseCore.SetLastResult(Result);
	PhaseCore.SetCalledOff(false);
	EndedAtServerTime = Now();
	PhaseCore.RequestPhase(ERoundPhase::Ended, EndedAtServerTime);
}

bool FRoundMatchCore::DeclareObjectiveWin(uint8 Team, FQueueKey Key)
{
	return Team <= 1 && TryTerminate(FRoundTerminalOutcome::Win(Team, ERoundEndReason::ObjectiveWin), Key);
}

bool FRoundMatchCore::SubmitObjectiveCompletion(const FRoundObjectiveCompletion& Completion)
{
	const TOptional<FRoundTerminalOutcome> Outcome = StealthRoundWin::EvaluateObjectiveCompletion(Completion, MakeWinContext());
	return Outcome.IsSet() && DeclareObjectiveWin(Outcome->WinningTeam, DecisionKey());
}

FRoundWinContext FRoundMatchCore::MakeWinContext() const
{
	FRoundWinContext Context;
	Context.Phase = PhaseCore.GetPhase();
	Context.bTerminated = Latch.IsTerminated();
	TArray<FRoundRosterMember> Members;
	Gateways.Roster->GetRosteredParticipants(Members);
	for (const FRoundRosterMember& Member : Members)
	{
		const int32 Team = RoundMatchCoreLocal::TeamIndex(Member.Team);
		if (Team == INDEX_NONE)
		{
			continue;
		}
		++Context.RosterCount[Team];
		if (Member.State == EParticipantState::Active && Gateways.Session->GetConnectionState(Member.Participant) == ERoundConnectionState::Connected)
		{
			++Context.ConnectedActiveCount[Team];
		}
	}
	return Context;
}

void FRoundMatchCore::OnRosterTransition(const FRoundTransitionEvent& InEvent)
{
	if (PhaseCore.GetPhase() != ERoundPhase::Active || InEvent.bBatch)
	{
		return; // batch events are never evaluated (F2 proof item 5)
	}
	FRoundTransitionEvent Event = InEvent;
	Event.bKicked = Event.bKicked || KickedParticipants.Contains(Event.Participant.Value);
	const FQueueKey Key = DecisionKey();
	if (Event.bApplied)
	{
		// R19 StateTransition (TeamSummary's EliminatedCount reads these): P0, Code reason, D0..D2, D4 packed counts.
		FRoundLogEntry Entry;
		Entry.Type = ERoundLogType::StateTransition;
		Entry.Key = Key;
		Entry.Code = static_cast<uint16>(Event.Reason);
		Entry.Participants[0] = Event.Participant.Value;
		Entry.Data[0] = static_cast<uint32>(Event.Old);
		Entry.Data[1] = static_cast<uint32>(Event.New);
		Entry.Data[2] = static_cast<uint32>(Event.Team);
		Entry.Data[4] = (static_cast<uint32>(Event.ActiveCountAfter[0]) & 0xFFFF) | (static_cast<uint32>(Event.ActiveCountAfter[1]) << 16);
		LogEntry(Entry);
	}
	if (Latch.IsTerminated())
	{
		return;
	}
	const TOptional<FRoundTerminalOutcome> Outcome = StealthRoundWin::EvaluateTransition(Event, MakeWinContext());
	if (Outcome.IsSet())
	{
		TryTerminate(Outcome.GetValue(), Key);
	}
}

bool FRoundMatchCore::EndRound(uint8 WinningTeam, uint8 PublicReason)
{
	if (PhaseCore.GetPhase() != ERoundPhase::Active || WinningTeam > 1)
	{
		return false;
	}
	return TryTerminate(FRoundTerminalOutcome::Win(WinningTeam, PublicReason == 1 ? ERoundEndReason::ObjectiveWin : ERoundEndReason::Elimination), DecisionKey());
}

void FRoundMatchCore::ApplyDebugItem(const FDebugPayload& Payload, FQueueKey Key)
{
	if (Payload.Command == FRoundDebugResolver::CommandDebugWin)
	{
		// The resolver already wrote the Debug-flagged WinLatched entry.
		if (Payload.Arg <= 1)
		{
			TryTerminate(FRoundTerminalOutcome::Win(static_cast<uint8>(Payload.Arg), ERoundEndReason::DebugForced), Key, /*bAlreadyLogged*/ true);
		}
		return;
	}
	if (Payload.Command == FRoundDebugResolver::CommandDebugEliminate && Payload.Subject.Participant.IsValid())
	{
		// A real Roster transition inside the slot: its event reaches OnRosterTransition before the slot ends.
		Gateways.Roster->RequestTransition(Payload.Subject.Participant, EParticipantState::Informant, ERosterReason::Assassinated,
			Gateways.Roster->GetEpochToken(), TOptional<uint32>());
	}
}

// --- Reset and aborts ----------------------------------------------------------------------------------------------

bool FRoundMatchCore::EnterReset(bool bCalledOff, ERoundEndReason Reason, uint8 AbortCode, bool bFromPreparing)
{
	const bool bReachedEnded = PhaseCore.GetPhase() == ERoundPhase::Ended;
	if (!FRoundPhaseCore::IsLegalTransition(PhaseCore.GetPhase(), ERoundPhase::Reset))
	{
		return false;
	}
	// bCalledOff is part of the same public write as the phase, so listeners of Reset already see it.
	PhaseCore.SetCalledOff(bCalledOff);
	if (Preparing.GetSpawnEpoch().RoundEpoch != 0 && RoundEpochToken.RoundEpoch == 0)
	{
		RoundEpochToken = Preparing.GetSpawnEpoch();
	}
	// (1) admission Closed (the phase write pushes it).
	PhaseCore.RequestPhase(ERoundPhase::Reset, Now());
	// (2) every Round timer cancelled: grace records go (their queued items find none), the sequence stops.
	Preparing.Cancel();
	Grace.Reset();
	// (3) the slice archived server-side, the round buffer cleared.
	LogRecorder.ArchiveCurrent(PhaseCore.GetCurrentRoundIndex(), RoundEpochToken.RoundEpoch, Reason, AbortCode, bReachedEnded);
	// (5) round scope (R21): Terminated and the queue (Seq restarts at 1) reset; a Reset decided inside a slot (a void)
	// leaves the latched queue to the next Active entry, which opens it again.
	Latch.Reset();
	if (RoundQueue != nullptr && !RoundQueue->IsInSlot())
	{
		RoundQueue->ResetForNewRound();
	}
	if (bFromPreparing)
	{
		// (4) every player pawn of the round destroyed (0 pawns after an abort), (5) the deal discarded, (6) lobby travel.
		if (Gateways.Spawner != nullptr)
		{
			Gateways.Spawner->DestroyRoundPawns();
		}
		if (Gateways.CharacterPool != nullptr)
		{
			Gateways.CharacterPool->DiscardDeal();
		}
		Gateways.Session->RequestLobbyTravel();
	}
	bResetHeld = false;
	// (7) Reset -> Lobby on the next tick (Session reports every participant terminal; no travel exists yet).
	bResetPendingLobby = true;
	return true;
}

void FRoundMatchCore::AbortPreparing(ERoundPreparingAbortCode Code, bool bHost)
{
	if (PhaseCore.GetPhase() != ERoundPhase::Preparing)
	{
		return;
	}
	Preparing.Cancel();
	const ERoundEndReason Reason = bHost ? ERoundEndReason::Aborted : ERoundEndReason::PreparingFailed;
	// The reason is logged first; the queue is not open in Preparing, so nothing is latched (no Latch call).
	FRoundLogEntry Entry;
	Entry.Type = ERoundLogType::WinLatched;
	Entry.Code = static_cast<uint16>(Reason);
	Entry.Data[0] = GRoundLogNoTeam;
	Entry.Data[1] = static_cast<uint32>(Code);
	LogEntry(Entry);
	// No result, no score change; RoundsPlayedCount advances only at Active entry, so it is unchanged.
	EnterReset(/*bCalledOff*/ bHost, Reason, static_cast<uint8>(Code), /*bFromPreparing*/ true);
}

// --- Host commands -------------------------------------------------------------------------------------------------

ERoundCommandResult FRoundMatchCore::ExecuteReset(ERoundHostCommand Command)
{
	switch (PhaseCore.GetPhase())
	{
	case ERoundPhase::Preparing:
		// R8: the host abort in Preparing takes the one abort path (story rmf-034).
		AbortPreparing(ERoundPreparingAbortCode::None, /*bHost*/ true);
		return PhaseCore.GetPhase() == ERoundPhase::Reset ? ERoundCommandResult::Executed : ERoundCommandResult::Refused;
	case ERoundPhase::Active:
		if (!Params.bHostMayAbortActive)
		{
			if (HostSink != nullptr)
			{
				HostSink->OnCommandRefused(Command, ERoundCommandResult::WrongPhase, 0);
			}
			return ERoundCommandResult::WrongPhase;
		}
		if (Latch.IsTerminated())
		{
			// R9 (v): a host abort after the latch is held like an early reset.
			bResetHeld = true;
			return ERoundCommandResult::Held;
		}
		return EnterReset(/*bCalledOff*/ true, ERoundEndReason::Aborted, 0, /*bFromPreparing*/ false) ? ERoundCommandResult::Executed : ERoundCommandResult::Refused;
	case ERoundPhase::Ended:
		if (Now() - EndedAtServerTime >= Params.EndedMinDwellSec)
		{
			return EnterReset(/*bCalledOff*/ false, Latch.GetOutcome().EndReason, 0, /*bFromPreparing*/ false) ? ERoundCommandResult::Executed : ERoundCommandResult::Refused;
		}
		// R9: an earlier reset is parked and executed at the dwell's end.
		bResetHeld = true;
		return ERoundCommandResult::Held;
	default:
		// Lobby and Reset: nothing to reset.
		return ERoundCommandResult::NoOp;
	}
}

ERoundCommandResult FRoundMatchCore::SubmitDebugItem(ERoundHostCommand Command, const FRoundCommandArgs& Args)
{
#if UE_BUILD_SHIPPING
	return ERoundCommandResult::Unavailable;
#else
	if (PhaseCore.GetPhase() != ERoundPhase::Active)
	{
		return ERoundCommandResult::WrongPhase;
	}
	if (DebugQueue == nullptr)
	{
		return ERoundCommandResult::Unavailable;
	}
	FDebugPayload Payload;
	if (Command == ERoundHostCommand::DebugWin)
	{
		Payload.Command = FRoundDebugResolver::CommandDebugWin;
		Payload.Arg = static_cast<uint32>(Args.Team);
	}
	else
	{
		Payload.Command = FRoundDebugResolver::CommandDebugEliminate;
		Payload.Subject = FSubjectRef::ForParticipant(Args.Participant);
	}
	DebugQueue->Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::DebugItem, Gateways.Roster->GetEpochToken(), FMailboxId::ServerLocal(), Payload));
	return ERoundCommandResult::Executed;
#endif
}

ERoundCommandResult FRoundMatchCore::ExecuteHostCommand(const FRoundCommandCaller& Caller, ERoundHostCommand Command, const FRoundCommandArgs& Args)
{
	if (!Caller.IsHost())
	{
		// R2/R22: a non-host gets NotHost and no effect (and nothing reaches the host sink: it is not the host's action).
		return ERoundCommandResult::NotHost;
	}
	const ERoundPhase Phase = PhaseCore.GetPhase();
	switch (Command)
	{
	case ERoundHostCommand::Start:
		return Start(Caller).IsRefused() ? ERoundCommandResult::Refused : ERoundCommandResult::Executed;

	case ERoundHostCommand::Reset:
	case ERoundHostCommand::Abort:
		// abort is an alias of the abort forms of reset.
		return ExecuteReset(Command);

	case ERoundHostCommand::SwapTeam:
		return Args.Other.IsValid() ? TeamRules.ExchangeTeams(Args.Participant, Args.Other, Phase) : TeamRules.SwapTeam(Args.Participant, Phase, Params.TeamSize);

	case ERoundHostCommand::Remove:
		return TeamRules.RemoveParticipant(Args.Participant, Phase);

	case ERoundHostCommand::Preset:
	{
		const ERoundCommandResult Result = TeamRules.ApplyPresetTeamSize(Params.TeamSize, Args.PresetTeamSize, bTeamsLocked, Phase);
		if (Result == ERoundCommandResult::Executed || Result == ERoundCommandResult::NoOp)
		{
			Params.PresetId = Args.PresetId;
		}
		return Result;
	}

	case ERoundHostCommand::DebugWin:
	case ERoundHostCommand::DebugEliminate:
		return SubmitDebugItem(Command, Args);

	case ERoundHostCommand::DumpLog:
#if UE_BUILD_SHIPPING
		return ERoundCommandResult::Unavailable;
#else
		// ADR-0007: the log never leaves the server before Ended; a refusal produces no output.
		if (Phase != ERoundPhase::Ended && Phase != ERoundPhase::Reset)
		{
			return ERoundCommandResult::WrongPhase;
		}
		if (DumpLogHandler)
		{
			DumpLogHandler();
		}
		return ERoundCommandResult::Executed;
#endif

	default:
		return ERoundCommandResult::Refused;
	}
}

void FRoundMatchCore::Tick()
{
	const ERoundPhase Phase = PhaseCore.GetPhase();
	if (Phase == ERoundPhase::Preparing)
	{
		Preparing.Tick();
		return;
	}
	if (bResetHeld && Phase == ERoundPhase::Ended && Now() - EndedAtServerTime >= Params.EndedMinDwellSec)
	{
		EnterReset(/*bCalledOff*/ false, Latch.GetOutcome().EndReason, 0, /*bFromPreparing*/ false);
		return;
	}
	if (bResetPendingLobby && Phase == ERoundPhase::Reset)
	{
		// R10 Reset completes into Lobby: teams unlock (they change only in Lobby); ResetAll runs at the next Start.
		bResetPendingLobby = false;
		Gateways.Roster->SetTeamsLocked(false);
		bTeamsLocked = false;
		PhaseCore.RequestPhase(ERoundPhase::Lobby, Now());
	}
}

// --- Grace resolver ------------------------------------------------------------------------------------------------

EResolveResult FRoundGraceResolver::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	const FTimerPayload* Payload = Item.Payload.TryGet<FTimerPayload>();
	if (Payload == nullptr || Payload->Kind != ETimerKind::GraceExpiry || Payload->Subject.Kind != ESubjectRefKind::Participant)
	{
		return EResolveResult::Voided;
	}
	const FParticipantId Participant = Payload->Subject.Participant;
	const FRoundGraceRecord* Record = Core.Grace.Find(Participant);
	// The generation guard: a rejoin (record closed) or a newer disconnect (newer generation) voids an older timer.
	if (Record == nullptr || Record->ExpectedConnectionGeneration != Payload->Generation
		|| Core.Gateways.Session->GetConnectionState(Participant) == ERoundConnectionState::Connected)
	{
		return EResolveResult::Voided;
	}
	const FRoundGraceRecord Copy = *Record;
	Core.Grace.Close(Participant);
	// The guarded Disconnect transition, inline: F2 evaluates its event as an expiry (or a kick) in this slot.
	Core.Gateways.Roster->RequestTransition(Participant, EParticipantState::Informant, ERosterReason::Disconnect, Copy.ExpectedEpoch,
		TOptional<uint32>(Copy.ExpectedConnectionGeneration));
	return EResolveResult::Executed;
}

// --- Debug resolver ------------------------------------------------------------------------------------------------

EResolveResult FRoundDebugResolver::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	const FDebugPayload* Payload = Item.Payload.TryGet<FDebugPayload>();
	if (Payload == nullptr)
	{
		return EResolveResult::Voided;
	}
	FRoundLogEntry Entry;
	Entry.Key = Key;
	Entry.ServerTime = Clock.GetServerTimeSeconds();
	Entry.RoundEpoch = Item.Expected.RoundEpoch;
	Entry.Flags = RoundLogFlags::Debug;
	Entry.SourceMailbox = Item.Source;
	if (Payload->Command == CommandDebugWin)
	{
		Entry.Type = ERoundLogType::WinLatched;
		Entry.Code = static_cast<uint16>(ERoundEndReason::DebugForced);
		Entry.Data[0] = Payload->Arg;
	}
	else
	{
		Entry.Type = ERoundLogType::StateTransition;
		Entry.Code = static_cast<uint16>(ERosterReason::Assassinated);
		Entry.Participants[0] = Payload->Subject.Participant.Value;
		Entry.Data[0] = static_cast<uint32>(EParticipantState::Active);
		Entry.Data[1] = static_cast<uint32>(EParticipantState::Informant);
	}
	Log.Append(Entry);
	if (OnDebug)
	{
		OnDebug(*Payload, Key);
	}
	return EResolveResult::Executed;
}
