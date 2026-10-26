#include "View/StealthRoundPublicState.h"

const TCHAR* LexRoundPhase(ERoundPhase Phase)
{
	switch (Phase)
	{
	case ERoundPhase::Lobby: return TEXT("Lobby");
	case ERoundPhase::Preparing: return TEXT("Preparing");
	case ERoundPhase::Active: return TEXT("Active");
	case ERoundPhase::Ended: return TEXT("Ended");
	case ERoundPhase::Reset: return TEXT("Reset");
	default: return TEXT("Unknown");
	}
}
