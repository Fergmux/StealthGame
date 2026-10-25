#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "StealthAnimInstance.generated.h"

/**
 * C++ parent of the one Shared Pawn AnimBP (ADR-0013 section 1; SCL R10, story SCL-005).
 *
 * Input rule (exhaustive, ADR-0013 section 1): the AnimBP and this class read only replicated
 * movement (velocity, CMC acceleration, movement mode), HeadLook, RemoteViewPitch16,
 * TraversalState, FActionState, FDeathState, the step-clock phase, per-pawn idle timers seeded
 * identically for every kind, the tier flags and EstimatedServerNow(). Never the controller,
 * IsPlayerControlled, IsLocallyControlled, the PlayerState, team or Roster state: every pawn
 * animates from the same inputs, so the listen host shows NPCs and players identically.
 * StealthGame.Locomotion.ClassParity scans this file and AnimBpNoLocalControlRead the AnimBP.
 *
 * The replicated-state inputs are added here by the stories that publish them (HeadLook SCL-021,
 * traversal SCL-046..049, action and death states in the Assassination epic); this story only
 * establishes the parent class with no local-control read.
 */
UCLASS(Abstract)
class STEALTHGAMEPLAY_API UStealthAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
};
