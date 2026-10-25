#include "StealthMovementComponent.h"
#include "StealthCharacter.h"
#include "StealthSettings.h"

UStealthPrototypeMovementComponent::UStealthPrototypeMovementComponent()
{
	// NPCs follow paths through the same acceleration model players use, so motion matching
	// produces the same kind of starts, stops and turns for both (Pillar 1, TD-FEASIBILITY risk 1).
	NavMovementProperties.bUseAccelerationForPaths = true;
}

float UStealthPrototypeMovementComponent::GetMaxSpeed() const
{
	float Speed = Super::GetMaxSpeed();

	if (const AStealthCharacter* Character = Cast<AStealthCharacter>(CharacterOwner))
	{
		if (Character->IsHeldInPlace())
		{
			return 0.f;
		}
		if (Character->IsInformant())
		{
			Speed = FMath::Min(Speed, UStealthSettings::Get()->InformantMaxSpeed);
		}
	}
	return Speed;
}

void UStealthPrototypeMovementComponent::SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode)
{
	// The Game Animation Sample switches to MOVE_Flying for the duration of a traversal montage.
	// Informants may not use parkour: refuse the switch and cancel the traversal.
	if (NewMovementMode == MOVE_Flying)
	{
		if (AStealthCharacter* Character = Cast<AStealthCharacter>(CharacterOwner))
		{
			if (Character->IsInformant())
			{
				Character->CancelTraversal();
				return;
			}
		}
	}
	Super::SetMovementMode(NewMovementMode, NewCustomMode);
}
