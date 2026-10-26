#pragma once

#include "CoreMinimal.h"
#include "View/Core/StealthPawnMovementComponent.h"
#include "StealthMovementComponent.generated.h"

/**
 * Movement component shared by players and NPCs. Adds the rule-driven speed caps
 * (informants walk only, attackers are held after a strike) on top of the Game Animation
 * Sample's own speed logic, so the caps apply identically in client prediction and on the server.
 *
 * Prototype class, renamed from UStealthMovementComponent when the production class of that name
 * landed in StealthGameplay (story SCL-001; ADR-0007 section 8). Config/DefaultEngine.ini carries
 * the [CoreRedirects] entry for Blueprints saved against the old name.
 */
UCLASS()
class STEALTHGAME_API UStealthPrototypeMovementComponent : public UStealthMovementComponent
{
	GENERATED_BODY()

public:
	UStealthPrototypeMovementComponent();

	virtual float GetMaxSpeed() const override;
	virtual void SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode = 0) override;
};
