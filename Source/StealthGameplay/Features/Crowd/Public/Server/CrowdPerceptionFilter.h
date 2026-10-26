#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CrowdPerceptionFilter.generated.h"

class APawn;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UCrowdPerceptionFilter : public UInterface
{
	GENERATED_BODY()
};

/**
 * ICrowdPerceptionFilter: the consumer-owned interface Crowd perception reads (ADR-0016 section 5; GDD CNB-R2, AC-CNB-36;
 * story cnb-008). Implemented by Concealment (ADR-0017) and registered weakly by the composition root through
 * UCrowdDirector::SetPerceptionFilter. Both answers are identical for every pawn kind and take no viewer. With no filter
 * registered (Concealment absent) every pawn is perceivable and an obstacle, and bush red herrings are disabled.
 */
class STEALTHGAMEPLAY_API ICrowdPerceptionFilter
{
	GENERATED_BODY()

public:
	/** False while the pawn is Concealed (any kind): no NPC glance, partner or reaction may target it. */
	virtual bool IsPerceivable(const APawn* Pawn) const = 0;

	/** False while the pawn is Entering, Exiting, Emerging or Concealed (it ignores pawn collision): no steering around it. */
	virtual bool IsSteeringObstacle(const APawn* Pawn) const = 0;
};
