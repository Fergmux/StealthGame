#include "View/Core/StealthPawn.h"

#include "Components/CapsuleComponent.h"
#include "GameplayTasksComponent.h"
#include "Server/Core/StealthPawnCommandComponent.h"
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

	// ADR-0011 section 1: one non-replicated command component per pawn, never added per kind.
	PawnCommand = CreateDefaultSubobject<UPawnCommandComponent>(UPawnCommandComponent::ComponentName);

	SharedMotionWarping = CreateOptionalDefaultSubobject<UStealthMotionWarpingComponent>(MotionWarpingComponentName);

	// R17.1 Body: one capsule size for every pawn, the sample character's values recorded here.
	GetCapsuleComponent()->InitCapsuleSize(SampleCapsuleRadiusCm, SampleCapsuleHalfHeightCm);
}

FStealthBodyConstants AStealthPawn::GetPublishedBodyConstants()
{
	const AStealthPawn* Defaults = GetDefault<AStealthPawn>();
	FStealthBodyConstants Out;
	Out.CapsuleRadiusCm = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	Out.CapsuleHalfHeightCm = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const UCharacterMovementComponent* Movement = Defaults->GetCharacterMovement();
	Out.MaxStepHeightCm = Movement->MaxStepHeight;
	Out.WalkableFloorAngleDeg = Movement->GetWalkableFloorAngle();
	return Out;
}

void AStealthPawn::Restart()
{
	// ADR-0010 E2: APawn::Restart would also StopMovementImmediately and consume the pending input
	// vector, and ACharacter::Restart would reset the movement mode. Keep only the eye height.
	RecalculateBaseEyeHeight();
	// A body that has never had a movement mode (a freshly spawned, first-possessed pawn) gets the default one, the
	// part of ACharacter::Restart a first possession needs; a body already moving keeps its mode, so a possession
	// change stays invisible in its motion (D53 integration fix, story iif-094: production input drove a MOVE_None body).
	// Only once per body: a body later disabled (MOVE_None on death) is never silently re-enabled by a re-possession.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement(); Movement && Movement->MovementMode == MOVE_None && !bMovementModeInitialised)
	{
		Movement->SetDefaultMovementMode();
	}
	bMovementModeInitialised = true;
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

UPawnCommandComponent* AStealthPawn::GetPawnCommand() const
{
	return PawnCommand;
}
