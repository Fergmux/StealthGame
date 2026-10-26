#pragma once

#include "CoreMinimal.h"
#include "View/Core/StealthPawnMovementComponent.h"
#include "StealthMovementComponent.generated.h"

/**
 * Prototype adapter over the production movement component (decision D53). The production UStealthMovementComponent
 * owns every movement rule (speed tiers, informant Run clamp, traversal-mode backstop: stories SCL-010, SCL-014); this
 * adapter only feeds it the prototype state that has no production source yet: the legality class from the prototype
 * PlayerState (informant), the post-strike hold (as disabled commanded motion), and the sample traversal cancel when the
 * backstop refuses the traversal mode. Removed with AStealthCharacter.
 *
 * Prototype class, renamed from UStealthMovementComponent when the production class of that name
 * landed in StealthGameplay (story SCL-001; ADR-0007 section 8). Config/DefaultEngine.ini carries
 * the [CoreRedirects] entry for Blueprints saved against the old name.
 */
UCLASS()
class STEALTHGAME_API UStealthPrototypeMovementComponent : public UStealthMovementComponent
{
	GENERATED_BODY()

protected:
	virtual EStealthGaitLegality GetGaitLegality() const override;
	virtual bool IsCommandedMotionDisabled() const override;
	virtual void OnTraversalModeRefused() override;
};
