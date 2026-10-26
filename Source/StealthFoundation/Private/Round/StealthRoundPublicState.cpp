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

FText GetRoundEndReasonText(uint8 PublicReason)
{
	switch (PublicReason)
	{
	case RoundPublicReason::Objective: return NSLOCTEXT("StealthRound", "ReasonObjective", "completed the final objective");
	case RoundPublicReason::Elimination: return NSLOCTEXT("StealthRound", "ReasonElimination", "every rival eliminated for the round");
	default: return FText::GetEmpty();
	}
}
