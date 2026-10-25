#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthTypes.h"
#include "StealthObjective.generated.h"

class AStealthCharacter;
class UStaticMeshComponent;

/**
 * Reusable objective definition: location (the actor), team eligibility, stage, interaction
 * requirement (hold interact in range for UStealthSettings::ObjectiveInteractTime) and completion
 * state. Stage 1-2 objectives belong to one team; the stage-3 objective is the shared final target.
 * While someone interacts, the objective visibly "works" for every viewer: interacting is a tell.
 */
UCLASS()
class STEALTHGAME_API AStealthObjective : public AActor
{
	GENERATED_BODY()

public:
	AStealthObjective();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** Team that may complete this objective; STEALTH_NO_TEAM = both teams (final objective). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stealth")
	uint8 OwningTeam = STEALTH_NO_TEAM;

	/** 1 or 2 for preliminary stages, 3 (STEALTH_FINAL_STAGE) for the final objective. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stealth", meta = (ClampMin = "1", ClampMax = "3"))
	int32 Stage = 1;

	/** Task text shown to the owning team, e.g. "Bribe the scribe". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stealth")
	FText DisplayName;

	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsCompleted() const { return bCompleted; }

	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsBeingWorked() const { return InteractorCount > 0; }

	/** Server: starts or stops an interaction by this character. */
	void SetInteractor(AStealthCharacter* Character, bool bInteracting);

	/** Server: back to the not-completed state for a new round. */
	void ResetObjective();

	/** Server: whether Character is close enough to interact. */
	bool IsInRange(const AStealthCharacter* Character) const;

protected:
	UFUNCTION()
	void OnRep_Visuals();

	void UpdateVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stealth")
	TObjectPtr<UStaticMeshComponent> Marker;

	UPROPERTY(ReplicatedUsing = OnRep_Visuals)
	bool bCompleted = false;

	UPROPERTY(ReplicatedUsing = OnRep_Visuals)
	int32 InteractorCount = 0;

	/** Server: held-interaction progress per character, seconds. */
	TMap<TWeakObjectPtr<AStealthCharacter>, float> Progress;

	float PulseTime = 0.f;
};
