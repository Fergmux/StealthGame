#pragma once

// Shared fixture of the SCL movement integration tests (stories SCL-009 to SCL-018): a bare game world with a floor,
// Shared Pawns spawned without a controller (bRunPhysicsWithNoController) and stepped at a fixed 1/60 s by ticking the
// pawn command component and the movement component directly, so every run is deterministic. Two drive paths:
//   - the owner (player) path: the input vector a client feeds its predicted pawn (AddMovementInput + requested gait),
//     which is what ServerMove carries;
//   - the NPC path: UPawnCommandComponent::Move(Direction, Gait), the entry point the crowd AI calls.
// Test-only (never shipped).

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace StealthLocomotionTest
{
	/** The fixed simulation step (60 Hz, the AC frame). */
	inline constexpr float Step = 1.0f / 60.0f;

	/** A bare game world with a 200 m floor whose top is at Z 0. */
	struct FWorld
	{
		UWorld* World = nullptr;

		explicit FWorld(const TCHAR* Name)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, Name);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			SpawnBox(FVector(0.0, 0.0, -20.0), FVector(20000.0, 20000.0, 20.0));
		}

		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		/** A static box: Size in cm, its bottom-centre at Base. */
		AStaticMeshActor* SpawnBox(const FVector& Base, const FVector& Size)
		{
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Box = World->SpawnActor<AStaticMeshActor>(Base + FVector(0.0, 0.0, Size.Z * 0.5), FRotator::ZeroRotator, Params);
			Box->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Box->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Box->SetActorScale3D(Size / 100.0);
			Box->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			return Box;
		}

		/** A Shared Pawn standing on the floor at XY, facing Yaw, walking, driven without a controller. */
		AStealthPawn* SpawnPawn(const FVector2D& XY, float YawDeg = 0.0f)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStealthPawn* Pawn = World->SpawnActor<AStealthPawn>(FVector(XY.X, XY.Y, AStealthPawn::SampleCapsuleHalfHeightCm + 1.0), FRotator(0.0, YawDeg, 0.0), Params);
			UStealthMovementComponent* Movement = Pawn->GetStealthMovement();
			Movement->bRunPhysicsWithNoController = true;
			Movement->SetMovementMode(MOVE_Walking);
			return Pawn;
		}
	};

	/** World-space input for a direction at Yaw degrees with magnitude M (the owner path). */
	inline FVector WorldInput(float YawDeg, float Magnitude)
	{
		return FRotator(0.0, YawDeg, 0.0).Vector() * Magnitude;
	}

	/** Move Direction (X right, Y forward relative to the zero control yaw of a controller-less pawn) for a world yaw. */
	inline FVector2D MoveDirection(float YawDeg, float Magnitude)
	{
		const FVector World = WorldInput(YawDeg, Magnitude);
		return FVector2D(World.Y, World.X);
	}

	/** Owner path: the input vector and requested gait for this frame. */
	inline void DriveOwner(AStealthPawn* Pawn, float YawDeg, float Magnitude, EGait Gait)
	{
		Pawn->GetStealthMovement()->SetRequestedGait(Gait);
		if (Magnitude > 0.0f)
		{
			Pawn->AddMovementInput(WorldInput(YawDeg, Magnitude), 1.0f, /*bForce*/ true);
		}
	}

	/** NPC path: the command entry point (held intent, re-applied every tick by the command component). */
	inline void DriveMove(AStealthPawn* Pawn, float YawDeg, float Magnitude, EGait Gait)
	{
		Pawn->GetPawnCommand()->Move(MoveDirection(YawDeg, Magnitude), Gait);
	}

	/** One fixed step of Pawn: command component (applies a held Move intent) then the movement component. */
	inline void StepPawn(AStealthPawn* Pawn, float Dt = Step)
	{
		UPawnCommandComponent* Command = Pawn->GetPawnCommand();
		if (!Command->GetMoveIntent().IsNearlyZero())
		{
			Command->TickComponent(Dt, LEVELTICK_All, &Command->PrimaryComponentTick);
		}
		UStealthMovementComponent* Movement = Pawn->GetStealthMovement();
		Movement->TickComponent(Dt, LEVELTICK_All, &Movement->PrimaryComponentTick);
	}

	inline double Speed2D(const AActor* Actor)
	{
		return Actor->GetVelocity().Size2D();
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
