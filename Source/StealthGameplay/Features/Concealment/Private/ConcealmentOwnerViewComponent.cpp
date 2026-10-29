// Story con-023: the owner's concealed flag on its PlayerController. See View/ConcealmentOwnerViewComponent.h.

#include "View/ConcealmentOwnerViewComponent.h"

#include "GameFramework/PlayerController.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

const FName UConcealmentOwnerViewComponent::ComponentName(TEXT("ConcealmentOwnerView"));

UConcealmentOwnerViewComponent::UConcealmentOwnerViewComponent() = default;

void UConcealmentOwnerViewComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// COND_OwnerOnly on a controller (relevant to its owner only): the owner audience twice over.
	FDoRepLifetimeParams Params;
	Params.Condition = COND_OwnerOnly;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UConcealmentOwnerViewComponent, bIsConcealed, Params);
}

bool UConcealmentOwnerViewComponent::SetConcealed(bool bConcealed)
{
	if (bIsConcealed == bConcealed)
	{
		return false;
	}
	bIsConcealed = bConcealed;
	++ServerWriteCount;
	MARK_PROPERTY_DIRTY_FROM_NAME(UConcealmentOwnerViewComponent, bIsConcealed, this);
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	// The listen host's own controller never receives the RepNotify: raise the owner event here (host parity).
	if (const APlayerController* Controller = Cast<APlayerController>(GetOwner()); Controller != nullptr && Controller->IsLocalController())
	{
		OnConcealedChanged.Broadcast(bIsConcealed);
	}
	return true;
}

void UConcealmentOwnerViewComponent::OnRep_IsConcealed()
{
	OnConcealedChanged.Broadcast(bIsConcealed);
}
