#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/Round/StealthRoundTypes.h"

/**
 * Round's Start validation (GDD round-match-framework R5, F3; stories rmf-022, rmf-023). A pure function over the
 * Start inputs; the orchestration (admission lock, ResetAll, travel, rollback) is FRoundMatchCore::Start.
 *
 * Fixed check order, the first failure decides:
 *   (1) NotHost  (2) WrongPhase / TravelInProgress  (3) ParticipantNotConnected (Unresponsive, Connecting or Dormant;
 *   the ids are listed)  (4) TeamsInvalid (F3: Oversize, then TooSmall, then Imbalance)  (5) PresetInvalid (TDC's
 *   best-effort dry run; skipped when Unavailable) / DeckInfeasible (no gateway yet: skipped).
 *
 * F3: for t in {0,1}: m_t <= n_t <= TeamSize; |n0 - n1| <= MaxTeamImbalanceCount; n0 + n1 >= 1, where m_t =
 * MinPlayersPerTeamCount except that with bAllowEmptyTeamDev an empty team has m_t = 0. Traveling, Disconnected,
 * Rejoining and Expired are not R5 check-3 states and are not refused here (story rmf-023 open point).
 */
struct FRoundStartMemberState
{
	FParticipantId Participant;
	ERosterTeam Team = ERosterTeam::None;
	ERoundConnectionState Connection = ERoundConnectionState::Connected;
};

struct FRoundStartInputs
{
	bool bIsHost = false;
	ERoundPhase Phase = ERoundPhase::Lobby;
	bool bTravelInProgress = false;
	TArray<FRoundStartMemberState> Members;
	FRoundRuleParams Params;
	ERoundPresetPreview PresetPreview = ERoundPresetPreview::Unavailable;
};

class STEALTHGAMEPLAY_API FRoundStartValidator
{
public:
	/** The R5 checks in order; Accepted() when every check passes. */
	static FStartRefusal Validate(const FRoundStartInputs& Inputs);

	/** F3 alone over the two team counts (None when valid). */
	static ERoundTeamsInvalidReason CheckTeams(int32 Team0Count, int32 Team1Count, const FRoundRuleParams& Params);

	/** R5 check 3's states. */
	static bool IsNotConnectedForStart(ERoundConnectionState State);
};
