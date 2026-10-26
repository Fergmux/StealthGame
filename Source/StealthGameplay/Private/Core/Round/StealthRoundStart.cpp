#include "Server/Core/Round/StealthRoundStart.h"

bool FRoundStartValidator::IsNotConnectedForStart(ERoundConnectionState State)
{
	return State == ERoundConnectionState::Unresponsive || State == ERoundConnectionState::Connecting || State == ERoundConnectionState::Dormant;
}

ERoundTeamsInvalidReason FRoundStartValidator::CheckTeams(int32 Team0Count, int32 Team1Count, const FRoundRuleParams& Params)
{
	const int32 Counts[2] = { Team0Count, Team1Count };
	// Oversize is defensive only: the real Roster refuses to exceed TeamSize.
	for (const int32 Count : Counts)
	{
		if (Count > Params.TeamSize)
		{
			return ERoundTeamsInvalidReason::Oversize;
		}
	}
	if (Team0Count + Team1Count < 1)
	{
		return ERoundTeamsInvalidReason::TooSmall;
	}
	const bool bOneTeamEmpty = Team0Count == 0 || Team1Count == 0;
	for (const int32 Count : Counts)
	{
		const int32 Min = (Params.bAllowEmptyTeamDev && bOneTeamEmpty && Count == 0) ? 0 : Params.MinPlayersPerTeamCount;
		if (Count < Min)
		{
			return ERoundTeamsInvalidReason::TooSmall;
		}
	}
	if (FMath::Abs(Team0Count - Team1Count) > Params.MaxTeamImbalanceCount)
	{
		return ERoundTeamsInvalidReason::Imbalance;
	}
	return ERoundTeamsInvalidReason::None;
}

FStartRefusal FRoundStartValidator::Validate(const FRoundStartInputs& Inputs)
{
	// (1)
	if (!Inputs.bIsHost)
	{
		return FStartRefusal::Make(ERoundStartRefusalCode::NotHost);
	}
	// (2)
	if (Inputs.Phase != ERoundPhase::Lobby)
	{
		return FStartRefusal::Make(ERoundStartRefusalCode::WrongPhase);
	}
	if (Inputs.bTravelInProgress)
	{
		return FStartRefusal::Make(ERoundStartRefusalCode::TravelInProgress);
	}
	// (3) A Dormant participant is never pruned automatically: the host removes them or waits.
	FStartRefusal NotConnected = FStartRefusal::Make(ERoundStartRefusalCode::ParticipantNotConnected);
	for (const FRoundStartMemberState& Member : Inputs.Members)
	{
		if (IsNotConnectedForStart(Member.Connection))
		{
			NotConnected.ParticipantIds.Add(Member.Participant);
		}
	}
	if (NotConnected.ParticipantIds.Num() > 0)
	{
		return NotConnected;
	}
	// (4) An unassigned participant cannot reach here: R3 assigns every new key in Lobby.
	int32 Counts[2] = { 0, 0 };
	for (const FRoundStartMemberState& Member : Inputs.Members)
	{
		if (Member.Team == ERosterTeam::Team0 || Member.Team == ERosterTeam::Team1)
		{
			++Counts[static_cast<int32>(Member.Team)];
		}
	}
	const ERoundTeamsInvalidReason Teams = CheckTeams(Counts[0], Counts[1], Inputs.Params);
	if (Teams != ERoundTeamsInvalidReason::None)
	{
		return FStartRefusal::Make(ERoundStartRefusalCode::TeamsInvalid, Teams);
	}
	// (5) Best effort: an Unavailable dry run is skipped (Preparing catches the fault later).
	if (Inputs.PresetPreview == ERoundPresetPreview::Invalid)
	{
		return FStartRefusal::Make(ERoundStartRefusalCode::PresetInvalid);
	}
	return FStartRefusal::Accepted();
}
