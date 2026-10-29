// Concealment stories con-013 / con-014 (AC3): the bush gate on Prepare and Interact. See StealthBushGatedPredicate.h.

#include "StealthBushGatedPredicate.h"

#include "Server/ConcealmentService.h"
#include "View/Core/StealthPawn.h"

void UStealthBushGatedPredicate::Initialize(TWeakInterfacePtr<ICommandPredicate> InInner, UConcealmentService* InConcealment)
{
	Inner = InInner;
	Concealment = InConcealment;
}

FPredicateResult UStealthBushGatedPredicate::CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const
{
	if (const UConcealmentService* Service = Concealment.Get(); Service != nullptr && Service->IsInBushOrTransition(&Pawn))
	{
		return FPredicateResult::Reject(ECommandRejectCode::NotNow);
	}
	const ICommandPredicate* Wrapped = Inner.Get();
	return Wrapped != nullptr ? Wrapped->CanBegin(Pawn, Cmd) : FPredicateResult::Reject(ECommandRejectCode::NotNow);
}
