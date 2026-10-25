#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "StealthNpcController.generated.h"

/**
 * Crowd citizen: walks between random reachable points around the crowd centre and idles in
 * between. It drives the same AStealthCharacter pawn players use, through the same movement
 * component, so its locomotion is produced by the same motion-matching setup.
 */
UCLASS()
class STEALTHGAME_API AStealthNpcController : public AAIController
{
	GENERATED_BODY()

public:
	AStealthNpcController();

	/** Centre and radius of the area this NPC wanders in (set by the game mode before possession). */
	FVector WanderCenter = FVector::ZeroVector;
	float WanderRadius = 3000.f;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** Picks a new destination and walks there. */
	void WanderToNextPoint();

	/** Idles for a random time from UStealthSettings::NpcIdleTimeRange, then wanders. */
	void ScheduleNextWander(float MinDelay = -1.f);

	FTimerHandle WanderTimer;
};
