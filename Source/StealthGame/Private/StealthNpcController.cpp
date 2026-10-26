#include "StealthNpcController.h"
#include "StealthSettings.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "TimerManager.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"
#include "View/Core/WalkSlowerScale.h"

namespace StealthNpcControllerLocal
{
	/** A corner counts as reached within this horizontal distance (the next corner then becomes the target). */
	constexpr double CornerReachCm = 60.0;
	/** Probability of each pace of a leg: full pace at least half the walking time (R5.1 full-pace rule). */
	constexpr float FullPaceShare = 0.6f;
	constexpr float MiddlePaceShare = 0.2f;
	/** A route that makes less than this much progress toward its next corner in StuckSeconds is dropped. */
	constexpr double StuckProgressCm = 30.0;
	constexpr float StuckSeconds = 3.0f;
}

AStealthNpcController::AStealthNpcController()
{
	bWantsPlayerState = false;
	PrimaryActorTick.bCanEverTick = true;
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
	ReleaseMove();
	Route.Reset();
	Super::OnUnPossess();
}

void AStealthNpcController::ScheduleNextWander(float MinDelay)
{
	const FVector2D Range = UStealthSettings::Get()->NpcIdleTimeRange;
	const float Delay = MinDelay >= 0.f ? FMath::FRandRange(MinDelay, FMath::Max(MinDelay, Range.Y)) : FMath::FRandRange(Range.X, FMath::Max(Range.X, Range.Y));
	GetWorldTimerManager().SetTimer(WanderTimer, this, &AStealthNpcController::WanderToNextPoint, FMath::Max(0.05f, Delay), false);
}

void AStealthNpcController::WanderToNextPoint()
{
	using namespace StealthNpcControllerLocal;
	APawn* ControlledPawn = GetPawn();
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (bHeld)
	{
		ScheduleNextWander(0.5f);
		return;
	}
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
	const UNavigationPath* Path = NavSystem->FindPathToLocationSynchronously(GetWorld(), ControlledPawn->GetActorLocation(), Destination.Location, ControlledPawn);
	if (Path == nullptr || !Path->IsValid() || Path->PathPoints.Num() < 2)
	{
		ScheduleNextWander(1.f);
		return;
	}
	TArray<FVector> Points(Path->PathPoints);
	Points.RemoveAt(0); // the start
	// The leg pace from the shared set (prototype draw; the Crowd epic owns the real pace distribution).
	const float Roll = FMath::FRand();
	const float Pace = Roll < FullPaceShare ? StealthPaceSet::Full : (Roll < FullPaceShare + MiddlePaceShare ? StealthPaceSet::Middle : StealthPaceSet::Slow);
	FollowRoute(Points, Pace);
}

void AStealthNpcController::FollowRoute(const TArray<FVector>& Points, float Pace)
{
	Route = Points;
	CurrentPace = Pace;
	bStopping = false;
	ProgressTimer = 0.0f;
	ProgressMark = GetPawn() != nullptr ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
	StepRoute();
}

void AStealthNpcController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Route.Num() > 0 && GetPawn() != nullptr)
	{
		// Watchdog: a blocked or orbiting walker drops its route and stops by the baseline (no other recovery motion).
		ProgressTimer += DeltaSeconds;
		if (ProgressTimer >= StealthNpcControllerLocal::StuckSeconds)
		{
			const bool bStuck = FVector::Dist2D(GetPawn()->GetActorLocation(), ProgressMark) < StealthNpcControllerLocal::StuckProgressCm;
			ProgressTimer = 0.0f;
			ProgressMark = GetPawn()->GetActorLocation();
			if (bStuck)
			{
				Route.Reset();
				ReleaseMove();
				bStopping = true;
			}
		}
	}
	if (Route.Num() > 0 || bStopping)
	{
		StepRoute();
	}
}

void AStealthNpcController::ReleaseMove()
{
	if (const AStealthPawn* Body = Cast<AStealthPawn>(GetPawn()))
	{
		if (UPawnCommandComponent* Command = Body->GetPawnCommand())
		{
			Command->Move(FVector2D::ZeroVector, EGait::Walk);
		}
	}
}

void AStealthNpcController::StepRoute()
{
	using namespace StealthNpcControllerLocal;
	const AStealthPawn* Body = Cast<AStealthPawn>(GetPawn());
	UPawnCommandComponent* Command = Body ? Body->GetPawnCommand() : nullptr;
	if (Command == nullptr)
	{
		Route.Reset();
		bStopping = false;
		ScheduleNextWander(1.f);
		return;
	}
	if (bStopping)
	{
		// Wait for the baseline stop to finish, then idle.
		if (Body->GetVelocity().Size2D() <= 1.0)
		{
			bStopping = false;
			CurrentPace = 0.0f;
			ScheduleNextWander();
		}
		return;
	}
	const FVector Here = Body->GetActorLocation();
	while (Route.Num() > 1 && FVector::Dist2D(Here, Route[0]) <= CornerReachCm)
	{
		Route.RemoveAt(0);
	}
	if (Route.Num() == 0)
	{
		return;
	}
	// Release at the F2 stop distance of the current speed so the stop to rest ends at the goal.
	const UStealthMovementComponent* Movement = Body->GetStealthMovement();
	const double Speed = Body->GetVelocity().Size2D();
	const double StopDistance = Movement != nullptr && Speed > 0.0
		? Speed * Speed / (2.0 * FMath::Max(StealthLocomotion::StopDeceleration(Movement->GetLocomotionTuning(), static_cast<float>(Speed), EGait::Walk), 1.0f))
		: 0.0;
	const double ToGoal = FVector::Dist2D(Here, Route.Last());
	if (Route.Num() == 1 && ToGoal <= FMath::Max(StopDistance, 5.0))
	{
		Route.Reset();
		ReleaseMove();
		bStopping = true;
		return;
	}
	// Move input is relative to the control yaw (X right, Y forward), like a player stick.
	const FVector ToCorner = (Route[0] - Here).GetSafeNormal2D();
	const double ControlYaw = GetControlRotation().Yaw;
	const FVector Local = FRotator(0.0, -ControlYaw, 0.0).RotateVector(ToCorner);
	Command->Move(FVector2D(Local.Y, Local.X) * CurrentPace, EGait::Walk);
}
