#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthBush.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * The graybox bush planter (Concealment epic): the visual placeholder and the stand-in Manifest's BushSite (its footprint
 * centre; StealthConcealmentWiring GatherBushSites). Hiding is the production Hide command (UConcealmentService); the hide
 * volume is the local ABushVolumeSet's box. This actor has no gameplay of its own (the prototype's overlap-driven,
 * replicated concealment was retired); story con-035 deletes the class when the Level Socket Contract places bushes.
 */
UCLASS()
class STEALTHGAME_API AStealthBush : public AActor
{
	GENERATED_BODY()

public:
	AStealthBush();

	/**
	 * The bush's 2D footprint: the concealment volume's world-aligned bounds (the Assassination bush-spot stand-in's
	 * footprint until Concealment's FBushRuntime, ADR-0017).
	 */
	FBox2D GetFootprint2D() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	void ApplyFoliageColor();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Concealment volume. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stealth")
	TObjectPtr<UBoxComponent> Volume;

	/** Placeholder foliage visual with the one consistent "hideable bush" silhouette (art bible). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stealth")
	TObjectPtr<UStaticMeshComponent> Foliage;

	/** Foliage tint (placeholder until the art kit exists). */
	UPROPERTY(EditAnywhere, Category = "Stealth")
	FLinearColor FoliageColor = FLinearColor(0.12f, 0.28f, 0.10f);
};
