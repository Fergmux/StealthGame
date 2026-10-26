#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "View/Core/StealthLocomotionModel.h"
#include "StealthPawn.generated.h"

class UGameplayTasksComponent;
class UStealthMotionWarpingComponent;
class UStealthMovementComponent;

/**
 * The Shared Pawn (ADR-0010 section 2; SCL R1 One class, R3 Possession parity). Every body in the
 * city, NPC or player, is one AStealthPawn on one movement component class with the same default
 * subobjects. Nothing here reads the controller kind, the PlayerState or Roster state (ADR-0001);
 * the class parity scan (StealthGame.Locomotion.ClassParity) enforces that on this file.
 *
 * Header placement: Public/View/Core/ (client-safe), per the ADR-0010 Key Interfaces amendment
 * (architecture review 2026-10-25, Conflict 4). Pawns are created only by UStealthPawnFactory
 * (story SCL-006); nothing may add a component to a pawn at run time.
 */
UCLASS()
class STEALTHGAMEPLAY_API AStealthPawn : public ACharacter
{
	GENERATED_BODY()

public:
	/** Sets UStealthMovementComponent as the movement subobject and builds the fixed subobject set. */
	AStealthPawn(const FObjectInitializer& ObjectInitializer);

	/**
	 * The body constants of every pawn, read from the class default object (R17.1): capsule radius and half-height
	 * (the reparented sample character's values, recorded in these class defaults), MaxStepHeight and the walkable
	 * floor angle. The Level Socket Contract and the Generator consume this (CR-4).
	 */
	static FStealthBodyConstants GetPublishedBodyConstants();

	/** Capsule radius recorded from the Game Animation Sample character (R17.1, measured in the 5.8.4 project). */
	static constexpr float SampleCapsuleRadiusCm = 30.0f;
	/** Capsule half-height recorded from the Game Animation Sample character (R17.1, measured in the 5.8.4 project). */
	static constexpr float SampleCapsuleHalfHeightCm = 86.0f;

	/** Name of the non-replicated gameplay tasks subobject (also found by AAIController on possession). */
	static const FName GameplayTasksComponentName;

	/** Name of the motion-warping subobject. */
	static const FName MotionWarpingComponentName;

	/**
	 * Recalculates the base eye height only (ADR-0010 E2). Deliberately skips ACharacter::Restart and
	 * APawn::Restart, which stop movement, consume pending input and reset the movement mode: a
	 * possession change must not be visible in the body's motion.
	 */
	virtual void Restart() override;

	/**
	 * Resets client prediction data, then runs APawn::PawnClientRestart directly, skipping
	 * ACharacter's StopMovementImmediately so the owner's velocity is kept across a possession.
	 */
	virtual void PawnClientRestart() override;

	/** No-op: the factory possesses every pawn; no engine path may spawn a controller for one. */
	virtual void SpawnDefaultController() override;

	/** The Shared Pawn's movement component (never null on a constructed pawn). */
	UStealthMovementComponent* GetStealthMovement() const;

	/** The non-replicated gameplay tasks component every pawn carries. */
	UGameplayTasksComponent* GetGameplayTasks() const { return GameplayTasks; }

	/**
	 * The motion-warping component (WarpTargets never replicate). Null on the D53 prototype adapter pawns, which use the
	 * sample Blueprint's own component until ADR-0010's Blueprint cleanup: callers must null-check.
	 */
	UStealthMotionWarpingComponent* GetMotionWarping() const { return SharedMotionWarping; }

private:
	/** Default subobject: AAIController adopts it on possession instead of adding one (ADR-0010). */
	UPROPERTY(VisibleAnywhere, Category = "Stealth|Pawn")
	TObjectPtr<UGameplayTasksComponent> GameplayTasks;

	/**
	 * Default subobject "MotionWarping": motion warping with WarpTargets replication disabled (ADR-0018 section 7.2).
	 * The C++ member is not named MotionWarping, and the subobject is optional, so the reparented sample Blueprint
	 * (which still carries its own SCS component and variable of that name until ADR-0010's Blueprint cleanup) compiles
	 * against this class: the D53 prototype adapter AStealthCharacter opts out of it (DoNotCreateDefaultSubobject).
	 */
	UPROPERTY(VisibleAnywhere, Category = "Stealth|Pawn")
	TObjectPtr<UStealthMotionWarpingComponent> SharedMotionWarping;
};
