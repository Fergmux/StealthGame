#pragma once

// The Shared Pawn saved move (ADR-0010 section 6; story SCL-011). Lives outside Core/SharedPawn because the engine's
// CombineWith signature names the player controller type, which the class-parity scan forbids in the pawn files.

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "View/Core/StealthLocomotionModel.h"

/**
 * The saved move: the requested gait (FLAG_Custom_0), the action pose, and the model latch state the move started with
 * (pace, stop latch, reversal / pivot latches, turn-to-look timer), restored on replay and on combination so the
 * replayed and combined moves start from the latches of their first frame. Facing and speed are not saved: replay
 * starts them from the corrected actor rotation and velocity.
 */
class STEALTHGAMEPLAY_API FSavedMove_Stealth : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
	virtual void PrepMoveFor(ACharacter* C) override;

	/** True when the latch fields of A and B agree (facing and speed are re-read from the actor, so they are ignored). */
	static bool SameLatches(const FStealthMotionState& A, const FStealthMotionState& B);

	/** Restores the latches of Saved on Movement, keeping its current facing and speed. */
	static void RestoreLatches(class UStealthMovementComponent& Movement, const FStealthMotionState& Saved);

	bool bSavedWantsToRun = false;
	EStealthActionPose SavedPose = EStealthActionPose::None;
	/** The model state at the start of the move. */
	FStealthMotionState StartMotionState;
};

/** Allocates FSavedMove_Stealth. */
class STEALTHGAMEPLAY_API FNetworkPredictionData_Client_Stealth : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_Stealth(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {}

	virtual FSavedMovePtr AllocateNewMove() override;
};
