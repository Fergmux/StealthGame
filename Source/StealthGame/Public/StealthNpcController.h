#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "StealthNpcController.generated.h"

/**
 * Crowd citizen (prototype, pending the Crowd & NPC Behaviour follower of ADR-0016 section 4): walks between random
 * reachable points around the crowd centre and idles in between. Since story SCL-018 it moves its pawn ONLY through the
 * pawn command interface, Move(Direction, Walk), exactly as a player does (R5.7): it computes a navigation path and
 * steers toward each corner with Move input; the engine path follower never drives the pawn (the movement component
 * backstops RequestDirectMove / RequestPathMove). The walk pace is drawn per route leg from the shared pace set with
 * about 60 % of the legs at full pace (a prototype draw toward the R5.1 full-pace share, AC-SCL-36), and changes only at a leg boundary; Move is released at the
 * baseline stop distance so the F2 stop ends at the goal. A route that makes no progress for 3 s is dropped (the
 * walker stops by the baseline and wanders again later).
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

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Follows Points (world positions, the first being the next corner) at Pace with Move input only. Public so a test
	 * can drive the follower on a fixed route without the navigation system.
	 */
	void FollowRoute(const TArray<FVector>& Points, float Pace);

	/** True while a route is being followed. */
	bool IsFollowingRoute() const { return Route.Num() > 0; }

	/** The pace of the current route leg (0 when idle). */
	float GetCurrentPace() const { return CurrentPace; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Picks a new destination and walks there. */
	void WanderToNextPoint();

	/** Idles for a random time from UStealthSettings::NpcIdleTimeRange, then wanders. */
	void ScheduleNextWander(float MinDelay = -1.f);

	/** Sends Move(Direction, Walk) toward the next corner, releases it at the stop distance, and advances corners. */
	void StepRoute();

	/** Releases Move (the baseline stop follows). */
	void ReleaseMove();

	FTimerHandle WanderTimer;
	TArray<FVector> Route;
	float CurrentPace = 0.0f;
	bool bStopping = false;
	/** Progress watchdog: time since the last check and the position then. */
	float ProgressTimer = 0.0f;
	FVector ProgressMark = FVector::ZeroVector;
};
