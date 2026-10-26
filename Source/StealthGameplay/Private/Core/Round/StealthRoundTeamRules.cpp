#include "Server/Core/Round/StealthRoundTeamRules.h"

ERosterTeam FRoundTeamRules::ChooseTeamForNewKey(int32 Team0Count, int32 Team1Count, uint32 RoundsPlayedCount)
{
	if (Team0Count != Team1Count)
	{
		return Team0Count < Team1Count ? ERosterTeam::Team0 : ERosterTeam::Team1;
	}
	// Ties alternate by round so an odd head-count does not always enlarge team 0 (R3).
	return (RoundsPlayedCount % 2u) == 0u ? ERosterTeam::Team0 : ERosterTeam::Team1;
}

void FRoundTeamRules::CountTeams(int32 OutCounts[2], FParticipantId Skip) const
{
	OutCounts[0] = OutCounts[1] = 0;
	TArray<FRoundRosterMember> Members;
	Roster.GetRosteredParticipants(Members);
	for (const FRoundRosterMember& Member : Members)
	{
		if (Member.Participant != Skip && (Member.Team == ERosterTeam::Team0 || Member.Team == ERosterTeam::Team1))
		{
			++OutCounts[static_cast<int32>(Member.Team)];
		}
	}
}

const FRoundRosterMember* FRoundTeamRules::FindMember(const TArray<FRoundRosterMember>& Members, FParticipantId Id) const
{
	return Members.FindByPredicate([Id](const FRoundRosterMember& M) { return M.Participant == Id; });
}

void FRoundTeamRules::Refuse(ERoundHostCommand Command, ERoundCommandResult Result, uint8 Detail) const
{
	if (HostSink != nullptr)
	{
		HostSink->OnCommandRefused(Command, Result, Detail);
	}
}

ERosterTeam FRoundTeamRules::OnParticipantRegistered(FParticipantId Id, ERoundPhase Phase, uint32 RoundsPlayedCount)
{
	// Assignment applies in Lobby only (R3); a joiner in any other phase is a rejoin path (R18, other stories).
	if (Phase != ERoundPhase::Lobby || !Id.IsValid())
	{
		return ERosterTeam::None;
	}
	int32 Counts[2];
	CountTeams(Counts, Id);
	const ERosterTeam Team = ChooseTeamForNewKey(Counts[0], Counts[1], RoundsPlayedCount);
	const FRoundTeamAssignment Assignment{ Id, Team };
	return Roster.AssignTeams(MakeArrayView(&Assignment, 1)) == ERosterResult::Applied ? Team : ERosterTeam::None;
}

ERoundCommandResult FRoundTeamRules::SwapTeam(FParticipantId Id, ERoundPhase Phase, int32 TeamSize)
{
	if (Phase != ERoundPhase::Lobby)
	{
		Refuse(ERoundHostCommand::SwapTeam, ERoundCommandResult::WrongPhase, 0);
		return ERoundCommandResult::WrongPhase;
	}
	TArray<FRoundRosterMember> Members;
	Roster.GetRosteredParticipants(Members);
	const FRoundRosterMember* Member = FindMember(Members, Id);
	if (Member == nullptr)
	{
		Refuse(ERoundHostCommand::SwapTeam, ERoundCommandResult::Refused, static_cast<uint8>(ERosterResult::Rejected_UnknownParticipant));
		return ERoundCommandResult::Refused;
	}
	const ERosterTeam Target = Member->Team == ERosterTeam::Team0 ? ERosterTeam::Team1 : ERosterTeam::Team0;
	int32 Counts[2];
	CountTeams(Counts, Id);
	if (Counts[static_cast<int32>(Target)] >= TeamSize)
	{
		Refuse(ERoundHostCommand::SwapTeam, ERoundCommandResult::Refused, static_cast<uint8>(ERosterResult::Rejected_TeamFull));
		return ERoundCommandResult::Refused;
	}
	const FRoundTeamAssignment Assignment{ Id, Target };
	const ERosterResult Result = Roster.AssignTeams(MakeArrayView(&Assignment, 1));
	if (Result != ERosterResult::Applied)
	{
		Refuse(ERoundHostCommand::SwapTeam, ERoundCommandResult::Refused, static_cast<uint8>(Result));
		return ERoundCommandResult::Refused;
	}
	return ERoundCommandResult::Executed;
}

ERoundCommandResult FRoundTeamRules::ExchangeTeams(FParticipantId A, FParticipantId B, ERoundPhase Phase)
{
	if (Phase != ERoundPhase::Lobby)
	{
		Refuse(ERoundHostCommand::SwapTeam, ERoundCommandResult::WrongPhase, 0);
		return ERoundCommandResult::WrongPhase;
	}
	TArray<FRoundRosterMember> Members;
	Roster.GetRosteredParticipants(Members);
	const FRoundRosterMember* MemberA = FindMember(Members, A);
	const FRoundRosterMember* MemberB = FindMember(Members, B);
	if (MemberA == nullptr || MemberB == nullptr || MemberA->Team == MemberB->Team)
	{
		Refuse(ERoundHostCommand::SwapTeam, ERoundCommandResult::Refused, static_cast<uint8>(ERosterResult::Rejected_InvalidBatch));
		return ERoundCommandResult::Refused;
	}
	// One batch validated against the final state: the counts never change, so a full 5/5 exchange succeeds.
	const FRoundTeamAssignment Batch[2] = { { A, MemberB->Team }, { B, MemberA->Team } };
	const ERosterResult Result = Roster.AssignTeams(MakeArrayView(Batch, 2));
	if (Result != ERosterResult::Applied)
	{
		Refuse(ERoundHostCommand::SwapTeam, ERoundCommandResult::Refused, static_cast<uint8>(Result));
		return ERoundCommandResult::Refused;
	}
	return ERoundCommandResult::Executed;
}

ERoundCommandResult FRoundTeamRules::RemoveParticipant(FParticipantId Id, ERoundPhase Phase)
{
	if (Phase != ERoundPhase::Lobby)
	{
		Refuse(ERoundHostCommand::Remove, ERoundCommandResult::WrongPhase, 0);
		return ERoundCommandResult::WrongPhase;
	}
	const ERosterResult Result = Roster.RemoveParticipant(Id);
	if (Result != ERosterResult::Applied)
	{
		Refuse(ERoundHostCommand::Remove, ERoundCommandResult::Refused, static_cast<uint8>(Result));
		return ERoundCommandResult::Refused;
	}
	return ERoundCommandResult::Executed;
}

ERoundCommandResult FRoundTeamRules::ApplyPresetTeamSize(int32& AppliedTeamSize, int32 RequestedTeamSize, bool bTeamsLocked, ERoundPhase Phase)
{
	if (Phase != ERoundPhase::Lobby)
	{
		Refuse(ERoundHostCommand::Preset, ERoundCommandResult::WrongPhase, 0);
		return ERoundCommandResult::WrongPhase;
	}
	if (bTeamsLocked)
	{
		// Round never changes TeamSize while teams are locked.
		Refuse(ERoundHostCommand::Preset, ERoundCommandResult::Refused, static_cast<uint8>(ERosterResult::Rejected_TeamsLocked));
		return ERoundCommandResult::Refused;
	}
	if (RequestedTeamSize == AppliedTeamSize)
	{
		return ERoundCommandResult::NoOp;
	}
	const ERosterResult Result = Roster.SetTeamSize(RequestedTeamSize);
	if (Result != ERosterResult::Applied)
	{
		// Rejected_TeamSizeBelowRoster (and any other refusal): the previous preset stays; teams must be reassigned first.
		Refuse(ERoundHostCommand::Preset, ERoundCommandResult::Refused, static_cast<uint8>(Result));
		return ERoundCommandResult::Refused;
	}
	AppliedTeamSize = RequestedTeamSize;
	return ERoundCommandResult::Executed;
}
