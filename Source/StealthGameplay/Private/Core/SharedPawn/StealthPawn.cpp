#include "View/Core/StealthPawn.h"

#include "GameplayTasksComponent.h"
#include "View/Core/StealthMotionWarpingComponent.h"
#include "View/Core/StealthPawnMovementComponent.h"

const FName AStealthPawn::GameplayTasksComponentName(TEXT("GameplayTasks"));
const FName AStealthPawn::MotionWarpingComponentName(TEXT("MotionWarping"));

AStealthPawn::AStealthPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UStealthMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Facing follows movement for every pawn; the look path (HeadLook) never turns the body.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Only the factory creates and possesses pawns (ADR-0010 section 3): no engine auto-possession,
	// and no default AI controller class for anything to spawn.
	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AIControllerClass = nullptr;

	// Present on every pawn so AI possession finds one instead of adding a component (AAIController
	// otherwise creates it at OnPossess). Local bookkeeping only: never replicated.
	GameplayTasks = CreateDefaultSubobject<UGameplayTasksComponent>(GameplayTasksComponentName);
	GameplayTasks->SetIsReplicated(false);

	MotionWarping = CreateDefaultSubobject<UStealthMotionWarpingComponent>(MotionWarpingComponentName);
}

void AStealthPawn::Restart()
{
	// ADR-0010 E2: APawn::Restart would also StopMovementImmediately and consume the pending input
	// vector, and ACharacter::Restart would reset the movement mode. Keep only the eye height.
	RecalculateBaseEyeHeight();
}

void AStealthPawn::PawnClientRestart()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->ResetPredictionData_Client();
	}
	// Skip ACharacter::PawnClientRestart (it zeroes velocity); APawn's version calls Restart() above.
	APawn::PawnClientRestart();
}

void AStealthPawn::SpawnDefaultController()
{
	// Intentionally empty: UStealthPawnFactory possesses every pawn (ADR-0010).
}

UStealthMovementComponent* AStealthPawn::GetStealthMovement() const
{
	return CastChecked<UStealthMovementComponent>(GetCharacterMovement());
}
