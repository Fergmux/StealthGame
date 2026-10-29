#include "Core/Movement/StealthSavedMove.h"

#include "GameFramework/Character.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace StealthSavedMoveLocal
{
	UStealthMovementComponent* MovementOf(ACharacter* C)
	{
		return C != nullptr ? Cast<UStealthMovementComponent>(C->GetCharacterMovement()) : nullptr;
	}

}

void FSavedMove_Stealth::RestoreLatches(UStealthMovementComponent& Movement, const FStealthMotionState& Saved)
{
	const float Facing = Movement.MotionState.FacingYawDeg;
	const float Speed = Movement.MotionState.Speed;
	Movement.MotionState = Saved;
	Movement.MotionState.FacingYawDeg = Facing;
	Movement.MotionState.Speed = Speed;
}

void FSavedMove_Stealth::Clear()
{
	Super::Clear();
	bSavedWantsToRun = false;
	SavedPose = EStealthActionPose::None;
	StartMotionState = FStealthMotionState();
	SavedGateState = 0;
}

uint8 FSavedMove_Stealth::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToRun)
	{
		Flags |= FLAG_Custom_0;
	}
	return Flags;
}

bool FSavedMove_Stealth::SameLatches(const FStealthMotionState& A, const FStealthMotionState& B)
{
	FStealthMotionState AA = A;
	FStealthMotionState BB = B;
	AA.FacingYawDeg = BB.FacingYawDeg = 0.0f;
	AA.Speed = BB.Speed = 0.0f;
	return AA == BB;
}

bool FSavedMove_Stealth::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Stealth* Other = static_cast<const FSavedMove_Stealth*>(NewMove.Get());
	if (Other == nullptr || Other->bSavedWantsToRun != bSavedWantsToRun || Other->SavedPose != SavedPose || Other->SavedGateState != SavedGateState)
	{
		return false;
	}
	// Never merge across any latch difference: the combined move must replay from one start state.
	if (!SameLatches(StartMotionState, Other->StartMotionState))
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Stealth::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UStealthMovementComponent* Movement = StealthSavedMoveLocal::MovementOf(C))
	{
		bSavedWantsToRun = Movement->bWantsToRun;
		SavedPose = Movement->ActionPose;
		StartMotionState = Movement->MotionState;
		SavedGateState = Movement->MotionGateState;
	}
}

void FSavedMove_Stealth::CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation)
{
	Super::CombineWith(OldMove, InCharacter, PC, OldStartLocation);
	// The engine reverts position, rotation and velocity to the pending move's start; the model latches go back too.
	const FSavedMove_Stealth* Old = static_cast<const FSavedMove_Stealth*>(OldMove);
	StartMotionState = Old->StartMotionState;
	if (UStealthMovementComponent* Movement = StealthSavedMoveLocal::MovementOf(InCharacter))
	{
		RestoreLatches(*Movement, Old->StartMotionState);
	}
}

void FSavedMove_Stealth::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UStealthMovementComponent* Movement = StealthSavedMoveLocal::MovementOf(C))
	{
		RestoreLatches(*Movement, StartMotionState);
		Movement->ActionPose = SavedPose;
		Movement->MotionGateState = SavedGateState;
	}
}

FSavedMovePtr FNetworkPredictionData_Client_Stealth::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Stealth());
}
