#include "View/Core/StealthMotionWarpingComponent.h"

#include "Net/UnrealNetwork.h"

void UStealthMotionWarpingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// WarpTargets is protected, so the name-checked DISABLE_REPLICATED_PROPERTY does not compile from
	// a subclass; the FAST form leaves the lifetime row with COND_Never (ADR-0018 E5).
	DISABLE_REPLICATED_PROPERTY_FAST(UMotionWarpingComponent, WarpTargets);
}
