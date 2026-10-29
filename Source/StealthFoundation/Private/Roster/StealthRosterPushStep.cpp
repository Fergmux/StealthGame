#include "Roster/StealthRosterPushStep.h"

#include "GameFramework/Pawn.h"

FRosterPushStep::FRosterPushStep(const FRosterCore& InCore, const IRosterRoundPhase& InRoundPhase, FPawnResolver InResolver)
	: Core(InCore)
	, RoundPhase(InRoundPhase)
	, Resolver(MoveTemp(InResolver))
{
}

void FRosterPushStep::RegisterView(FParticipantId Id, URosterTeamViewComponent* View)
{
	Views.Add(Id, View);
	bAllDirty = true;
}

void FRosterPushStep::UnregisterView(FParticipantId Id)
{
	Views.Remove(Id);
}

void FRosterPushStep::Reset()
{
	Views.Reset();
	Reconnect.Reset();
	OwnerRespawnDue.Reset();
	OwnerTally.Reset();
	Concealed.Reset();
	OwnerDirty.Reset();
	RetiringThisFrame.Reset();
	RetiringClearNextFrame.Reset();
	bDirty[0] = bDirty[1] = false;
	bAllDirty = false;
}

void FRosterPushStep::MarkTeamDirty(ERosterTeam Team)
{
	if (Team == ERosterTeam::Team0 || Team == ERosterTeam::Team1)
	{
		bDirty[static_cast<int32>(Team)] = true;
	}
}

void FRosterPushStep::ClearReconnect(FParticipantId Id, EReconnectEndCause Cause)
{
	FReconnectState* State = Reconnect.Find(Id);
	if (State != nullptr && State->bReconnecting)
	{
		State->bReconnecting = false;
		State->Cause = Cause;
	}
}

void FRosterPushStep::OnRosterEvent(const FRosterEvent& Event)
{
	const FParticipantRecord* Record = Event.Id.IsValid() ? Core.FindRecord(Event.Id) : nullptr;
	const ERosterTeam Team = Record != nullptr ? Record->Team : ERosterTeam::None;

	switch (Event.Type)
	{
	case ERosterEventType::ConnectionChanged:
		if (Event.bKicked)
		{
			ClearReconnect(Event.Id, EReconnectEndCause::Cleared);
		}
		else if (Event.Connection == ERosterConnection::Disconnected)
		{
			// D23: only during Round phase Active; outside it the flag stays false.
			if (RoundPhase.IsRoundPhaseActive())
			{
				FReconnectState& State = Reconnect.FindOrAdd(Event.Id);
				State.bReconnecting = true;
				State.Cause = EReconnectEndCause::None;
			}
		}
		else if (Event.Connection == ERosterConnection::Connected)
		{
			ClearReconnect(Event.Id, EReconnectEndCause::Rejoined);
		}
		MarkTeamDirty(Team);
		break;

	case ERosterEventType::ParticipantStateChanged:
		ClearReconnect(Event.Id, Event.Reason == ERosterReason::Disconnect ? EReconnectEndCause::GraceExpired : EReconnectEndCause::Cleared);
		if (Event.Old == EParticipantState::Active && Event.New == EParticipantState::Informant)
		{
			RetiringThisFrame.Add(Event.Id);
		}
		MarkTeamDirty(Event.Team != ERosterTeam::None ? Event.Team : Team);
		break;

	case ERosterEventType::ParticipantRemoved:
		ClearReconnect(Event.Id, EReconnectEndCause::Cleared);
		bAllDirty = true;
		break;

	case ERosterEventType::RosterRoundEnded:
	case ERosterEventType::RosterReset:
		if (Event.Type == ERosterEventType::RosterReset)
		{
			OwnerRespawnDue.Reset(); // the new epoch starts with no countdown (Penalty also writes 0 before the latch)
			OwnerTally.Reset(); // and with a 0 tally (Penalty's reset re-push writes 0 under the new epoch)
			Concealed.Reset(); // every pawn is destroyed at the reset: no teammate stays concealed
		}
		for (TPair<FParticipantId, FReconnectState>& Pair : Reconnect)
		{
			ClearReconnect(Pair.Key, EReconnectEndCause::Cleared);
		}
		bAllDirty = true;
		break;

	case ERosterEventType::BindingLost:
		MarkTeamDirty(Team);
		break;

	default:
		// Registration, reclaim and team changes can move anyone between teams.
		bAllDirty = true;
		break;
	}
}

FTeamView FRosterPushStep::BuildView(const FParticipantRecord& Owner) const
{
	FTeamView View;
	View.Owner.Handle = Owner.PublicHandle;
	View.Owner.Team = static_cast<uint8>(Owner.Team);
	View.Owner.State = static_cast<uint8>(Owner.State);
	View.Owner.RoundEpoch = Core.GetRoundEpoch();
	if (const double* Due = OwnerRespawnDue.Find(Owner.ParticipantId))
	{
		View.Owner.T_due = *Due;
	}
	if (const TPair<uint8, uint32>* Tally = OwnerTally.Find(Owner.ParticipantId))
	{
		View.Owner.WrongTargetTally = Tally->Key;
		View.Owner.LastResultSeq = Tally->Value;
	}
	if (Owner.Team == ERosterTeam::None)
	{
		return View;
	}

	TArray<FParticipantId> Ids;
	Core.GetParticipantIds(Ids);
	for (const FParticipantId& Id : Ids)
	{
		const FParticipantRecord* Mate = Core.FindRecord(Id);
		if (Mate == nullptr || Id == Owner.ParticipantId || Mate->Team != Owner.Team)
		{
			continue;
		}
		FTeamViewTeammateEntry& Entry = View.Teammates.AddDefaulted_GetRef();
		Entry.Handle = Mate->PublicHandle;
		Entry.State = static_cast<uint8>(Mate->State);
		FRosterPawnHandle Pawn = Mate->PawnBinding;
		if (RetiringThisFrame.Contains(Id))
		{
			int64 RetireAtMs = 0;
			if (Core.FindLatestRemovedPawn(Id, Pawn, RetireAtMs))
			{
				Entry.RetireAtServerTime = static_cast<double>(RetireAtMs) / 1000.0;
			}
		}
		Entry.PawnRef = (Pawn.IsSet() && Resolver) ? Resolver(Pawn) : nullptr;
		Entry.bIsConcealed = Concealed.Contains(Id);
		if (const FReconnectState* State = Reconnect.Find(Id))
		{
			Entry.bReconnecting = State->bReconnecting;
			Entry.ReconnectEndCause = State->Cause;
		}
	}
	return View;
}

int32 FRosterPushStep::Flush()
{
	// A retiring update is followed by one more flush of the same team that clears RetireAtServerTime.
	for (const FParticipantId& Id : RetiringClearNextFrame)
	{
		if (const FParticipantRecord* Record = Core.FindRecord(Id))
		{
			MarkTeamDirty(Record->Team);
		}
	}
	RetiringClearNextFrame.Reset();

	if (!bAllDirty && !bDirty[0] && !bDirty[1] && OwnerDirty.Num() == 0)
	{
		return 0;
	}

	int32 Written = 0;
	for (TPair<FParticipantId, TWeakObjectPtr<URosterTeamViewComponent>>& Pair : Views)
	{
		URosterTeamViewComponent* Component = Pair.Value.Get();
		const FParticipantRecord* Owner = Core.FindRecord(Pair.Key);
		if (Component == nullptr || Owner == nullptr)
		{
			continue;
		}
		const bool bOwnerDirty = bAllDirty || OwnerDirty.Contains(Pair.Key) || (Owner->Team != ERosterTeam::None && bDirty[static_cast<int32>(Owner->Team)]);
		if (!bOwnerDirty)
		{
			continue;
		}
		const FTeamView View = BuildView(*Owner);
		if (View != Component->GetView())
		{
			Component->ApplyView(View);
			++Written;
		}
	}

	RetiringClearNextFrame = MoveTemp(RetiringThisFrame);
	RetiringThisFrame.Reset();
	bDirty[0] = bDirty[1] = false;
	bAllDirty = false;
	OwnerDirty.Reset();
	return Written;
}

ERosterResult FRosterPushStep::SetOwnerRespawnDue(FParticipantId Id, double DueServerTime, uint32 ExpectedEpoch)
{
	if (ExpectedEpoch != Core.GetRoundEpoch())
	{
		return ERosterResult::Rejected_StaleEpoch;
	}
	if (Core.FindRecord(Id) == nullptr)
	{
		return ERosterResult::Rejected_UnknownParticipant;
	}
	const double Value = DueServerTime > 0.0 ? DueServerTime : 0.0;
	if (GetOwnerRespawnDue(Id) == Value)
	{
		return ERosterResult::NoOp_Unchanged;
	}
	if (Value == 0.0)
	{
		OwnerRespawnDue.Remove(Id);
	}
	else
	{
		OwnerRespawnDue.Add(Id, Value);
	}
	OwnerDirty.Add(Id); // the owner's own view only: teammates' views carry no T_due
	return ERosterResult::Applied;
}

ERosterResult FRosterPushStep::SetOwnerTally(FParticipantId Id, uint8 WrongTargetTally, uint32 LastResultSeq, uint32 ExpectedEpoch)
{
	if (ExpectedEpoch != Core.GetRoundEpoch())
	{
		return ERosterResult::Rejected_StaleEpoch;
	}
	if (Core.FindRecord(Id) == nullptr)
	{
		return ERosterResult::Rejected_UnknownParticipant;
	}
	const TPair<uint8, uint32> Value(WrongTargetTally, LastResultSeq);
	if (GetOwnerTally(Id) == Value)
	{
		return ERosterResult::NoOp_Unchanged;
	}
	if (WrongTargetTally == 0 && LastResultSeq == 0)
	{
		OwnerTally.Remove(Id);
	}
	else
	{
		OwnerTally.Add(Id, Value);
	}
	OwnerDirty.Add(Id); // the owner's own view only: teammates' views carry no tally
	return ERosterResult::Applied;
}

ERosterResult FRosterPushStep::SetTeammateConcealed(FParticipantId Id, bool bConcealed)
{
	const FParticipantRecord* Record = Core.FindRecord(Id);
	if (Record == nullptr)
	{
		return ERosterResult::Rejected_UnknownParticipant;
	}
	if (Concealed.Contains(Id) == bConcealed)
	{
		return ERosterResult::NoOp_Unchanged;
	}
	if (bConcealed)
	{
		Concealed.Add(Id);
	}
	else
	{
		Concealed.Remove(Id);
	}
	MarkTeamDirty(Record->Team); // the teammates' views carry the flag
	return ERosterResult::Applied;
}

TPair<uint8, uint32> FRosterPushStep::GetOwnerTally(FParticipantId Id) const
{
	const TPair<uint8, uint32>* Tally = OwnerTally.Find(Id);
	return Tally != nullptr ? *Tally : TPair<uint8, uint32>(0, 0);
}

double FRosterPushStep::GetOwnerRespawnDue(FParticipantId Id) const
{
	const double* Due = OwnerRespawnDue.Find(Id);
	return Due != nullptr ? *Due : 0.0;
}

bool FRosterPushStep::IsReconnecting(FParticipantId Id) const
{
	const FReconnectState* State = Reconnect.Find(Id);
	return State != nullptr && State->bReconnecting;
}

EReconnectEndCause FRosterPushStep::GetReconnectEndCause(FParticipantId Id) const
{
	const FReconnectState* State = Reconnect.Find(Id);
	return State != nullptr ? State->Cause : EReconnectEndCause::None;
}
