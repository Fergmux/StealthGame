// Story SCL-002 (production/epics/shared-character-locomotion/story-002-body-constants-published.md), AC-SCL-48 /
// R17.1: the Shared Pawn class defaults publish MaxStepHeight = TraversalMinHeightCm (45), the engine-default walkable
// floor angle (44.765 degrees) and the capsule recorded from the reparented sample character (CBP_SandboxCharacter,
// measured in the 5.8.4 project and committed as AStealthPawn::SampleCapsule*); in a test world a 40 cm kerb is
// stepped onto while 46 and 69 cm blocks stop the pawn; ClassifyCopingHeight says vault at 80 cm and step-or-block at
// 45, 46 and 69 cm. (Exactly 45 cm is a boundary the AC does not state; the in-world 80 cm vault is story SCL-047.)

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace BodyConstantsPublishedTestData
{
	/** AC-SCL-48 values. */
	constexpr float ExpectedMaxStepHeightCm = 45.0f;
	constexpr float ExpectedWalkableAngleDeg = 44.765f;
	constexpr float AngleToleranceDeg = 0.001f;
	/** The obstacle heights the AC names. */
	constexpr float KerbCm = 40.0f;
	constexpr float LowBlockCm = 46.0f;
	constexpr float HighBlockCm = 69.0f;
	constexpr float VaultCopingCm = 80.0f;
	/** The step boundary (classifier only). */
	constexpr float StepBoundaryCm = 45.0f;

	const TCHAR* const SampleCharacterClass = TEXT("/Game/Blueprints/CBP_SandboxCharacter.CBP_SandboxCharacter_C");
	const TCHAR* const CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");

	/** Lane layout: the pawn starts at X 0 and walks +X; the obstacle front face is at ObstacleStartX. */
	constexpr double ObstacleStartX = 200.0;
	constexpr double ObstacleDepth = 400.0;
	constexpr double LaneSpacing = 600.0;
	constexpr int32 SimFrames = 180;
	constexpr float FrameSeconds = 1.0f / 60.0f;

	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BodyConstantsTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}

		~FScopedTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	/** A static box: Size in cm, its bottom-centre at Base. */
	AStaticMeshActor* SpawnBox(UWorld* World, UStaticMesh* Cube, const FVector& Base, const FVector& Size)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Box = World->SpawnActor<AStaticMeshActor>(Base + FVector(0.0, 0.0, Size.Z * 0.5), FRotator::ZeroRotator, Params);
		Box->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Box->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Box->SetActorScale3D(Size / 100.0);
		Box->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		return Box;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionBodyConstantsPublishedTest, "StealthGame.Locomotion.BodyConstantsPublished",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionBodyConstantsPublishedTest::RunTest(const FString& Parameters)
{
	using namespace BodyConstantsPublishedTestData;

	// --- Published constants from the class defaults ------------------------------------------------------------
	const FStealthBodyConstants Body = AStealthPawn::GetPublishedBodyConstants();
	const FStealthTraversalHeights Heights;
	TestEqual(TEXT("TraversalMinHeightCm default is 45 (provisional: a changed value must fail here)"), Heights.MinHeightCm, ExpectedMaxStepHeightCm);
	TestEqual(TEXT("MaxStepHeight equals TraversalMinHeightCm"), Body.MaxStepHeightCm, Heights.MinHeightCm);
	TestTrue(TEXT("walkable floor angle is the engine default 44.765"), FMath::IsNearlyEqual(Body.WalkableFloorAngleDeg, ExpectedWalkableAngleDeg, AngleToleranceDeg));
	TestEqual(TEXT("published capsule radius is the recorded class default"), Body.CapsuleRadiusCm, AStealthPawn::SampleCapsuleRadiusCm);
	TestEqual(TEXT("published capsule half-height is the recorded class default"), Body.CapsuleHalfHeightCm, AStealthPawn::SampleCapsuleHalfHeightCm);

	// The recorded values are the reparented sample character's (R17.1 [verify] resolved against the project).
	if (UClass* Sample = LoadObject<UClass>(nullptr, SampleCharacterClass))
	{
		const ACharacter* SampleDefaults = Cast<ACharacter>(Sample->GetDefaultObject());
		if (TestNotNull(TEXT("CBP_SandboxCharacter is a character"), SampleDefaults))
		{
			const float Radius = SampleDefaults->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
			const float HalfHeight = SampleDefaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
			AddInfo(FString::Printf(TEXT("CBP_SandboxCharacter capsule: radius %.3f, half-height %.3f"), Radius, HalfHeight));
			TestEqual(TEXT("sample character capsule radius equals the published radius"), Radius, Body.CapsuleRadiusCm);
			TestEqual(TEXT("sample character capsule half-height equals the published half-height"), HalfHeight, Body.CapsuleHalfHeightCm);
			TestTrue(TEXT("sample character is a Shared Pawn"), Sample->IsChildOf(AStealthPawn::StaticClass()));
		}
	}
	else
	{
		AddError(TEXT("CBP_SandboxCharacter class did not load"));
	}

	// --- Height bands ------------------------------------------------------------------------------------------------
	TestTrue(TEXT("80 cm coping is a vault"), StealthLocomotion::ClassifyCopingHeight(VaultCopingCm, Heights) == EStealthCopingClass::Vault);
	TestTrue(TEXT("40 cm kerb is a step"), StealthLocomotion::ClassifyCopingHeight(KerbCm, Heights) == EStealthCopingClass::Step);
	TestTrue(TEXT("45 cm is not a vault"), StealthLocomotion::ClassifyCopingHeight(StepBoundaryCm, Heights) != EStealthCopingClass::Vault);
	TestTrue(TEXT("46 cm blocks"), StealthLocomotion::ClassifyCopingHeight(LowBlockCm, Heights) == EStealthCopingClass::Block);
	TestTrue(TEXT("69 cm blocks"), StealthLocomotion::ClassifyCopingHeight(HighBlockCm, Heights) == EStealthCopingClass::Block);

	// --- World walk: 40 cm kerb stepped onto, 46 and 69 cm blocks stop the pawn ----------------------------------------
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubeMesh);
	if (!TestNotNull(TEXT("engine cube mesh loads"), Cube))
	{
		return false;
	}
	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.World;
	SpawnBox(World, Cube, FVector(0.0, LaneSpacing, -20.0), FVector(4000.0, 4000.0, 20.0)); // floor, top at Z 0

	const float Obstacles[] = { KerbCm, LowBlockCm, HighBlockCm };
	TArray<AStealthPawn*> Pawns;
	for (int32 Lane = 0; Lane < UE_ARRAY_COUNT(Obstacles); ++Lane)
	{
		const double Y = Lane * LaneSpacing;
		SpawnBox(World, Cube, FVector(ObstacleStartX + ObstacleDepth * 0.5, Y, 0.0), FVector(ObstacleDepth, 300.0, Obstacles[Lane]));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStealthPawn* Pawn = World->SpawnActor<AStealthPawn>(FVector(0.0, Y, Body.CapsuleHalfHeightCm + 2.0), FRotator::ZeroRotator, Params);
		Pawn->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Pawns.Add(Pawn);
	}
	// The movement component is ticked directly (the bare world never begins play, so no tick function is registered
	// and no world subsystem starts); collision queries run against the world's physics scene as in a round.
	TArray<double> MaxZ;
	MaxZ.Init(0.0, Pawns.Num());
	for (int32 Frame = 0; Frame < SimFrames; ++Frame)
	{
		for (int32 Lane = 0; Lane < Pawns.Num(); ++Lane)
		{
			AStealthPawn* Pawn = Pawns[Lane];
			MaxZ[Lane] = FMath::Max(MaxZ[Lane], Pawn->GetActorLocation().Z);
			Pawn->AddMovementInput(FVector::ForwardVector, 1.0f, /*bForce*/ true);
			UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
			Movement->TickComponent(FrameSeconds, LEVELTICK_All, &Movement->PrimaryComponentTick);
		}
	}

	const FVector Kerb = Pawns[0]->GetActorLocation();
	TestTrue(FString::Printf(TEXT("40 cm kerb: the pawn passed the front face (X %.1f)"), Kerb.X), Kerb.X > ObstacleStartX + Body.CapsuleRadiusCm);
	TestTrue(FString::Printf(TEXT("40 cm kerb: the pawn stepped up onto it (max Z %.1f)"), MaxZ[0]), MaxZ[0] > KerbCm + Body.CapsuleHalfHeightCm - 5.0);
	for (int32 Lane = 1; Lane < UE_ARRAY_COUNT(Obstacles); ++Lane)
	{
		const FVector Blocked = Pawns[Lane]->GetActorLocation();
		TestTrue(FString::Printf(TEXT("%.0f cm block: the pawn did not pass the front face (X %.1f)"), Obstacles[Lane], Blocked.X), Blocked.X < ObstacleStartX);
		TestTrue(FString::Printf(TEXT("%.0f cm block: the pawn never climbed it (max Z %.1f)"), Obstacles[Lane], MaxZ[Lane]), MaxZ[Lane] < Body.CapsuleHalfHeightCm + 10.0);
	}
	TestTrue(TEXT("control: the pawns moved at all"), Pawns[1]->GetActorLocation().X > 50.0);
	return true;
}

#endif
