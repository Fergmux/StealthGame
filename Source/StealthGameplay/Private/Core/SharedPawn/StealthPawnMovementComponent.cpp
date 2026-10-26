#include "View/Core/StealthPawnMovementComponent.h"

#include "View/Core/StealthLocomotionModel.h"

UStealthMovementComponent::UStealthMovementComponent()
{
	// Kind-blind defaults (ADR-0010 section 2). RVO would steer bodies by a local avoidance pass the
	// owner's prediction cannot reproduce; the same value applies to every pawn.
	bOrientRotationToMovement = true;
	bUseRVOAvoidance = false;

	// R17.1 / ADR-0010 Body: MaxStepHeight is TraversalMinHeightCm (one key, two uses); the walkable floor angle
	// stays at the engine default (44.765 degrees). The default here is the canonical key default; ApplyTraversalHeights
	// re-applies the frozen tuning snapshot once SCL-003 registers the key.
	ApplyTraversalHeights(FStealthTraversalHeights());
}

void UStealthMovementComponent::ApplyTraversalHeights(const FStealthTraversalHeights& Heights)
{
	MaxStepHeight = Heights.MinHeightCm;
}
