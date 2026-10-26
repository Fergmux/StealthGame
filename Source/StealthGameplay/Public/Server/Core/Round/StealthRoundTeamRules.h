#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/Round/StealthRoundTypes.h"

/**
 * Round's team rules (GDD round-match-framework R3, R22; ADR-0005 section 2; stories rmf-018..021). The rule is
 * Round's, the data is the Roster's: counts are read through IRoundRosterGateway::GetRosteredParticipants and every
 * change is one atomic AssignTeams (or SetTeamSize / RemoveParticipant) call. Round never stores a team itself and
 * never on a PlayerController or PlayerState.
 *
 *   OnParticipantRegistered  a new key in Lobby joins the smaller team; a tie goes to team RoundsPlayedCount mod 2
 *                            (so team 0 at RoundsPlayedCount 0: the first registrant, normally the host).
 *   OnParticipantReclaimed   a returning Dormant key keeps its team: no AssignTeams, no rebalance.
 *   SwapTeam                 one move, refused only when the target team already holds TeamSize; Lobby only.
 *   ExchangeTeams            a pair swap validated against the final state in ONE batch (a 5/5 swap succeeds).
 *   RemoveParticipant        Lobby only; the only way a Dormant slot is freed.
 *   ApplyPresetTeamSize      SetTeamSize once when the preset's TeamSize differs, never while teams are locked;
 *                            Rejected_TeamSizeBelowRoster keeps the previous preset and tells the host.
 * Every refusal reaches the host sink only.
 */
class STEALTHGAMEPLAY_API FRoundTeamRules
{
public:
	/** Roster must outlive the rules; HostSink may be null (no host UI). */
	FRoundTeamRules(IRoundRosterGateway& InRoster, IRoundHostSink* InHostSink) : Roster(InRoster), HostSink(InHostSink) {}

	/** R3 pure rule: the team of fewer members; a tie goes to team (RoundsPlayedCount mod 2). */
	static ERosterTeam ChooseTeamForNewKey(int32 Team0Count, int32 Team1Count, uint32 RoundsPlayedCount);

	/** Members per team in the Roster's current layout (Dormant included), optionally skipping one id. */
	void CountTeams(int32 OutCounts[2], FParticipantId Skip = FParticipantId()) const;

	/** R3: assigns a new key in Lobby (one AssignTeams call); outside Lobby nothing happens. Returns the team chosen or None. */
	ERosterTeam OnParticipantRegistered(FParticipantId Id, ERoundPhase Phase, uint32 RoundsPlayedCount);

	/** R3: a reclaimed Dormant key keeps its team. No Roster call. */
	void OnParticipantReclaimed(FParticipantId Id) {}

	/** R3/R22 single move to the other team. */
	ERoundCommandResult SwapTeam(FParticipantId Id, ERoundPhase Phase, int32 TeamSize);

	/** R3 pair exchange: A and B (on different teams) swap teams in one atomic batch. */
	ERoundCommandResult ExchangeTeams(FParticipantId A, FParticipantId B, ERoundPhase Phase);

	/** R22 remove (Lobby only; legal while teams are unlocked, R5 check 3). */
	ERoundCommandResult RemoveParticipant(FParticipantId Id, ERoundPhase Phase);

	/**
	 * R3 preset TeamSize injection. AppliedTeamSize is the preset TeamSize currently applied; on Executed it becomes
	 * RequestedTeamSize, on a refusal it is unchanged (the previous preset stays).
	 */
	ERoundCommandResult ApplyPresetTeamSize(int32& AppliedTeamSize, int32 RequestedTeamSize, bool bTeamsLocked, ERoundPhase Phase);

private:
	const FRoundRosterMember* FindMember(const TArray<FRoundRosterMember>& Members, FParticipantId Id) const;
	void Refuse(ERoundHostCommand Command, ERoundCommandResult Result, uint8 Detail) const;

	IRoundRosterGateway& Roster;
	IRoundHostSink* HostSink = nullptr;
};
