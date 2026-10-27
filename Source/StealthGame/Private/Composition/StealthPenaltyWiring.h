#pragma once

#include "CoreMinimal.h"
#include "Server/StrikeTallySource.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UPenaltyService;

/**
 * STAND-IN of the composition root while the prototype penalty still exiles (NPC-Kill Penalty stories nkp-001..004;
 * story nkp-005 moves the exile and nkp-021 retires the prototype): the encounter service's IStrikeTallySource.
 *
 * Every Attack's tally query also reaches the production UPenaltyService, so its tally, LastResultSeq and StrikeCounted
 * rows stay in step with the strikes it counts from OnPawnKilled (no k <= t fault); the answer the result carries is still
 * the prototype's (its exile sets bExiled), so nothing a player sees changes until the production exile lands. When story
 * nkp-005 lands, the root registers UPenaltyService itself as the tally source and this adapter goes.
 */
class FStealthShadowPenaltyTally final : public IStrikeTallySource
{
public:
	explicit FStealthShadowPenaltyTally(IStrikeTallySource* InPrototype) : Prototype(InPrototype) {}

	/** The production service that shadows every query (null: the prototype alone). */
	void SetPenalty(UPenaltyService* InPenalty);

	virtual FStrikeTally GetTallyForResult(const APawn* Attacker, EStrikeResult Result) override;
	virtual FStrikeTally GetTallyForStrike(const APawn* Attacker, EStrikeResult Result, const FClientCommandSeq& Seq) override;

private:
	IStrikeTallySource* Prototype = nullptr;
	TWeakObjectPtr<UPenaltyService> Penalty;
};
