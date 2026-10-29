#pragma once

#include "CoreMinimal.h"
#include "Server/Core/CommandPredicate.h"
#include "UObject/Object.h"
#include "UObject/WeakInterfacePtr.h"

#include "StealthBushGatedPredicate.generated.h"

class UConcealmentService;

/**
 * The composition root's bush gate on Prepare and Interact (GDD CON-R7, CON-R9, IIF R9; Concealment stories con-013 and
 * con-014 AC3; STAND-IN for IIF story 052 "no brace or tend in bush"): a pawn in a bush or any hide transition answers
 * Reject(NotNow) (a concealed brace would stack two advantages behind one tell; concealed tending defeats P4); every other
 * pawn gets the wrapped feature predicate's verdict unchanged. Neither refusal is a concealment break.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class UStealthBushGatedPredicate : public UObject, public ICommandPredicate
{
	GENERATED_BODY()

public:
	/** Wraps Inner (held weakly; dead or missing: NotNow, as the hub answers for a missing predicate). */
	void Initialize(TWeakInterfacePtr<ICommandPredicate> InInner, UConcealmentService* InConcealment);

	virtual FPredicateResult CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const override;

private:
	TWeakInterfacePtr<ICommandPredicate> Inner;
	TWeakObjectPtr<UConcealmentService> Concealment;
};
