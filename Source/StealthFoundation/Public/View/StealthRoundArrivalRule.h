#pragma once

#include "CoreMinimal.h"
#include "View/StealthRosterTypes.h"
#include "View/StealthRoundPublicState.h"

/**
 * The client arrival rule of the round end (GDD round-match-framework R9 "Arrival order"; ADR-0002 section 8.5; story
 * rmf-056). The GameState (FRoundPublicState, Phase = Ended) and the Roster carriers are different actors, so a client
 * may receive a record's RoundEnded state a net frame before its phase turns Ended. Every consumer of a teammate's state
 * (Teammate Markers TM-R5, the HUD viewer state) reads it through this rule: a RoundEnded that arrives while the
 * client's phase is not yet Ended is no change, and the consumer keeps what it showed. Client-safe (View/), header-only.
 *
 * Example:
 *   Shown = StealthRoundArrival::ResolveRecordState(ClientPhase, Incoming, Shown);
 */
namespace StealthRoundArrival
{
	/** The state a consumer shows for a record: Incoming, except a RoundEnded arriving before the phase is Ended. */
	inline EParticipantState ResolveRecordState(ERoundPhase ClientPhase, EParticipantState Incoming, EParticipantState Shown)
	{
		if (Incoming == EParticipantState::RoundEnded && ClientPhase != ERoundPhase::Ended && ClientPhase != ERoundPhase::Reset)
		{
			return Shown;
		}
		return Incoming;
	}
}
