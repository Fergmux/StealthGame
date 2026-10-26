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

void FRoundMatchCore::SetRoundQueue(FResolutionQueueCore* InQueue)
{
	if (RoundQueue != nullptr && RoundQueue != InQueue && RoundQueue->GetTerminalDecision() == &EndStepRunner)
	{
		RoundQueue->SetTerminalDecision(nullptr);
	}
	RoundQueue = InQueue;
	if (RoundQueue != nullptr)
	{
		// ADR-0002 section 8.4: the queue routes the staged RoundEndStep write to Round's handler.
		RoundQueue->SetTerminalDecision(&EndStepRunner);
	}
}

const FRoundRosterMember* FRoundMatchCore::FindMember(FParticipantId Participant) const
{
	Gateways.Roster->GetRosteredParticipants(MemberScratch);
	const FRoundRosterMember* Member = MemberScratch.FindByPredicate([Participant](const FRoundRosterMember& M) { return M.Participant == Participant; });
	if (Member == nullptr)
	{
		return nullptr;
	}
	FoundMember = *Member;
	return &FoundMember;
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
	AiDriven.Reset();
	InRoundParticipants.Reset();
	LobbyArrived.Reset();
	bRoundEndPending = false;
	bRoundEndStepRan = false;
	RoundEndDueTime = -1.0;
	bSessionEnded = false;
	bAbortBeforeP1 = false;
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
	if (!bTravelPending || bSessionEnded)
	{
		return false;
	}
	bTravelPending = false;
	if (bAbortBeforeP1)
	{
		// Edge Cases: the host aborted while travelling; at P1 Reset runs at once and no pawn is spawned.
		RunAbortBeforeP1();
		return true;
	}
	if (!PhaseCore.RequestPhase(ERoundPhase::Preparing, Now()))
	{
		return false;
	}
	// R6: the readiness sequence with this Preparing's acknowledgement nonce (stale acks of earlier rounds drop).
	Latch.Reset();
	RoundEpochToken = FEpochToken();
	Grace.Reset();
	KickedParticipants.Reset();
	AiDriven.Reset();
	bRoundEndPending = false;
	bRoundEndStepRan = false;
	RoundEndDueTime = -1.0;
	LatchedSummary[0] = LatchedSummary[1] = FRoundTeamSummary();
	// R18 InRound: the records the round's ResetAll put in the round (Dormant ones stay RoundEnded).
	InRoundParticipants.Reset();
	Gateways.Roster->GetRosteredParticipants(MemberScratch);
	for (const FRoundRosterMember& Member : MemberScratch)
	{
		if (Member.State != EParticipantState::RoundEnded)
		{
			InRoundParticipants.Add(Member.Participant.Value);
		}
	}
	Preparing.Begin(Params, Seed, FRoundAckEpoch{ ++NextAckNonce });
	return true;
}

void FRoundMatchCore::RunAbortBeforeP1()
{
	bAbortBeforeP1 = false;
	Latch.Reset();
	if (PhaseCore.RequestPhase(ERoundPhase::Preparing, Now()))
	{
		// The readiness sequence never begins: no tuning signal, no deal, no spawn (0 pawns).
		AbortPreparing(ERoundPreparingAbortCode::None, /*bHost*/ true);
	}
}

void FRoundMatchCore::NotifyRoundTravelCancelled()
{
	if (!bTravelPending || bSessionEnded)
	{
		return;
	}
	bTravelPending = false;
	if (bAbortBeforeP1)
	{
		// R22: the cancel the abort asked for; Reset runs now (0 pawns).
		RunAbortBeforeP1();
		return;
	}
	// A Session-initiated cancel without an abort rolls back like a refused travel (R5).
	Gateways.Session->SetAdmissionMode(ERoundAdmissionMode::Open);
	Gateways.Roster->SetTeamsLocked(false);
	bTeamsLocked = false;
	PhaseCore.SetCurrentRoundIndex(PhaseCore.GetRoundsPlayedCount());
	LogStartRolledBack();
}

void FRoundMatchCore::OnParticipantConnectionChanged(FParticipantId Participant, ERoundConnectionState State, bool bKicked, double AtServerTime)
{
	if (!Participant.IsValid())
	{
		return;
	}
	if (State == ERoundConnectionState::Connected)
	{
		OnParticipantRejoined(Participant);
		return;
	}
	if (State == ERoundConnectionState::Disconnected)
	{
		OnParticipantDisconnected(Participant, bKicked ? ERoundDisconnectReason::Kicked : ERoundDisconnectReason::Quit, AtServerTime);
	}
}

// --- Disconnect policy ---------------------------------------------------------------------------------------------

void FRoundMatchCore::OnParticipantUnresponsive(FParticipantId Participant, double AtServerTime)
{
	if (bSessionEnded || !Participant.IsValid() || !Params.bHandOverOnUnresponsive || PhaseCore.GetPhase() != ERoundPhase::Active
		|| Latch.IsTerminated() || AiDriven.Contains(Participant.Value))
	{
		return;
	}
	const FRoundRosterMember* Member = FindMember(Participant);
	// R17.1: an Active participant (or an Unresponsive informant) with a bound pawn; the takeover changes no state.
	if (Member == nullptr || Member->State == EParticipantState::RoundEnded || Gateways.PawnControl == nullptr)
	{
		return;
	}
	if (Gateways.PawnControl->RequestTakeover(Participant))
	{
		AiDriven.Add(Participant.Value);
		FRoundLogEntry Entry;
		Entry.Type = ERoundLogType::HandOver;
		Entry.Participants[0] = Participant.Value;
		LogEntry(Entry);
	}
}

void FRoundMatchCore::OnParticipantResponsive(FParticipantId Participant)
{
	if (bSessionEnded || !Participant.IsValid() || !AiDriven.Contains(Participant.Value) || Grace.Find(Participant) != nullptr)
	{
		return; // a disconnected participant returns only through the rejoin (R17.3)
	}
	if (PhaseCore.GetPhase() != ERoundPhase::Active || Latch.IsTerminated())
	{
		return;
	}
	if (Gateways.PawnControl != nullptr && Gateways.PawnControl->RequestHandBack(Participant))
	{
		AiDriven.Remove(Participant.Value);
		FRoundLogEntry Entry;
		Entry.Type = ERoundLogType::HandBack;
		Entry.Participants[0] = Participant.Value;
		LogEntry(Entry);
	}
}

void FRoundMatchCore::OnParticipantDisconnected(FParticipantId Participant, ERoundDisconnectReason Reason, double AtServerTime)
{
	if (bSessionEnded || !Participant.IsValid())
	{
		return;
	}
	const bool bKicked = Reason == ERoundDisconnectReason::Kicked;
	LastDisconnectedAt.Add(Participant.Value, AtServerTime);
	DisconnectedTick.Add(Participant.Value, Clock != nullptr ? Clock->GetServerTickIndex() : 0);
	LobbyArrived.Remove(Participant.Value);
	if (bKicked)
	{
		KickedParticipants.Add(Participant.Value);
	}
	// Ended/latched: a disconnect changes only the connection (R9). Preparing: the record is made at Active entry.
	if (PhaseCore.GetPhase() != ERoundPhase::Active || Latch.IsTerminated())
	{
		return;
	}
	if (Grace.Find(Participant) != nullptr)
	{
		// R17.2: a kick on a live record starts no second timer and no second takeover; the record is marked, its expiry kept.
		Grace.MarkKicked(Participant, bKicked);
		return;
	}
	const FRoundRosterMember* Member = FindMember(Participant);
	if (Member == nullptr || Member->State != EParticipantState::Active)
	{
		// R17.5 (an Informant has no grace value) is story rmf-072; a non-member has nothing to convert.
		return;
	}
	FRoundLogEntry Entry;
	Entry.Type = ERoundLogType::Disconnected;
	Entry.Participants[0] = Participant.Value;
	Entry.Code = static_cast<uint16>(Reason);
	LogEntry(Entry);
	// R17.2: take over at once unless the Soft hand-over already did (a supersede continues it: no second blend).
	if (!AiDriven.Contains(Participant.Value) && Gateways.PawnControl != nullptr && Gateways.PawnControl->RequestTakeover(Participant))
	{
		AiDriven.Add(Participant.Value);
		FRoundLogEntry HandOver;
		HandOver.Type = ERoundLogType::HandOver;
		HandOver.Participants[0] = Participant.Value;
		LogEntry(HandOver);
	}
	const bool bKnownEpoch = RoundEpochToken.MatchId != 0 || RoundEpochToken.RoundEpoch != 0;
	OpenGraceRecord(Participant, AtServerTime, bKnownEpoch ? RoundEpochToken : Gateways.Roster->GetEpochToken());
}

void FRoundMatchCore::OnParticipantRejoined(FParticipantId Participant)
{
	if (bSessionEnded || !Participant.IsValid())
	{
		return;
	}
	LastDisconnectedAt.Remove(Participant.Value);
	DisconnectedTick.Remove(Participant.Value);
	const ERoundPhase Phase = PhaseCore.GetPhase();
	if (Phase == ERoundPhase::Preparing)
	{
		Preparing.OnParticipantConnected(Participant);
		return;
	}
	// Ended (R9, R18): admitted, view only: no pawn, no transition, no Crowd call. Lobby and Reset: Session's own rules.
	if (Phase != ERoundPhase::Active || Latch.IsTerminated())
	{
		return;
	}
	// The grace timer is cancelled (the record goes; a timer item still in the heap finds none and voids).
	Grace.Close(Participant);
	const FRoundRosterMember* Member = FindMember(Participant);
	if (Member == nullptr)
	{
		return;
	}
	FRoundLogEntry Entry;
	Entry.Type = ERoundLogType::Rejoined;
	Entry.Participants[0] = Participant.Value;
	LogEntry(Entry);
	if (Member->State == EParticipantState::Active)
	{
		// R17.3: the same pawn returns to the human by the reverse blend; the state is unchanged.
		if (AiDriven.Contains(Participant.Value) && Gateways.PawnControl != nullptr && Gateways.PawnControl->RequestHandBack(Participant))
		{
			FRoundLogEntry HandBack;
			HandBack.Type = ERoundLogType::HandBack;
			HandBack.Participants[0] = Participant.Value;
			LogEntry(HandBack);
		}
		AiDriven.Remove(Participant.Value);
		return;
	}
	AiDriven.Remove(Participant.Value);
	if (Member->State == EParticipantState::Informant)
	{
		// R18: re-pawned by Penalty's C1 path, started inside Round's slot (a ConnectionTransition item).
		if (RoundQueue != nullptr)
		{
			FConnectionTransitionPayload Payload;
			Payload.Participant = Participant;
			Payload.ConnectionGeneration = Gateways.Session->GetConnectionGeneration(Participant);
			Payload.Transition = RoundConnectionTransition::InformantReadmitted;
			const bool bKnownEpoch = RoundEpochToken.MatchId != 0 || RoundEpochToken.RoundEpoch != 0;
			RoundQueue->Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::ConnectionTransition,
				bKnownEpoch ? RoundEpochToken : Gateways.Roster->GetEpochToken(), FMailboxId::ServerLocal(), Payload));
		}
		else
		{
			ReadmitInformant(Participant);
		}
	}
}

bool FRoundMatchCore::ReadmitInformant(FParticipantId Participant)
{
	if (bSessionEnded || PhaseCore.GetPhase() != ERoundPhase::Active || Latch.IsTerminated() || Gateways.InformantBodies == nullptr)
	{
		return false;
	}
	const FRoundRosterMember* Member = FindMember(Participant);
	if (Member == nullptr || Member->State != EParticipantState::Informant || LastDisconnectedAt.Contains(Participant.Value))
	{
		return false; // re-validation: still an informant and still connected
	}
	Gateways.InformantBodies->OnInformantReadmitted(Participant);
	return true;
}

bool FRoundMatchCore::ShouldAdmitExpired(FParticipantId Participant) const
{
	const ERoundPhase Phase = PhaseCore.GetPhase();
	return (Phase == ERoundPhase::Preparing || Phase == ERoundPhase::Active || Phase == ERoundPhase::Ended)
		&& InRoundParticipants.Contains(Participant.Value);
}

// --- Session end and travel ----------------------------------------------------------------------------------------

void FRoundMatchCore::OnSessionEnding()
{
	if (bSessionEnded)
	{
		return;
	}
	// R8: the log is flushed with EndReason SessionEnded; then nothing more is processed. No phase change, no release.
	FRoundLogEntry Entry;
	Entry.Type = ERoundLogType::WinLatched;
	Entry.Code = static_cast<uint16>(ERoundEndReason::SessionEnded);
	Entry.Data[0] = GRoundLogNoTeam;
	LogEntry(Entry);
	bSessionEnded = true;
	Preparing.Cancel();
	bRoundEndPending = false;
	RoundEndDueTime = -1.0;
	bResetHeld = false;
	bResetPendingLobby = false;
	if (RoundQueue != nullptr)
	{
		RoundQueue->CancelAllTimers();
	}
}

void FRoundMatchCore::NotifyParticipantLobbyTravelComplete(FParticipantId Participant)
{
	if (Participant.IsValid())
	{
		LobbyArrived.Add(Participant.Value);
	}
}

void FRoundMatchCore::NotifyLobbyTravelComplete()
{
	Gateways.Roster->GetRosteredParticipants(MemberScratch);
	for (const FRoundRosterMember& Member : MemberScratch)
	{
		LobbyArrived.Add(Member.Participant.Value);
	}
}

bool FRoundMatchCore::IsLobbyExitDue() const
{
	if (Now() - ResetEnteredAt >= Params.Preparing.TravelLoadTimeoutSec)
	{
		return true; // R10: Reset never waits on anything unbounded
	}
	Gateways.Roster->GetRosteredParticipants(MemberScratch);
	for (const FRoundRosterMember& Member : MemberScratch)
	{
		// Terminal: TravelComplete reported, or Disconnected (a known disconnect with no reconnection since).
		if (!LobbyArrived.Contains(Member.Participant.Value) && !LastDisconnectedAt.Contains(Member.Participant.Value))
		{
			return false;
		}
	}
	return true;
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
	if (bSessionEnded || PhaseCore.GetPhase() != ERoundPhase::Preparing)
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
		if (Member.State != EParticipantState::RoundEnded)
		{
			InRoundParticipants.Add(Member.Participant.Value);
		}
	}
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
		// R17.2 / R6: a participant spawned AI-driven at P6 (closed gate) already is; one that dropped between P6 and Active
		// is taken over now, so no bound pawn waits driverless through the grace.
		if (Member.State == EParticipantState::Active && !AiDriven.Contains(Member.Participant.Value))
		{
			if (Preparing.IsGateClosed(Member.Participant))
			{
				AiDriven.Add(Member.Participant.Value);
			}
			else if (Gateways.PawnControl != nullptr && Gateways.PawnControl->RequestTakeover(Member.Participant))
			{
				AiDriven.Add(Member.Participant.Value);
			}
		}
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
	if (bSessionEnded)
	{
		return false;
	}
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
	if (!Outcome.IsWin())
	{
		// R8 void: Active -> Reset directly, no winner, no score, called off; no RoundEndStep is staged.
		EnterReset(/*bCalledOff*/ true, Outcome.EndReason, 0, /*bFromPreparing*/ false);
		return true;
	}

	// The latch step (R9 i-v): TeamSummary computed now, published only by the round-end step; nothing else observable.
	ComputeTeamSummary(Outcome, LatchedSummary);
	bRoundEndPending = true;
	RoundEndDueTime = -1.0;
	// A strike decided the round when the deciding transition had a decision reason (debug eliminations excluded).
	const bool bStrikeDecided = Outcome.EndReason == ERoundEndReason::Elimination && !bApplyingDebugItem;
	if (RoundQueue != nullptr && RoundQueue->IsInSlot() && RoundQueue->GetTerminalDecision() == &EndStepRunner)
	{
		// ADR-0002 8.3: one RoundEndStep write; the queue binds it at slot end (death write, fallback, or inline).
		FRoundEndPayload Payload;
		Payload.TerminalKey = Key;
		Payload.bStrikeDecided = bStrikeDecided;
		Payload.FallbackDeferSec = Params.StrikeNetLeadSec;
		if (bStrikeDecided && Gateways.PawnControl != nullptr && DecidingParticipant.IsValid())
		{
			Payload.DecidingVictim = Gateways.PawnControl->GetBoundPawn(DecidingParticipant);
		}
		FStagedWrite Write;
		Write.Type = EStagedWriteType::RoundEndStep;
		Write.ServerTime = Now();
		Write.Payload.Set<FRoundEndPayload>(Payload);
		if (RoundQueue->StageWrite(Write))
		{
			return true;
		}
	}
	if (bStrikeDecided)
	{
		// The prototype strike adapter (outside the queue): the round-end step waits the death-write offset (Tick runs it).
		RoundEndDueTime = Now() + Params.AdapterStrikeRoundEndDelaySec;
		return true;
	}
	RunRoundEndStep();
	return true;
}

void FRoundMatchCore::ComputeTeamSummary(const FRoundTerminalOutcome& Outcome, FRoundTeamSummary OutSummary[2]) const
{
	OutSummary[0] = OutSummary[1] = FRoundTeamSummary();
	// EliminatedCount: Informant in the latch record, every cause alike (forfeits included), before EndRoundAll.
	Gateways.Roster->GetRosteredParticipants(MemberScratch);
	for (const FRoundRosterMember& Member : MemberScratch)
	{
		const int32 Team = RoundMatchCoreLocal::TeamIndex(Member.Team);
		if (Team != INDEX_NONE && Member.State == EParticipantState::Informant && OutSummary[Team].EliminatedCount < MAX_uint8)
		{
			++OutSummary[Team].EliminatedCount;
		}
	}
	// WrongTargetCount: this round's KillResolved rows whose killer is on the team and whose result is WrongTarget.
	int32 WrongTargets[2] = { 0, 0 };
	for (const FRoundLogEntry& Entry : LogRecorder.GetCurrentEntries())
	{
		if (Entry.Type == ERoundLogType::KillResolved && Entry.Code == static_cast<uint16>(ERoundKillResult::WrongTarget) && Entry.Data[0] <= 1)
		{
			++WrongTargets[Entry.Data[0]];
		}
	}
	for (int32 Team = 0; Team < 2; ++Team)
	{
		OutSummary[Team].WrongTargetCount = static_cast<uint8>(FMath::Min(WrongTargets[Team], 255));
		// StepsCompleted: the completed preliminary stages, plus the final for the objective winner only (0-3).
		int32 Steps = Gateways.ObjectiveProgress != nullptr ? FMath::Clamp(Gateways.ObjectiveProgress->GetCompletedPreliminaryStages(Team), 0, 2) : 0;
		if (Outcome.EndReason == ERoundEndReason::ObjectiveWin && Outcome.WinningTeam == Team)
		{
			++Steps;
		}
		OutSummary[Team].StepsCompleted = static_cast<uint8>(FMath::Clamp(Steps, 0, 3));
	}
}

void FRoundMatchCore::OnQueueRoundEndStep(const FRoundEndPayload& Payload)
{
	if (Latch.IsTerminated() && Payload.TerminalKey == Latch.GetTerminalKey())
	{
		RunRoundEndStep();
	}
}

void FRoundMatchCore::RunRoundEndStep()
{
	if (!bRoundEndPending || !Latch.IsTerminated() || PhaseCore.GetPhase() != ERoundPhase::Active || bSessionEnded)
	{
		return;
	}
	bRoundEndPending = false;
	RoundEndDueTime = -1.0;
	const FRoundTerminalOutcome& Outcome = Latch.GetOutcome();
	FRoundResult Result;
	Result.WinningTeam = Outcome.WinningTeam;
	Result.PublicReason = Outcome.GetPublicReason();
	Result.RoundIndex = PhaseCore.GetCurrentRoundIndex();
	Result.TeamSummary[0] = LatchedSummary[0];
	Result.TeamSummary[1] = LatchedSummary[1];
	// (a) + (c): FRoundResult and MatchScore[Winner] + 1 in the same FRoundPublicState write as phase Ended.
	PhaseCore.SetLastResult(Result);
	PhaseCore.SetCalledOff(false);
	EndedAtServerTime = Now();
	bRoundEndStepRan = true;
	PhaseCore.RequestPhase(ERoundPhase::Ended, EndedAtServerTime);
	// (b) the Roster batch (its batch event is never evaluated as an elimination, F2 proof item 5).
	Gateways.Roster->EndRoundAll();
	// (d) the held NotNow replies.
	if (Gateways.Commands != nullptr)
	{
		Gateways.Commands->ReleaseHeldReplies();
	}
	// (e) OnRoundEnded; every viewer is PostRound from here.
	if (OnRoundEnded)
	{
		OnRoundEnded(Result);
	}
	// (f) the whole city holds: Halt, then one kind-blind stop request per live pawn (each pawn once), bound RunStopSec.
	if (Gateways.Crowd != nullptr)
	{
		Gateways.Crowd->Halt();
		if (Gateways.Spawner != nullptr)
		{
			TArray<FRoundLivePawn> Live;
			Gateways.Spawner->GetLivePawns(Live);
			TSet<uint32> Stopped;
			for (const FRoundLivePawn& Pawn : Live)
			{
				bool bAlreadyStopped = false;
				Stopped.Add(Pawn.PawnId, &bAlreadyStopped);
				if (!bAlreadyStopped)
				{
					Gateways.Crowd->RequestPawnStop(Pawn, Params.RunStopSec);
				}
			}
		}
	}
	// (g) every frozen hold cancels through the normal release blend.
	if (Gateways.Commands != nullptr)
	{
		Gateways.Commands->CancelFrozenHolds();
	}
	// (h) EndedMinDwellSec / EndedAutoResetSec count from EndedAtServerTime; parked host resets are held to the dwell end.
	if (RoundQueue != nullptr)
	{
		for (const FQueueItem& Parked : RoundQueue->TakeParkedHostCommands())
		{
			bResetHeld = bResetHeld || (Parked.Type == EQueueItemType::HostCommand && Parked.Payload.IsType<FHostCommandPayload>());
		}
	}
}

bool FRoundMatchCore::DeclareObjectiveWin(uint8 Team, FQueueKey Key)
{
	return Team <= 1 && TryTerminate(FRoundTerminalOutcome::Win(Team, ERoundEndReason::ObjectiveWin), Key);
}

ERoundCompletionValidity FRoundMatchCore::ValidateFinalCompleter(FParticipantId Completer, uint8 Team, const FRoundCompleterControl& Control) const
{
	const FRoundRosterMember* Member = FindMember(Completer);
	FRoundCompleterFacts Facts;
	Facts.bRostered = Member != nullptr;
	Facts.bOnTeam = Member != nullptr && RoundMatchCoreLocal::TeamIndex(Member->Team) == static_cast<int32>(Team);
	Facts.State = Member != nullptr ? Member->State : EParticipantState::RoundEnded;
	Facts.bConnected = Member != nullptr && !LastDisconnectedAt.Contains(Completer.Value)
		&& Gateways.Session->GetConnectionState(Completer) == ERoundConnectionState::Connected;
	const uint64* DisconnectTick = DisconnectedTick.Find(Completer.Value);
	Facts.bDisconnectedThisTick = DisconnectTick != nullptr && Clock != nullptr && *DisconnectTick == Clock->GetServerTickIndex();
	Facts.bAiDriven = AiDriven.Contains(Completer.Value);
	Facts.Control = Control;
	return StealthRoundWin::ValidateCompleter(Facts);
}

bool FRoundMatchCore::SubmitFinalCompletion(FParticipantId Completer, uint8 Team, const FRoundCompleterControl& Control, bool bSiteLive, FQueueKey Key)
{
	const ERoundCompletionValidity Validity = bSiteLive ? ValidateFinalCompleter(Completer, Team, Control) : ERoundCompletionValidity::SiteNotLive;
	if (Validity != ERoundCompletionValidity::Valid)
	{
		// R15 / ADR-0002 8.9: one Voided(FinalCompletion, code) entry; nothing latches and the round stays Active. Inside a
		// slot the queue writes it for the voided item with this code; outside one Round writes it.
		if (RoundQueue != nullptr && RoundQueue->IsInSlot())
		{
			RoundQueue->SetVoidCode(static_cast<uint32>(Validity));
			return false;
		}
		FRoundLogEntry Entry;
		Entry.Type = ERoundLogType::Voided;
		Entry.Key = Key;
		Entry.Code = static_cast<uint16>(EVoidReason::Revalidation);
		Entry.Data[0] = static_cast<uint32>(EQueueItemType::HoldTimer);
		Entry.Data[1] = static_cast<uint32>(Validity);
		Entry.Participants[0] = Completer.Value;
		LogEntry(Entry);
		return false;
	}
	return DeclareObjectiveWin(Team, Key);
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
	if (bSessionEnded || PhaseCore.GetPhase() != ERoundPhase::Active || InEvent.bBatch)
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
		DecidingParticipant = Event.Participant;
		TryTerminate(Outcome.GetValue(), Key);
		DecidingParticipant = FParticipantId();
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
		// A real Roster transition inside the slot: its event reaches OnRosterTransition before the slot ends. A debug
		// elimination stages no death write, so its round-end step is the latch step (bApplyingDebugItem).
		TGuardValue<bool> DebugGuard(bApplyingDebugItem, true);
		Gateways.Roster->RequestTransition(Payload.Subject.Participant, EParticipantState::Informant, ERosterReason::Assassinated,
			Gateways.Roster->GetEpochToken(), TOptional<uint32>());
	}
}

// --- Reset and aborts ----------------------------------------------------------------------------------------------

bool FRoundMatchCore::EnterReset(bool bCalledOff, ERoundEndReason Reason, uint8 AbortCode, bool bFromPreparing)
{
	const bool bReachedEnded = PhaseCore.GetPhase() == ERoundPhase::Ended;
	if (bSessionEnded || !FRoundPhaseCore::IsLegalTransition(PhaseCore.GetPhase(), ERoundPhase::Reset))
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
	// (2) every Round timer cancelled: grace records go and the due-time heap is emptied, the sequence stops.
	Preparing.Cancel();
	Grace.Reset();
	if (RoundQueue != nullptr)
	{
		RoundQueue->CancelAllTimers();
		// Writes staged earlier in this frame belong to the round being torn down (a void decided inside a slot).
		RoundQueue->DropStagedWrites();
	}
	bRoundEndPending = false;
	RoundEndDueTime = -1.0;
	// (3) the slice archived server-side, the round buffer cleared.
	LogRecorder.ArchiveCurrent(PhaseCore.GetCurrentRoundIndex(), RoundEpochToken.RoundEpoch, Reason, AbortCode, bReachedEnded);
	// (5) round scope (R21): Terminated and the queue (Seq restarts at 1) reset; a Reset decided inside a slot (a void)
	// leaves the latched queue to the next Active entry, which opens it again.
	Latch.Reset();
	if (RoundQueue != nullptr && !RoundQueue->IsInSlot())
	{
		RoundQueue->ResetForNewRound();
	}
	// (4) every player pawn of the round destroyed, never released (the world is about to be torn down; Roster R9 needs
	// it before the next ResetAll); a Preparing abort also discards the deal. (5) round-scoped data. (6) lobby travel.
	if (Gateways.Spawner != nullptr)
	{
		Gateways.Spawner->DestroyRoundPawns();
	}
	if (bFromPreparing && Gateways.CharacterPool != nullptr)
	{
		Gateways.CharacterPool->DiscardDeal();
	}
	AiDriven.Reset();
	KickedParticipants.Reset();
	LobbyArrived.Reset();
	Gateways.Session->RequestLobbyTravel();
	bResetHeld = false;
	// (7) Reset -> Lobby once every participant is terminal or TravelLoadTimeoutSec has passed (Tick).
	ResetEnteredAt = Now();
	bResetPendingLobby = true;
	return true;
}

void FRoundMatchCore::LogHostAbort(const FRoundCommandCaller& Caller)
{
	// R22: an abort is recorded with the host's ParticipantId (P0); the slice is archived and never released.
	FRoundLogEntry Entry;
	Entry.Type = ERoundLogType::WinLatched;
	Entry.Code = static_cast<uint16>(ERoundEndReason::Aborted);
	Entry.Data[0] = GRoundLogNoTeam;
	Entry.Participants[0] = Caller.Caller.Value;
	LogEntry(Entry);
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
	// R22: a host abort carries the host's ParticipantId.
	if (bHost)
	{
		Entry.Participants[0] = AbortingHost.Value;
	}
	LogEntry(Entry);
	// No result, no score change; RoundsPlayedCount advances only at Active entry, so it is unchanged.
	EnterReset(/*bCalledOff*/ bHost, Reason, static_cast<uint8>(Code), /*bFromPreparing*/ true);
}

// --- Host commands -------------------------------------------------------------------------------------------------

ERoundCommandResult FRoundMatchCore::ExecuteReset(ERoundHostCommand Command, const FRoundCommandCaller& Caller)
{
	switch (PhaseCore.GetPhase())
	{
	case ERoundPhase::Preparing:
		// R8: the host abort in Preparing takes the one abort path (story rmf-034).
		AbortingHost = Caller.Caller;
		AbortPreparing(ERoundPreparingAbortCode::None, /*bHost*/ true);
		return PhaseCore.GetPhase() == ERoundPhase::Reset ? ERoundCommandResult::Executed : ERoundCommandResult::Refused;
	case ERoundPhase::Active:
		if (Latch.IsTerminated())
		{
			// R9 (v): a host reset or abort between the latch and the round-end step is held like an early reset.
			bResetHeld = true;
			return ERoundCommandResult::Held;
		}
		if (!Params.bHostMayAbortActive)
		{
			if (HostSink != nullptr)
			{
				HostSink->OnCommandRefused(Command, ERoundCommandResult::WrongPhase, 0);
			}
			return ERoundCommandResult::WrongPhase;
		}
		// R8: Active -> Reset directly, EndReason Aborted, no result, no score, called off; the host's id in the log.
		LogHostAbort(Caller);
		return EnterReset(/*bCalledOff*/ true, ERoundEndReason::Aborted, 0, /*bFromPreparing*/ false) ? ERoundCommandResult::Executed : ERoundCommandResult::Refused;
	case ERoundPhase::Lobby:
		if (bTravelPending && !bAbortBeforeP1)
		{
			// Edge Cases / R22: an abort while the round travels (before P1): marked, and Session is asked to cancel. The
			// reason entry (with the host's id) is written by the Preparing abort at P1 or at the cancel.
			bAbortBeforeP1 = true;
			AbortingHost = Caller.Caller;
			Gateways.Session->CancelRoundTravel();
			return ERoundCommandResult::Executed;
		}
		return ERoundCommandResult::NoOp;
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
	if (bSessionEnded)
	{
		return ERoundCommandResult::Unavailable; // R8: processing stopped at OnSessionEnding
	}
	const ERoundPhase Phase = PhaseCore.GetPhase();
	switch (Command)
	{
	case ERoundHostCommand::Start:
		return Start(Caller).IsRefused() ? ERoundCommandResult::Refused : ERoundCommandResult::Executed;

	case ERoundHostCommand::Reset:
	case ERoundHostCommand::Abort:
		// abort is an alias of the abort forms of reset.
		return ExecuteReset(Command, Caller);

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
	if (bSessionEnded)
	{
		return; // R8: processing stopped
	}
	const ERoundPhase Phase = PhaseCore.GetPhase();
	if (Phase == ERoundPhase::Preparing)
	{
		Preparing.Tick();
		return;
	}
	if (Phase == ERoundPhase::Active && bRoundEndPending && RoundEndDueTime >= 0.0 && Now() + FResolutionQueueCore::DueEpsilonSec >= RoundEndDueTime)
	{
		// The adapter path's round-end step (a prototype strike decided the round outside the queue).
		RunRoundEndStep();
		return;
	}
	if (Phase == ERoundPhase::Ended)
	{
		const double Dwell = Now() - EndedAtServerTime;
		const bool bAutoReset = Params.EndedAutoResetSec > 0.0 && Dwell >= Params.EndedAutoResetSec;
		if ((bResetHeld || bAutoReset) && Dwell >= Params.EndedMinDwellSec)
		{
			EnterReset(/*bCalledOff*/ false, Latch.GetOutcome().EndReason, 0, /*bFromPreparing*/ false);
		}
		return;
	}
	if (bResetPendingLobby && Phase == ERoundPhase::Reset && IsLobbyExitDue())
	{
		// R10 Reset completes into Lobby: teams unlock (they change only in Lobby); ResetAll runs at the next Start.
		bResetPendingLobby = false;
		Gateways.Roster->SetTeamsLocked(false);
		bTeamsLocked = false;
		// The round's scope ends with the lobby (R21): viewers are no longer PostRound, the summary is published history.
		bRoundEndStepRan = false;
		LatchedSummary[0] = LatchedSummary[1] = FRoundTeamSummary();
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
	if (Core.bSessionEnded || Record == nullptr || Record->ExpectedConnectionGeneration != Payload->Generation
		|| Core.Gateways.Session->GetConnectionState(Participant) == ERoundConnectionState::Connected)
	{
		return EResolveResult::Voided;
	}
	const FRoundGraceRecord Copy = *Record;
	Core.Grace.Close(Participant);
	const FPawnHandle Pawn = Core.Gateways.PawnControl != nullptr ? Core.Gateways.PawnControl->GetBoundPawn(Participant) : FPawnHandle();
	// R17.4: the guarded Disconnect transition, inline: F2 evaluates its event as an expiry (or a kick) in this slot.
	const ERosterResult Result = Core.Gateways.Roster->RequestTransition(Participant, EParticipantState::Informant, ERosterReason::Disconnect,
		Copy.ExpectedEpoch, TOptional<uint32>(Copy.ExpectedConnectionGeneration));
	Core.AiDriven.Remove(Participant.Value);
	if (Result != ERosterResult::Applied)
	{
		// Rejected_StaleEpoch / StaleConnection / NotDisconnected, NoOp_AlreadyInState (a kill before expiry): a logged no-op.
		if (Core.RoundQueue != nullptr && Core.RoundQueue->IsInSlot())
		{
			Core.RoundQueue->SetVoidCode(static_cast<uint32>(Result));
		}
		return EResolveResult::Voided;
	}
	FRoundLogEntry Entry;
	Entry.Type = ERoundLogType::GraceExpired;
	Entry.Key = Key;
	Entry.Participants[0] = Participant.Value;
	Core.LogEntry(Entry);
	// In the same call stack: the pawn becomes an NPC with the same look (killing it now costs a strike).
	Core.Gateways.Roster->ReleaseToCrowd(Pawn);
	Core.AiDriven.Remove(Participant.Value);
	return EResolveResult::Executed;
}

// --- Connection resolver -------------------------------------------------------------------------------------------

EResolveResult FRoundConnectionResolver::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	const FConnectionTransitionPayload* Payload = Item.Payload.TryGet<FConnectionTransitionPayload>();
	if (Payload == nullptr || Payload->Transition != RoundConnectionTransition::InformantReadmitted)
	{
		return EResolveResult::Voided;
	}
	return Core.ReadmitInformant(Payload->Participant) ? EResolveResult::Executed : EResolveResult::Voided;
}

// --- Round-end step runner -----------------------------------------------------------------------------------------

void FRoundEndStepRunner::RunRoundEndStep(const FRoundEndPayload& Payload)
{
	Core.OnQueueRoundEndStep(Payload);
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
