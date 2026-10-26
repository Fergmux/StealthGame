#pragma once

#include "CoreMinimal.h"
#include "View/StealthRosterTypes.h"

/**
 * The owner's role-context gate (ADR-0011 section 9 amendment 2026-10-24, ADR-0021 section 8; story iif-071): which
 * gameplay context the owning client uses. IMC_Informant is selected only when the owner's own raw Roster state (its
 * C-TeamView owner part) is Informant AND (no HUD presenter is bound, or the HUD already presents the owner as an
 * informant, or the presentation hold cap has elapsed since the state arrived). Until then the client keeps IMC_Active
 * (the server rejects with NotNow), so the victim gets no pre-contact warning. A return to Active is immediate.
 * Pure: no world, no clock; the caller passes the times.
 *
 * Usage:
 *   const bool bInformant = StealthOwnerContext::UseInformantContext(RawState, bPresenterBound, PresentedRole, Now, Arrival, CapSec);
 */

/** What the HUD presents as the owner's own role (ADR-0021 PresentedOwnState). */
enum class EPresentedOwnRole : uint8
{
	Unknown,
	Active,
	Informant,
};

namespace StealthOwnerContext
{
	/**
	 * @param RawOwnState      the owner's own participant state from its C-TeamView owner part
	 * @param bPresenterBound  a HUD presenter has registered (SetPresentedOwnRole has been called)
	 * @param PresentedRole    the role the HUD presents
	 * @param LocalNow         client-local time, seconds
	 * @param TStateArrival    client-local time the raw Informant state arrived
	 * @param CapSec           HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec
	 * @return true when IMC_Informant must be the role context
	 */
	STEALTHGAMEPLAY_API bool UseInformantContext(EParticipantState RawOwnState, bool bPresenterBound, EPresentedOwnRole PresentedRole, double LocalNow,
		double TStateArrival, double CapSec);
}
