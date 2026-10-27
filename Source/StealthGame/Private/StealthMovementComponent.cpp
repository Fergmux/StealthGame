#include "StealthMovementComponent.h"
#include "StealthCharacter.h"

EStealthGaitLegality UStealthPrototypeMovementComponent::GetGaitLegality() const
{
	// Prototype legality source (the replicated owner-visible LifeState) until the Roster adapter is injected (iif-071).
	const AStealthCharacter* Character = Cast<AStealthCharacter>(CharacterOwner);
	if (Character != nullptr && Character->IsInformant())
	{
		return EStealthGaitLegality::Informant;
	}
	return Super::GetGaitLegality();
}

bool UStealthPrototypeMovementComponent::IsCommandedMotionDisabled() const
{
	const AStealthCharacter* Character = Cast<AStealthCharacter>(CharacterOwner);
	return Super::IsCommandedMotionDisabled() || (Character != nullptr && Character->IsHeldInPlace());
}

void UStealthPrototypeMovementComponent::OnTraversalModeRefused()
{
	// The Game Animation Sample switches to MOVE_Flying for a traversal montage: cancel the montage the refusal stranded.
	if (AStealthCharacter* Character = Cast<AStealthCharacter>(CharacterOwner))
	{
		Character->CancelTraversal();
	}
}
