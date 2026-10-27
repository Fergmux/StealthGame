#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/CommandPredicate.h"
#include "UObject/Object.h"

#include "AssassinationAttackPredicate.generated.h"

/**
 * Attack's CanBegin (ADR-0011 section 6; GDD ASN-R1; story asn-008): always Allow. No Attack is ever rejected for a
 * reason about its target, so no reply can confirm a target's kind, team, brace or concealment; the lockout is the action
 * slot's (Striking replies Busy at the Attack's queue slot, FAttackResolverCore), and the legality (Active, not running,
 * not traversing) is the IIF's. Registered weakly per EActionType::Attack from the composition root; side-effect-free.
 */
UCLASS(NotBlueprintable, NotBlueprintType)
class STEALTHGAMEPLAY_API UAssassinationAttackPredicate : public UObject, public ICommandPredicate
{
	GENERATED_BODY()

public:
	/** The pure verdict: Allow, for every pawn and every target. */
	static FPredicateResult Evaluate() { return FPredicateResult::Allow(); }

	/** ICommandPredicate. */
	virtual FPredicateResult CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const override { return Evaluate(); }
};
