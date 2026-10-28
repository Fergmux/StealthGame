#pragma once

// Test double of story oc-018 AC1 (StealthGame.Objectives.HidingNeverWins): the Concealment epic's Interact rejection (a pawn
// in a bush cannot tend: GDD concealment-bushes, AC-CON-24; not built) as a UActionHub predicate in front of the Objectives
// service. Concealed pawns get Reject(NotNow); every other pawn gets the inner predicate's answer. A UObject because the hub
// holds predicates weakly (TWeakInterfacePtr). Test-only: nothing outside StealthGame.Objectives tests creates it.

#include "CoreMinimal.h"
#include "Server/Core/CommandPredicate.h"
#include "UObject/WeakInterfacePtr.h"

#include "ObjectivesConcealmentStub.generated.h"

UCLASS(NotBlueprintType, NotBlueprintable, Transient)
class UObjectivesConcealmentStubPredicate : public UObject, public ICommandPredicate
{
	GENERATED_BODY()

public:
	/** The pawns hidden in a bush. */
	TSet<const AStealthPawn*> Concealed;
	/** The real Interact predicate behind the concealment check (the Objectives service). */
	TWeakInterfacePtr<ICommandPredicate> Inner;
	/** Begins this stub rejected (diagnostics). */
	mutable int32 Rejected = 0;

	virtual FPredicateResult CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const override
	{
		if (Concealed.Contains(&Pawn))
		{
			++Rejected;
			return FPredicateResult::Reject(ECommandRejectCode::NotNow);
		}
		return Inner.IsValid() ? Inner->CanBegin(Pawn, Cmd) : FPredicateResult::Reject(ECommandRejectCode::NotNow);
	}
};
