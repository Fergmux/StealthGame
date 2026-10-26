// Story SCL-018 (production/epics/shared-character-locomotion/story-018-ai-path-follow-routes-through-move.md), the live
// crowd half: the prototype crowd controller AStealthNpcController (rewritten as a Move-only follower) walks a cornered
// route on a Shared Pawn with only Move input: 0 movement backstop hits (no RequestDirectMove / RequestPathMove), the
// held Move magnitude is always the leg pace, it finishes the route and stops by the baseline near the goal, and a
// blocked route is dropped by the progress watchdog (the walker then stands). The AI controller and the pawn are ticked
// directly at a fixed 1/60 s in a bare game world (the controller keeps its control rotation on the pawn orientation,
// as in play).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "StealthNpcController.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace NpcControllerMoveOnlyTestData
{
	constexpr float Step = 1.0f / 60.0f;

	struct FTestWorld
	{
		UWorld* World = nullptr;
		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("NpcControllerMoveOnlyTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			Box(FVector(0.0, 0.0, -20.0), FVector(20000.0, 20000.0, 20.0));
		}
		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		void Box(const FVector& Base, const FVector& Size)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Base + FVector(0.0, 0.0, Size.Z * 0.5), FRotator::ZeroRotator, Params);
			Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Actor->SetActorScale3D(Size / 100.0);
			Actor->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
	};

	void Tick(AStealthNpcController* Controller, AStealthPawn* Pawn)
	{
		Controller->Tick(Step);
		UPawnCommandComponent* Command = Pawn->GetPawnCommand();
		if (!Command->GetMoveIntent().IsNearlyZero())
		{
			Command->TickComponent(Step, LEVELTICK_All, &Command->PrimaryComponentTick);
		}
		UStealthMovementComponent* Movement = Pawn->GetStealthMovement();
		Movement->TickComponent(Step, LEVELTICK_All, &Movement->PrimaryComponentTick);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionNpcControllerMoveOnlyTest, "StealthGame.Locomotion.NpcControllerMoveOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionNpcControllerMoveOnlyTest::RunTest(const FString& Parameters)
{
	using namespace NpcControllerMoveOnlyTestData;
	FTestWorld TestWorld;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStealthPawn* Pawn = TestWorld.World->SpawnActor<AStealthPawn>(FVector(0.0, 0.0, AStealthPawn::SampleCapsuleHalfHeightCm + 1.0), FRotator::ZeroRotator, Params);
	Pawn->GetStealthMovement()->SetMovementMode(MOVE_Walking);
	AStealthNpcController* Controller = TestWorld.World->SpawnActor<AStealthNpcController>();
	Controller->Possess(Pawn);

	// A cornered route walked at the slow pace.
	const TArray<FVector> Route = { FVector(800.0, 0.0, 0.0), FVector(800.0, 600.0, 0.0), FVector(200.0, 600.0, 0.0) };
	const float Pace = 0.8f;
	Controller->FollowRoute(Route, Pace);
	int32 OffPace = 0;
	int32 Frames = 0;
	for (; Frames < 60 * 40 && (Controller->IsFollowingRoute() || Pawn->GetVelocity().Size2D() > 0.0); ++Frames)
	{
		Tick(Controller, Pawn);
		const double Magnitude = Pawn->GetPawnCommand()->GetMoveIntent().Size();
		OffPace += (Magnitude > 1e-3 && FMath::Abs(Magnitude - Pace) > 1e-3) ? 1 : 0;
	}
	const double ToGoal = FVector::Dist2D(Pawn->GetActorLocation(), Route.Last());
	AddInfo(FString::Printf(TEXT("NPCCONTROLLER route done in %.1f s, %.1f cm from the goal, backstop hits %d, off-pace frames %d"),
		Frames * Step, ToGoal, Pawn->GetStealthMovement()->GetBackstopHitCount(), OffPace));
	TestFalse(TEXT("the route is finished"), Controller->IsFollowingRoute());
	TestTrue(TEXT("the walker is at rest"), Pawn->GetVelocity().Size2D() <= 0.0);
	TestTrue(FString::Printf(TEXT("it stopped near the goal (%.1f cm)"), ToGoal), ToGoal <= 15.0);
	TestEqual(TEXT("0 direct velocity or path move requests"), Pawn->GetStealthMovement()->GetBackstopHitCount(), 0);
	TestEqual(TEXT("every held Move is the leg pace"), OffPace, 0);

	// A blocked route: a wall across the way; the watchdog drops the route and the walker stands.
	TestWorld.Box(FVector(200.0 + 150.0, 600.0, 0.0), FVector(40.0, 600.0, 300.0));
	Controller->FollowRoute({ FVector(1000.0, 600.0, 0.0) }, 1.0f);
	for (int32 Frame = 0; Frame < 60 * 8; ++Frame)
	{
		Tick(Controller, Pawn);
	}
	TestFalse(TEXT("blocked: the watchdog dropped the route"), Controller->IsFollowingRoute());
	TestTrue(TEXT("blocked: the walker stands"), Pawn->GetVelocity().Size2D() <= 0.0);
	TestEqual(TEXT("blocked: still no backstop hit"), Pawn->GetStealthMovement()->GetBackstopHitCount(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
