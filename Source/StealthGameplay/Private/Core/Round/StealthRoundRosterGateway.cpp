#include "Server/Core/Round/StealthRoundRosterGateway.h"

#include "Server/Roster/StealthRosterStore.h"

void URoundRosterStoreGateway::Bind(URosterStore* InStore)
{
	Store = InStore;
}

ERosterResult URoundRosterStoreGateway::BeginMatch(int32 TeamSize)
{
	return Store.IsValid() ? Store->OpenSession(TeamSize) : ERosterResult::Rejected_NotAuthority;
}

ERosterResult URoundRosterStoreGateway::SetTeamSize(int32 TeamSize)
{
	return Store.IsValid() ? Store->SetTeamSize(TeamSize) : ERosterResult::Rejected_NotAuthority;
}

ERosterResult URoundRosterStoreGateway::SetTeamsLocked(bool bLocked)
{
	return Store.IsValid() ? Store->SetTeamsLocked(bLocked) : ERosterResult::Rejected_NotAuthority;
}

ERosterResult URoundRosterStoreGateway::AssignTeams(TConstArrayView<FRoundTeamAssignment> Assignments)
{
	if (!Store.IsValid())
	{
		return ERosterResult::Rejected_NotAuthority;
	}
	TArray<FRosterTeamAssignment, TInlineAllocator<10>> Batch;
	for (const FRoundTeamAssignment& Assignment : Assignments)
	{
		Batch.Add({ Assignment.Participant, Assignment.Team == ERosterTeam::None ? 2 : static_cast<int32>(Assignment.Team) });
	}
	return Store->AssignTeams(Batch);
}

ERosterResult URoundRosterStoreGateway::RemoveParticipant(FParticipantId Participant)
{
	return Store.IsValid() ? Store->RemoveParticipant(Participant) : ERosterResult::Rejected_NotAuthority;
}

ERosterResult URoundRosterStoreGateway::ResetAll(uint32 RoundIndex)
{
	return Store.IsValid() ? Store->ResetAll(RoundIndex) : ERosterResult::Rejected_NotAuthority;
}

ERosterResult URoundRosterStoreGateway::EndRoundAll()
{
	// FRosterCore::EndRoundAll is story psr-023; until then the round end changes no Roster state.
	return ERosterResult::NoOp_Unchanged;
}

ERosterResult URoundRosterStoreGateway::RequestTransition(FParticipantId Participant, EParticipantState NewState, ERosterReason Reason,
	const FEpochToken& ExpectedEpoch, TOptional<uint32> ExpectedConnectionGeneration)
{
	if (!Store.IsValid())
	{
		return ERosterResult::Rejected_NotAuthority;
	}
	FRosterTransitionOptions Options;
	Options.ExpectedEpoch = FRosterEpochToken{ ExpectedEpoch.MatchId, ExpectedEpoch.RoundEpoch };
	Options.ExpectedConnectionGeneration = ExpectedConnectionGeneration;
	return Store->RequestTransition(Participant, NewState, Reason, Options);
}

ERosterResult URoundRosterStoreGateway::BindPawn(FParticipantId Participant, const FPawnHandle& Pawn)
{
	return ERosterResult::Rejected_PawnInUse;
}

ERosterResult URoundRosterStoreGateway::ReleaseToCrowd(const FPawnHandle& Pawn)
{
	return ERosterResult::Rejected_PawnInUse;
}

void URoundRosterStoreGateway::GetUnboundActiveParticipants(TArray<FParticipantId>& OutParticipants) const
{
	OutParticipants.Reset();
	if (Store.IsValid())
	{
		Store->GetUnboundActiveParticipants(OutParticipants);
	}
}

ERosterResult URoundRosterStoreGateway::RecordAppearance(FParticipantId Participant, uint16 AppearanceId)
{
	// The Roster writer of AssignedAppearanceId is unnamed in the GDD (psr-022 open point); no-op until it exists.
	return ERosterResult::NoOp_Unchanged;
}

FEpochToken URoundRosterStoreGateway::GetEpochToken() const
{
	if (!Store.IsValid())
	{
		return FEpochToken();
	}
	const FRosterEpochToken Token = Store->GetEpochToken();
	return FEpochToken(Token.MatchId, Token.RoundEpoch);
}

void URoundRosterStoreGateway::GetRosteredParticipants(TArray<FRoundRosterMember>& OutMembers) const
{
	OutMembers.Reset();
	if (!Store.IsValid() || !Store->IsStoreInitialized())
	{
		return;
	}
	TArray<FParticipantId> Ids;
	Store->GetParticipantIds(Ids);
	for (const FParticipantId& Id : Ids)
	{
		if (const FParticipantRecord* Record = Store->FindRecord(Id))
		{
			OutMembers.Add({ Id, Record->Team, Record->State });
		}
	}
}
