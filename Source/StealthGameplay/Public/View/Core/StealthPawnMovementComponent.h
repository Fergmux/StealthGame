#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StealthPawnMovementComponent.generated.h"

/**
 * The Shared Pawn's movement component (ADR-0010): one CMC subclass for every pawn kind. This
 * story (SCL-001) adds only the class and its kind-blind defaults; movement shaping arrives with
 * stories SCL-009 onward, one function per rule, reading tuning values only.
 *
 * File name: StealthPawnMovementComponent.h, not StealthMovementComponent.h, because the
 * prototype module still has a header of that name (ADR-0007 rule (f), unique header names; the
 * generated headers would also collide). Rename when the prototype class is deleted.
 */
UCLASS()
class STEALTHGAMEPLAY_API UStealthMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	/** Kind-blind defaults: orient to movement, no RVO avoidance, MaxStepHeight = TraversalMinHeightCm. */
	UStealthMovementComponent();

	/** Applies the traversal heights of the frozen tuning snapshot (MaxStepHeight = MinHeightCm). Same for every pawn. */
	void ApplyTraversalHeights(const struct FStealthTraversalHeights& Heights);
};
