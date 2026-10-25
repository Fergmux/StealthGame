#include "View/Core/StealthPawnMovementComponent.h"

UStealthMovementComponent::UStealthMovementComponent()
{
	// Kind-blind defaults (ADR-0010 section 2). RVO would steer bodies by a local avoidance pass the
	// owner's prediction cannot reproduce; the same value applies to every pawn.
	bOrientRotationToMovement = true;
	bUseRVOAvoidance = false;
}
