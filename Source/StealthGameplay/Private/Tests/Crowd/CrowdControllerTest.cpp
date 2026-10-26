// Story cnb-006 (production/epics/crowd-npc-behaviour/story-006-crowd-controller-and-look-pitch.md): the one crowd
// AIController (ADR-0016 sections 1, 2, 4.4; engine facts E1-E4, E13).
//   Shape        no PathFollowingComponent, bWantsPlayerState and bStartAILogicOnPossess false, the crowd StateTree
//                component in TG_PrePhysics; OnPossess asks the Director for the pending driver (Full, or Restricted
//                after DriveBoundPawn).
//   LookPitch    a Look of pitch -15 survives UpdateControlRotation (the stock AAIController zeroes it: negative
//                control), with no focal point.
//   TickOrder    Director -> StateTree component -> controller prerequisites hold at runtime.
//   ComponentParity  AI possession adds no component to the pawn (inventory before and after OnPossess identical).
// The observer half of the pitch criterion (RemoteViewPitch on a client) is the PIE test
// StealthGame.Crowd.PIE.LookPitchReachesObserver (StealthTestHarness).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/CrowdDirector.h"
#include "Server/CrowdStateTreeComponent.h"
#include "Server/StealthCrowdController.h"
#include "View/Core/StealthPawn.h"

namespace CrowdControllerTestData
{
	/** Factory hooks of the test world: no appearance, no binding, the crowd controller for an Ai pawn (optional). */
	class FHooks final : public IStealthPawnFactoryHooks
	{
	public:
		bool bCreateController = true;
		virtual void ApplyAppearance(AStealthPawn* Pawn, int32 AppearanceId) override {}
		virtual ERosterResult BindPawn(FParticipantId Participant, AStealthPawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch) override { return ERosterResult::NoOp_NotBound; }
		virtual AController* CreateAiController(AStealthPawn* Pawn) override
		{
			return bCreateController ? Pawn->GetWorld()->SpawnActor<AStealthCrowdController>() : nullptr;
		}
	};

	/** A game world with a pawn factory carrying the hooks (owner = a transient object kept alive by the test). */
	struct FTestWorld
	{
		UWorld* World = nullptr;
		FHooks Hooks;
		UObject* HooksOwner = nullptr;
		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CrowdControllerTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			HooksOwner = World; // the hooks live as long as the test world
			UStealthPawnFactory::Get(World)->SetHooks(&Hooks, HooksOwner);
			UStealthPawnFactory::Get(World)->SetDefaultPawnClass(AStealthPawn::StaticClass());
		}
		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		AStealthPawn* Spawn(const FVector& Location)
		{
			FStealthSpawnRequest Request;
			Request.Transform = FTransform(Location + FVector(0.0, 0.0, AStealthPawn::SampleCapsuleHalfHeightCm + 2.0));
			Request.ControlKind = EStealthPawnControlKind::Ai;
			return UStealthPawnFactory::Get(World)->SpawnSharedPawn(Request).Pawn;
		}
	};

	TArray<FString> Inventory(const AActor& Actor)
	{
		TArray<FString> Rows;
		for (const UActorComponent* Component : Actor.GetComponents())
		{
			Rows.Add(FString::Printf(TEXT("%s/%s/%d"), *Component->GetClass()->GetName(), *Component->GetName(), Component->GetIsReplicated() ? 1 : 0));
		}
		Rows.Sort();
		return Rows;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdControllerShapeTest, "StealthGame.Crowd.Controller.Shape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdControllerShapeTest::RunTest(const FString& Parameters)
{
	using namespace CrowdControllerTestData;
	const AStealthCrowdController* Cdo = GetDefault<AStealthCrowdController>();
	TestNull(TEXT("no PathFollowingComponent (DoNotCreateDefaultSubobject)"), Cdo->GetPathFollowingComponent());
	TestNull(TEXT("no PathFollowingComponent among the components"), Cdo->FindComponentByClass<UPathFollowingComponent>());
	TestFalse(TEXT("bWantsPlayerState is false"), Cdo->bWantsPlayerState);
	TestNotNull(TEXT("the crowd StateTree component exists"), Cdo->GetCrowdStateTree());
	TestEqual(TEXT("the controller ticks in TG_PrePhysics"), static_cast<int32>(Cdo->PrimaryActorTick.TickGroup), static_cast<int32>(TG_PrePhysics));
	TestEqual(TEXT("the StateTree component ticks in TG_PrePhysics"), static_cast<int32>(Cdo->GetCrowdStateTree()->PrimaryComponentTick.TickGroup), static_cast<int32>(TG_PrePhysics));

	FTestWorld TestWorld;
	AStealthPawn* Npc = TestWorld.Spawn(FVector::ZeroVector);
	AStealthCrowdController* Controller = Npc != nullptr ? Cast<AStealthCrowdController>(Npc->GetController()) : nullptr;
	if (!TestNotNull(TEXT("the factory possessed the NPC with the crowd controller"), Controller))
	{
		return false;
	}
	UCrowdDirector* Director = UCrowdDirector::Get(TestWorld.World);
	TestNotNull(TEXT("a Director exists on the authority"), Director);
	TestEqual(TEXT("OnPossess registered the driver with the Director"), Director->NumControllers(), 1);
	TestEqual(TEXT("an NPC gets the full driver"), static_cast<int32>(Controller->GetDriverKind()), static_cast<int32>(ECrowdDriverKind::Full));
	TestFalse(TEXT("bStartAILogicOnPossess left the tree stopped"), Controller->GetCrowdStateTree()->IsRunning());

	// DriveBoundPawn before the possession: the pending driver is Restricted (the call decides, never a binding).
	TestWorld.Hooks.bCreateController = false;
	AStealthPawn* Bound = TestWorld.Spawn(FVector(400.0, 0.0, 0.0));
	Director->DriveBoundPawn(Bound);
	AStealthCrowdController* Restricted = TestWorld.World->SpawnActor<AStealthCrowdController>();
	Restricted->Possess(Bound); // the test plays the factory's hand-over pass
	TestEqual(TEXT("DriveBoundPawn selects the restricted driver"), static_cast<int32>(Restricted->GetDriverKind()), static_cast<int32>(ECrowdDriverKind::Restricted));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdControllerLookPitchTest, "StealthGame.Crowd.Controller.LookPitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdControllerLookPitchTest::RunTest(const FString& Parameters)
{
	using namespace CrowdControllerTestData;
	FTestWorld TestWorld;
	AStealthPawn* Npc = TestWorld.Spawn(FVector::ZeroVector);
	AStealthCrowdController* Controller = Npc != nullptr ? Cast<AStealthCrowdController>(Npc->GetController()) : nullptr;
	if (!TestNotNull(TEXT("crowd controller"), Controller))
	{
		return false;
	}
	// The Look path: sink -> command component -> control rotation; UpdateControlRotation keeps yaw and pitch.
	const FRotator WantedLook(-15.0, 30.0, 0.0);
	TestTrue(TEXT("Look passes the sink"), Controller->Look(WantedLook));
	Controller->UpdateControlRotation(1.0f / 60.0f, true);
	TestTrue(TEXT("the control rotation pitch survives UpdateControlRotation (-15)"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch), -15.0, 0.01));
	TestTrue(TEXT("and its yaw (30)"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(Controller->GetControlRotation().Yaw), 30.0, 0.01));
	TestNull(TEXT("no focus actor"), Controller->GetFocusActor());

	// Negative control (E1): the stock AAIController zeroes the pitch of a pawn-less focus.
	TestWorld.Hooks.bCreateController = false;
	AStealthPawn* Other = TestWorld.Spawn(FVector(400.0, 0.0, 0.0));
	AAIController* Stock = TestWorld.World->SpawnActor<AAIController>();
	Stock->Possess(Other);
	Stock->SetControlRotation(WantedLook);
	Stock->UpdateControlRotation(1.0f / 60.0f, false);
	TestTrue(TEXT("negative control: the stock controller zeroes the pitch"), FMath::IsNearlyZero(FRotator::NormalizeAxis(Stock->GetControlRotation().Pitch), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdControllerTickOrderTest, "StealthGame.Crowd.Controller.TickOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdControllerTickOrderTest::RunTest(const FString& Parameters)
{
	using namespace CrowdControllerTestData;
	FTestWorld TestWorld;
	AStealthPawn* Npc = TestWorld.Spawn(FVector::ZeroVector);
	AStealthCrowdController* Controller = Npc != nullptr ? Cast<AStealthCrowdController>(Npc->GetController()) : nullptr;
	if (!TestNotNull(TEXT("crowd controller"), Controller))
	{
		return false;
	}
	{
		const UCrowdDirector* Director = UCrowdDirector::Get(TestWorld.World);
		const UCrowdStateTreeComponent* Tree = Controller->GetCrowdStateTree();
		bool bControllerAfterTree = false;
		for (const FTickPrerequisite& Prerequisite : Controller->PrimaryActorTick.GetPrerequisites())
		{
			bControllerAfterTree |= Tree != nullptr && Prerequisite.Get() == &Tree->PrimaryComponentTick;
		}
		AddInfo(FString::Printf(TEXT("TICKORDER director %d tree %d tree-after-director %d controller-after-tree %d groups %d/%d"), Director != nullptr, Tree != nullptr,
			Director != nullptr && Tree != nullptr && Director->IsTickPrerequisiteOf(Tree->PrimaryComponentTick), bControllerAfterTree,
			static_cast<int32>(Controller->PrimaryActorTick.TickGroup), Tree != nullptr ? static_cast<int32>(Tree->PrimaryComponentTick.TickGroup) : -1));
	}
	TestTrue(TEXT("Director -> StateTree component -> controller prerequisites hold"), Controller->VerifyTickOrder());
	// The controller precedes the pawn's movement component (AController::AddPawnTickDependency).
	bool bCmcAfterController = false;
	for (const FTickPrerequisite& Prerequisite : Npc->GetCharacterMovement()->PrimaryComponentTick.GetPrerequisites())
	{
		bCmcAfterController |= Prerequisite.Get() == &Controller->PrimaryActorTick;
	}
	TestTrue(TEXT("the pawn's CMC ticks after the controller"), bCmcAfterController);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdControllerComponentParityTest, "StealthGame.Crowd.Controller.ComponentParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdControllerComponentParityTest::RunTest(const FString& Parameters)
{
	using namespace CrowdControllerTestData;
	FTestWorld TestWorld;
	TestWorld.Hooks.bCreateController = false;
	AStealthPawn* Pawn = TestWorld.Spawn(FVector::ZeroVector);
	if (!TestNotNull(TEXT("pawn"), Pawn))
	{
		return false;
	}
	const TArray<FString> Before = Inventory(*Pawn);
	AStealthCrowdController* Controller = TestWorld.World->SpawnActor<AStealthCrowdController>();
	Controller->Possess(Pawn);
	const TArray<FString> After = Inventory(*Pawn);
	TestEqual(TEXT("AI possession adds no component (inventory size)"), After.Num(), Before.Num());
	TestTrue(TEXT("AI possession adds no component (inventory)"), Before == After);
	if (Before != After)
	{
		AddInfo(FString::Join(Before, TEXT(" ")) + TEXT(" | ") + FString::Join(After, TEXT(" ")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
