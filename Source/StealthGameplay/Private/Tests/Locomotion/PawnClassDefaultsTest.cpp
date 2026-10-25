// Story SCL-001 (production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md):
// one Shared Pawn class with an identical default subobject set (ADR-0010 section 2, E2; SCL R1, R3).
// Test StealthGame.Locomotion.PawnClassDefaults: class shape and defaults on the CDO, subobject
// parity across a player-possessed and an AI-possessed instance (AI possession adds nothing), the
// WarpTargets lifetime row is COND_Never, and Restart()/PawnClientRestart() keep velocity, movement
// mode and the pending input vector. AStealthCrowdController does not exist yet (Crowd epic), so
// AAIController stands in for it: its OnPossess is the path that would add a GameplayTasksComponent.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTasksComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "View/Core/StealthMotionWarpingComponent.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace PawnClassDefaultsTestData
{
	/** One row of the subobject fixture: (component class, name, replicates). */
	struct FComponentRow
	{
		FString ClassName;
		FString Name;
		bool bReplicates = false;

		bool operator==(const FComponentRow& Other) const
		{
			return ClassName == Other.ClassName && Name == Other.Name && bReplicates == Other.bReplicates;
		}
		FString ToString() const { return FString::Printf(TEXT("(%s, %s, %s)"), *ClassName, *Name, bReplicates ? TEXT("rep") : TEXT("local")); }
	};

	/** Restart fixture: a walking body at 140 cm/s (ADR-0010 E2 QA case). */
	const FVector RestartVelocity(140.0, 0.0, 0.0);
	const FVector PendingInput(1.0, 0.0, 0.0);

	/** The pawn's components, sorted by name so two instances compare as a canonical list. */
	TArray<FComponentRow> ComponentRows(const AActor& Actor)
	{
		TArray<FComponentRow> Rows;
		for (const UActorComponent* Component : Actor.GetComponents())
		{
			Rows.Add({ Component->GetClass()->GetName(), Component->GetName(), Component->GetIsReplicated() });
		}
		Rows.Sort([](const FComponentRow& A, const FComponentRow& B) { return A.Name < B.Name; });
		return Rows;
	}

	FString Describe(const TArray<FComponentRow>& Rows)
	{
		FString Out;
		for (const FComponentRow& Row : Rows)
		{
			Out += Row.ToString() + TEXT(" ");
		}
		return Out;
	}

	/** A bare game world for the test; torn down by the destructor. */
	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PawnClassDefaultsTestWorld"));
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

	/** The lifetime condition of Property on Class, or COND_Max when it has no lifetime row. */
	ELifetimeCondition LifetimeConditionOf(const UClass* Class, FName Property)
	{
		// ClassReps is built lazily (normally by the first net driver that replicates the class).
		const_cast<UClass*>(Class)->SetUpRuntimeReplicationData();
		TArray<FLifetimeProperty> Props;
		Class->GetDefaultObject()->GetLifetimeReplicatedProps(Props);
		for (const FLifetimeProperty& Row : Props)
		{
			if (!Class->ClassReps.IsValidIndex(Row.RepIndex))
			{
				continue;
			}
			const FRepRecord& Record = Class->ClassReps[Row.RepIndex];
			if (Record.Property->GetFName() == Property)
			{
				return Row.Condition;
			}
		}
		return COND_Max;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionPawnClassDefaultsTest, "StealthGame.Locomotion.PawnClassDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionPawnClassDefaultsTest::RunTest(const FString& Parameters)
{
	using namespace PawnClassDefaultsTestData;

	// --- Class shape and defaults, on the CDO ---------------------------------------------------
	const AStealthPawn* Cdo = GetDefault<AStealthPawn>();
	TestTrue(TEXT("AStealthPawn derives from ACharacter"), AStealthPawn::StaticClass()->IsChildOf(ACharacter::StaticClass()));
	TestTrue(TEXT("Movement component is UStealthMovementComponent"), Cdo->GetCharacterMovement() && Cdo->GetCharacterMovement()->IsA<UStealthMovementComponent>());
	TestTrue(TEXT("UStealthMovementComponent derives from the CMC"), UStealthMovementComponent::StaticClass()->IsChildOf(UCharacterMovementComponent::StaticClass()));
	TestFalse(TEXT("bUseControllerRotationYaw = false"), Cdo->bUseControllerRotationYaw);
	TestTrue(TEXT("bOrientRotationToMovement = true"), Cdo->GetCharacterMovement()->bOrientRotationToMovement);
	TestFalse(TEXT("bUseRVOAvoidance = false"), Cdo->GetCharacterMovement()->bUseRVOAvoidance);
	TestTrue(TEXT("AutoPossessAI = Disabled"), Cdo->AutoPossessAI == EAutoPossessAI::Disabled);
	TestTrue(TEXT("AutoPossessPlayer = Disabled"), Cdo->AutoPossessPlayer == EAutoReceiveInput::Disabled);
	TestTrue(TEXT("CDO carries a UGameplayTasksComponent"), Cdo->GetGameplayTasks() != nullptr);
	TestTrue(TEXT("CDO carries a UStealthMotionWarpingComponent"), Cdo->GetMotionWarping() != nullptr && Cdo->GetMotionWarping()->IsA<UStealthMotionWarpingComponent>());

	// --- WarpTargets: the lifetime row exists and is COND_Never ----------------------------------
	TestTrue(TEXT("UStealthMotionWarpingComponent.WarpTargets lifetime condition is COND_Never"),
		LifetimeConditionOf(UStealthMotionWarpingComponent::StaticClass(), TEXT("WarpTargets")) == COND_Never);
	// Control: the engine parent still replicates it (the subclass is what disables it).
	const ELifetimeCondition ParentCondition = LifetimeConditionOf(UMotionWarpingComponent::StaticClass(), TEXT("WarpTargets"));
	TestTrue(TEXT("Control: UMotionWarpingComponent.WarpTargets has a replicated lifetime row in the engine parent"),
		ParentCondition != COND_Never && ParentCondition != COND_Max);

	// --- Two instances: subobject parity, AI possession adds nothing ------------------------------
	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.World;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStealthPawn* PlayerBody = World->SpawnActor<AStealthPawn>(FVector(0.0, 0.0, 200.0), FRotator::ZeroRotator, Params);
	AStealthPawn* CrowdBody = World->SpawnActor<AStealthPawn>(FVector(500.0, 0.0, 200.0), FRotator::ZeroRotator, Params);
	if (!TestTrue(TEXT("Two pawns spawned"), PlayerBody != nullptr && CrowdBody != nullptr))
	{
		return false;
	}

	TestNull(TEXT("No controller is spawned for a pawn by the engine (auto-possession disabled)"), CrowdBody->GetController());
	const TArray<FComponentRow> CrowdBefore = ComponentRows(*CrowdBody);

	// SpawnDefaultController must do nothing even when an AI controller class is set.
	CrowdBody->AIControllerClass = AAIController::StaticClass();
	CrowdBody->SpawnDefaultController();
	TestNull(TEXT("SpawnDefaultController spawns and possesses nothing"), CrowdBody->GetController());
	CrowdBody->AIControllerClass = nullptr;

	// Velocity before AI possession, to observe that possession (which runs Restart) keeps it.
	CrowdBody->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	CrowdBody->GetCharacterMovement()->Velocity = RestartVelocity;

	APlayerController* PlayerStandIn = World->SpawnActor<APlayerController>(Params);
	AAIController* CrowdStandIn = World->SpawnActor<AAIController>(Params);
	PlayerStandIn->Possess(PlayerBody);
	CrowdStandIn->Possess(CrowdBody);
	TestTrue(TEXT("Player stand-in possesses its body"), PlayerBody->GetController() == PlayerStandIn);
	TestTrue(TEXT("AI stand-in possesses its body"), CrowdBody->GetController() == CrowdStandIn);

	const TArray<FComponentRow> PlayerRows = ComponentRows(*PlayerBody);
	const TArray<FComponentRow> CrowdRows = ComponentRows(*CrowdBody);
	TestTrue(FString::Printf(TEXT("Same (class, name, replicates) list on both instances: player [%s] crowd [%s]"), *Describe(PlayerRows), *Describe(CrowdRows)),
		PlayerRows == CrowdRows);
	TestTrue(FString::Printf(TEXT("AI possession adds no component: before [%s] after [%s]"), *Describe(CrowdBefore), *Describe(CrowdRows)),
		CrowdBefore == CrowdRows);

	for (const AStealthPawn* Body : { PlayerBody, CrowdBody })
	{
		TArray<UGameplayTasksComponent*> TaskComponents;
		Body->GetComponents(TaskComponents);
		TestEqual(TEXT("Exactly one UGameplayTasksComponent"), TaskComponents.Num(), 1);
		TestTrue(TEXT("The UGameplayTasksComponent does not replicate"), TaskComponents.Num() == 1 && !TaskComponents[0]->GetIsReplicated());
		TestTrue(TEXT("It is the pawn's default subobject"), TaskComponents.Num() == 1 && TaskComponents[0] == Body->GetGameplayTasks());
		TestTrue(TEXT("Motion warping is UStealthMotionWarpingComponent"), Body->GetMotionWarping() != nullptr);
	}
	TestEqual(TEXT("AI possession (Restart) kept the velocity"), CrowdBody->GetCharacterMovement()->Velocity, RestartVelocity);
	TestTrue(TEXT("AI possession (Restart) kept MOVE_Walking"), CrowdBody->GetCharacterMovement()->MovementMode == MOVE_Walking);

	// --- Restart(): velocity, movement mode and pending input unchanged ---------------------------
	UCharacterMovementComponent* Movement = PlayerBody->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = RestartVelocity;
	PlayerBody->AddMovementInput(PendingInput, 1.0f, true);
	TestEqual(TEXT("Arrange: pending input recorded"), PlayerBody->GetPendingMovementInputVector(), PendingInput);

	PlayerBody->Restart();
	TestEqual(TEXT("Restart keeps Velocity (140,0,0)"), Movement->Velocity, RestartVelocity);
	TestTrue(TEXT("Restart keeps MOVE_Walking"), Movement->MovementMode == MOVE_Walking);
	TestEqual(TEXT("Restart does not consume the pending input vector"), PlayerBody->GetPendingMovementInputVector(), PendingInput);

	// The client restart path (DispatchRestart(true) -> PawnClientRestart -> Restart -> NotifyRestarted).
	PlayerBody->DispatchRestart(true);
	TestEqual(TEXT("PawnClientRestart keeps Velocity"), Movement->Velocity, RestartVelocity);
	TestTrue(TEXT("PawnClientRestart keeps MOVE_Walking"), Movement->MovementMode == MOVE_Walking);

	// Control: the engine path this override replaces does zero the velocity.
	Movement->StopMovementImmediately();
	TestEqual(TEXT("Control: StopMovementImmediately (what ACharacter::PawnClientRestart calls) zeroes velocity"), Movement->Velocity, FVector::ZeroVector);

	// --- Secondary: source scan for a Super/ACharacter Restart call -------------------------------
	FString RawSource;
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp"));
	if (TestTrue(TEXT("StealthPawn.cpp is readable"), FFileHelper::LoadFileToString(RawSource, *SourcePath)))
	{
		// Code only: line comments name the skipped engine calls on purpose.
		TArray<FString> Lines;
		RawSource.ParseIntoArrayLines(Lines, false);
		FString Source;
		for (const FString& Line : Lines)
		{
			const int32 Comment = Line.Find(TEXT("//"));
			Source += (Comment == INDEX_NONE ? Line : Line.Left(Comment)) + TEXT(" ");
		}
		TestFalse(TEXT("No Super::Restart call"), Source.Contains(TEXT("Super::Restart")));
		TestFalse(TEXT("No ACharacter::Restart / ACharacter::PawnClientRestart call"), Source.Contains(TEXT("ACharacter::Restart")) || Source.Contains(TEXT("ACharacter::PawnClientRestart")));
		TestFalse(TEXT("No SetDefaultMovementMode call"), Source.Contains(TEXT("SetDefaultMovementMode(")));
		TestFalse(TEXT("DispatchRestart/NotifyRestarted are not overridden (ReceiveRestarted still fires)"), Source.Contains(TEXT("::NotifyRestarted")) || Source.Contains(TEXT("::DispatchRestart")));
	}

	PlayerStandIn->UnPossess();
	CrowdStandIn->UnPossess();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
