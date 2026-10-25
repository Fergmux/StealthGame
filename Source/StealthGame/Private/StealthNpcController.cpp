#include "StealthNpcController.h"
#include "StealthSettings.h"

#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

AStealthNpcController::AStealthNpcController()
{
	bWantsPlayerState = false;
}

void AStealthNpcController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// Stagger the first walk so the crowd does not start in lockstep.
	ScheduleNextWander(0.2f);
}

void AStealthNpcController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(WanderTimer);
	Super::OnUnPossess();
}

void AStealthNpcController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	ScheduleNextWander();
}

void AStealthNpcController::ScheduleNextWander(float MinDelay)
{
	const FVector2D Range = UStealthSettings::Get()->NpcIdleTimeRange;
	const float Delay = MinDelay >= 0.f ? FMath::FRandRange(MinDelay, FMath::Max(MinDelay, Range.Y)) : FMath::FRandRange(Range.X, FMath::Max(Range.X, Range.Y));
	GetWorldTimerManager().SetTimer(WanderTimer, this, &AStealthNpcController::WanderToNextPoint, FMath::Max(0.05f, Delay), false);
}

void AStealthNpcController::WanderToNextPoint()
{
	APawn* ControlledPawn = GetPawn();
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!ControlledPawn || !NavSystem)
	{
		ScheduleNextWander(1.f);
		return;
	}

	FNavLocation Destination;
	if (!NavSystem->GetRandomReachablePointInRadius(WanderCenter, WanderRadius, Destination))
	{
		ScheduleNextWander(1.f);
		return;
	}

	// bCanStrafe = false: the NPC faces its direction of travel, as a walking player does.
	const EPathFollowingRequestResult::Type Request = MoveToLocation(Destination.Location, 60.f, true, true, false, false);
	if (Request == EPathFollowingRequestResult::Failed)
	{
		ScheduleNextWander(1.f);
	}
	else if (Request == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		ScheduleNextWander();
	}
}
