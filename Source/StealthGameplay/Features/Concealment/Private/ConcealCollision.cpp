// Story con-008: the pure collision rule and its change-only writer. See View/ConcealCollision.h.

#include "View/ConcealCollision.h"

#include "Components/PrimitiveComponent.h"

FConcealCollision ComputeConcealCollision(EConcealState State, double /*ElapsedInTransition*/)
{
	FConcealCollision Out;
	if (State != EConcealState::Outside)
	{
		Out.BushVolume = ECR_Ignore;
		Out.Pawn = ECR_Ignore;
	}
	return Out;
}

bool IsConcealMoveBlocked(EConcealState State)
{
	return State != EConcealState::Outside;
}

bool FConcealCollisionWriter::Apply(UPrimitiveComponent* Capsule, const FConcealCollision& Next)
{
	if (Next == Current)
	{
		return false;
	}
	Current = Next;
	++WriteCount;
	if (Capsule != nullptr)
	{
		Capsule->SetCollisionResponseToChannel(ConcealmentCollision::BushVolumeChannel, Next.BushVolume);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, Next.Pawn);
	}
	return true;
}
