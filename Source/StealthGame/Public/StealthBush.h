#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthBush.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Designated hiding place. A character inside is invisible to enemy viewers (hidden per viewer on
 * each client; teammates still see them). A concealed character can still be blind-attacked by an
 * enemy who strikes at their spot. Every character (NPCs included) is concealed the same way, so
 * entering a bush is not by itself proof of being a player.
 */
UCLASS()
class STEALTHGAME_API AStealthBush : public AActor
{
	GENERATED_BODY()

public:
	AStealthBush();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	void ApplyFoliageColor();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

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
