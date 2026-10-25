#pragma once

#include "CoreMinimal.h"
#include "MotionWarpingComponent.h"
#include "StealthMotionWarpingComponent.generated.h"

/**
 * Motion warping on every Shared Pawn with the WarpTargets property never replicated
 * (ADR-0018 section 7.2, E5): warp targets are set locally on every machine from replicated
 * state, so no strike target or victim reference travels through motion warping. The lifetime row
 * stays present with COND_Never (DISABLE_REPLICATED_PROPERTY_FAST; WarpTargets is protected in
 * UE 5.8.4 and MotionWarping is Beta, so this is on the engine-upgrade checklist).
 */
UCLASS()
class STEALTHGAMEPLAY_API UStealthMotionWarpingComponent : public UMotionWarpingComponent
{
	GENERATED_BODY()

public:
	/** Calls the parent, then disables WarpTargets replication identically for every pawn. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
