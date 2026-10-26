#include "Server/Core/Round/StealthRoundMatchCore.h"

#include "Server/EventLog/StealthRoundLogTypes.h"
#include "Server/EventLog/StealthRoundLogWriter.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/StealthClock.h"

FRoundMatchCore::FRoundMatchCore(const FRoundGateways& InGateways, IRoundHostSink* InHostSink, IRoundLogWriter* InLog, const IStealthClock* InClock)
	: Gateways(InGateways)
	, HostSink(InHostSink)
	, Log(InLog)
	, Clock(InClock)
	, PhaseCore(InGateways.Session)
	, TeamRules(*InGateways.Roster, InHostSink)
{
	check(Gateways.Session != nullptr && Gateways.Roster != nullptr);
}

double FRoundMatchCore::Now() const
{
	return Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0;
}

// --- Match ---------------------------------------------------------------------------------------------------------

ERosterResult FRoundMatchCore::OpenMatch()
{
	const ERosterResult Result = Gateways.Roster->BeginMatch(Params.TeamSize);
	bTeamsLocked = false;
	bTravelPending = false;
	bResetHeld = false;
	bResetPendingLobby = false;
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
	if (Log == nullptr)
	{
		return;
	}
	FRoundLogEntry Entry;
	Entry.Type = ERoundLogType::StartRolledBack;
	Entry.ServerTime = Now();
	Entry.Code = static_cast<uint16>(ERoundStartRefusalCode::SessionRefused);
	Entry.Data[0] = PhaseCore.GetRoundsPlayedCount() + 1;
	Log->Append(Entry);
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

bool FRoundMatchCore::NotifyRoundTravelAccepted()
{
	if (!bTravelPending)
	{
		return false;
	}
	bTravelPending = false;
	return PhaseCore.RequestPhase(ERoundPhase::Preparing, Now());
}

// --- Phase drivers -------------------------------------------------------------------------------------------------

bool FRoundMatchCore::EnterActive()
{
	return PhaseCore.RequestPhase(ERoundPhase::Active, Now());
}

bool FRoundMatchCore::EndRound(uint8 WinningTeam, uint8 PublicReason)
{
	if (PhaseCore.GetPhase() != ERoundPhase::Active)
	{
		return false;
	}
	FRoundResult Result;
	Result.WinningTeam = WinningTeam;
	Result.PublicReason = PublicReason;
	Result.RoundIndex = PhaseCore.GetCurrentRoundIndex();
	PhaseCore.SetLastResult(Result);
	PhaseCore.SetCalledOff(false);
	EndedAtServerTime = Now();
	return PhaseCore.RequestPhase(ERoundPhase::Ended, EndedAtServerTime);
}

bool FRoundMatchCore::EnterReset(bool bCalledOff)
{
	if (!PhaseCore.RequestPhase(ERoundPhase::Reset, Now()))
	{
		return false;
	}
	if (bCalledOff)
	{
		PhaseCore.SetCalledOff(true);
	}
	bResetHeld = false;
	bResetPendingLobby = true;
	return true;
}

// --- Host commands -------------------------------------------------------------------------------------------------

ERoundCommandResult FRoundMatchCore::ExecuteReset(ERoundHostCommand Command)
{
	switch (PhaseCore.GetPhase())
	{
	case ERoundPhase::Preparing:
		// R8: abort (the abort cleanup itself is story rmf-034).
		return EnterReset(/*bCalledOff*/ true) ? ERoundCommandResult::Executed : ERoundCommandResult::Refused;
	case ERoundPhase::Active:
		if (!Params.bHostMayAbortActive)
		{
			if (HostSink != nullptr)
			{
				HostSink->OnCommandRefused(Command, ERoundCommandResult::WrongPhase, 0);
			}
			return ERoundCommandResult::WrongPhase;
		}
		return EnterReset(/*bCalledOff*/ true) ? ERoundCommandResult::Executed : ERoundCommandResult::Refused;
	case ERoundPhase::Ended:
		if (Now() - EndedAtServerTime >= Params.EndedMinDwellSec)
		{
			return EnterReset(/*bCalledOff*/ false) ? ERoundCommandResult::Executed : ERoundCommandResult::Refused;
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
	if (bResetHeld && PhaseCore.GetPhase() == ERoundPhase::Ended && Now() - EndedAtServerTime >= Params.EndedMinDwellSec)
	{
		EnterReset(/*bCalledOff*/ false);
		return;
	}
	if (bResetPendingLobby && PhaseCore.GetPhase() == ERoundPhase::Reset)
	{
		// R10 Reset completes into Lobby: teams unlock (they change only in Lobby); ResetAll runs at the next Start.
		bResetPendingLobby = false;
		Gateways.Roster->SetTeamsLocked(false);
		bTeamsLocked = false;
		PhaseCore.RequestPhase(ERoundPhase::Lobby, Now());
	}
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
