#include "View/Core/OwnerContextGate.h"

bool StealthOwnerContext::UseInformantContext(EParticipantState RawOwnState, bool bPresenterBound, EPresentedOwnRole PresentedRole, double LocalNow,
	double TStateArrival, double CapSec)
{
	if (RawOwnState != EParticipantState::Informant)
	{
		return false;
	}
	return !bPresenterBound || PresentedRole == EPresentedOwnRole::Informant || LocalNow - TStateArrival >= CapSec;
}
