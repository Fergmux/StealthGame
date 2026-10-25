#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StealthMovementComponent.generated.h"

/**
 * Movement component shared by players and NPCs. Adds the rule-driven speed caps
 * (informants walk only, attackers are held after a strike) on top of the Game Animation
 * Sample's own speed logic, so the caps apply identically in client prediction and on the server.
 */
UCLASS()
class STEALTHGAME_API UStealthMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UStealthMovementComponent();

	virtual float GetMaxSpeed() const override;
	virtual void SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode = 0) override;
};
