#include "StealthPenaltyWiring.h"

#include "Server/StealthPenaltyService.h"

void FStealthShadowPenaltyTally::SetPenalty(UPenaltyService* InPenalty)
{
	Penalty = InPenalty;
}

FStrikeTally FStealthShadowPenaltyTally::GetTallyForResult(const APawn* Attacker, EStrikeResult Result)
{
	return GetTallyForStrike(Attacker, Result, FClientCommandSeq());
}

FStrikeTally FStealthShadowPenaltyTally::GetTallyForStrike(const APawn* Attacker, EStrikeResult Result, const FClientCommandSeq& Seq)
{
	if (UPenaltyService* Service = Penalty.Get())
	{
		Service->GetTallyForStrike(Attacker, Result, Seq); // production store kept in step; its answer is not used yet
	}
	return Prototype != nullptr ? Prototype->GetTallyForResult(Attacker, Result) : FStrikeTally();
}
