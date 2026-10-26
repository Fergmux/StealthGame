#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "View/Core/StealthGait.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/StealthTuningPublicSubstructs.h"
#include "StealthPawnMovementComponent.generated.h"

class IStealthFaultSink;

/**
 * Where SCL reads a pawn legality class (R5.2, R6; ADR-0010 section 6 IGaitLegalitySource). Injected by the composition
 * root: on the server it maps the Roster legality class (Unbound -> Active), on the owning client the owner view of its
 * own state; other clients never ask. Tests inject a stub. One query for every pawn.
 */
class IStealthGaitLegalitySource
{
public:
	virtual ~IStealthGaitLegalitySource() = default;

	/** The legality class of Pawn. */
	virtual EStealthGaitLegality GetGaitLegality(const APawn& Pawn) const = 0;
};

/** Fault-sink Detail values of movement misuse (EStealthFault::MovementMisuse). */
enum class EStealthMovementMisuse : uint8
{
	/** RequestDirectMove reached a pawn: AI motion enters only through Move (R5.7, ADR-0010 section 6). */
	DirectMove = 1,
	/** RequestPathMove reached a pawn (the same backstop). */
	PathMove = 2,
	/** The traversal movement mode was requested for a pawn that may not traverse (informant or braced; R6 backstop). */
	RefusedTraversalMode = 3,
};

/**
 * The Shared Pawn movement component (ADR-0010): one CMC subclass for every pawn kind. Movement shaping is the plain-C++
 * FStealthLocomotionModel (F1-F3, R5): this component feeds it the step input (the input acceleration direction and
 * magnitude, the requested gait, the legality class and the action pose) from CalcVelocity in the walking modes, writes
 * the model velocity, and applies the model facing in PhysicsRotation. Owner prediction, the server for a remote
 * human and the server for an AI pawn run the same code; the requested gait travels in ServerMove as FLAG_Custom_0 and
 * the model latch state is restored on replay through FSavedMove_Stealth (story SCL-011).
 *
 * Published server state (R19): IsRunning (F4 hysteresis), IsInTraversal and the commanded-displacement bucket (R16).
 * Every rule reads the tuning snapshot only; no rule asks who drives the pawn.
 *
 * File name: StealthPawnMovementComponent.h, not StealthMovementComponent.h, because the prototype module still has a
 * header of that name (ADR-0007 rule (f), unique header names). Rename when the prototype class is deleted.
 */
UCLASS()
class STEALTHGAMEPLAY_API UStealthMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FSavedMove_Stealth;
	friend class FNetworkPredictionData_Client_Stealth;

public:
	/** Kind-blind defaults: no RVO avoidance, MaxStepHeight = TraversalMinHeightCm, the SCL default tuning snapshot. */
	UStealthMovementComponent();

	/** Applies the traversal heights of the frozen tuning snapshot (MaxStepHeight = MinHeightCm). Same for every pawn. */
	void ApplyTraversalHeights(const struct FStealthTraversalHeights& Heights);

	/** Applies a frozen Client-audience snapshot (every pawn of a round gets the same one). Also re-applies the heights. */
	void ApplyLocomotionTuning(const FLocomotionTuning_Public& Tuning);

	/** The snapshot the model reads. */
	const FLocomotionTuning_Public& GetLocomotionTuning() const { return Tuning; }

	/** Server: the ServerOnly IsRunning keys (RunSpeedThresholdCmPerSec, RunExitHysteresisCmPerSec). */
	void SetRunHysteresis(float ThresholdCmPerSec, float HysteresisCmPerSec);

	/** The gait the commanded Move asks for (set by UPawnCommandComponent::Move; on the server for a remote owner, from ServerMove). */
	void SetRequestedGait(EGait Gait) { bWantsToRun = Gait == EGait::Run; }
	EGait GetRequestedGait() const { return bWantsToRun ? EGait::Run : EGait::Walk; }

	/** The gait the model applies now (R5.2 clamps). */
	EGait GetEffectiveGait() const;

	/** Injects the legality source (composition root; tests). Null means Active. Not owned. */
	void SetGaitLegalitySource(const IStealthGaitLegalitySource* Source) { LegalitySource = Source; }

	/** The action pose SCL obeys (written by the IIF action state on the server; tests write it through a stub writer). */
	void SetActionPose(EStealthActionPose Pose) { ActionPose = Pose; }
	EStealthActionPose GetActionPose() const { return ActionPose; }

	/** Injects the fault sink the backstops report to (null: the process log sink). Not owned. */
	void SetFaultSink(IStealthFaultSink* Sink) { FaultSink = Sink; }

	/** The model state after the last step. */
	const FStealthMotionState& GetMotionState() const { return MotionState; }

	/** The quantised walk pace in effect (0 before the first walk input). */
	float GetEffectivePace() const { return MotionState.Pace; }

	/** R19 / F4: IsRunning with hysteresis, from commanded horizontal speed; false in traversal, pose locks, falling, disabled. */
	bool IsRunning() const { return bIsRunning; }

	/** R19: in a traversal (the traversal movement mode, MOVE_Flying per spike SCL-045). */
	bool IsInTraversal() const { return MovementMode == MOVE_Flying; }

	/** R16: commanded horizontal displacement (own non-zero Move input, and the stop that ends it) since SinceServerTime (world time), cm. */
	float GetCommandedDisplacementCm(double SinceServerTime) const;

	/** R16: horizontal displacement with zero Move input (pushes, depenetration, platforms) since the pawn began, cm. */
	float GetExternalDisplacementTotalCm() const { return ExternalTotalCm; }

	/** Backstop hits (direct/path move requests and refused traversal modes) since construction. */
	int32 GetBackstopHitCount() const { return BackstopHits; }

	/** The traversal movement mode SCL refuses for informants and braced pawns (MOVE_Flying, spike SCL-045). */
	static constexpr EMovementMode TraversalMovementMode = MOVE_Flying;

	// UCharacterMovementComponent
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual bool CanAttemptJump() const override;
	virtual void SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode = 0) override;
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;
	virtual void RequestPathMove(const FVector& MoveInput) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void PerformMovement(float DeltaTime) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void PhysicsRotation(float DeltaTime) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	/** Replay restores each saved move's gait and pose; the live intent is put back afterwards (engine pattern for crouch). */
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

	/** The legality class of the owning pawn (the injected source; Active without one). */
	virtual EStealthGaitLegality GetGaitLegality() const;

	/** True when the pawn itself has commanded motion disabled (a prototype adapter hook; production: false). */
	virtual bool IsCommandedMotionDisabled() const { return false; }

	/** Called after the traversal mode was refused (a prototype adapter cancels its montage here). */
	virtual void OnTraversalModeRefused() {}

	/** True while the model drives velocity and facing (walking modes, no root motion). */
	bool IsModelActive() const;

	/** Reports a backstop hit to the fault sink. */
	void ReportMisuse(EStealthMovementMisuse Misuse);

private:
	/** Builds the step input from the current acceleration, gait, legality, pose and look target. */
	FStealthMoveInput BuildStepInput() const;

	UPROPERTY(Transient)
	FLocomotionTuning_Public Tuning;

	float RunThresholdCmPerSec = 300.0f;
	float RunExitHysteresisCmPerSec = 25.0f;

	FStealthMotionState MotionState;
	bool bWantsToRun = false;
	EStealthActionPose ActionPose = EStealthActionPose::None;
	bool bIsRunning = false;
	/** The model stepped during the current PerformMovement (PhysicsRotation applies its facing). */
	bool bModelSteppedThisMove = false;

	const IStealthGaitLegalitySource* LegalitySource = nullptr;
	IStealthFaultSink* FaultSink = nullptr;
	int32 BackstopHits = 0;

	/** R16 commanded-displacement history: (world time, cumulative commanded cm), sampled every ~50 ms. */
	struct FDisplacementSample
	{
		double Time = 0.0;
		float CumulativeCm = 0.0f;
	};
	static constexpr int32 DisplacementHistorySize = 256;
	FDisplacementSample DisplacementHistory[DisplacementHistorySize];
	int32 DisplacementHead = 0;
	int32 DisplacementCount = 0;
	float CommandedTotalCm = 0.0f;
	float ExternalTotalCm = 0.0f;
	/** Where the last move left the capsule (displacement before the next move starts is external). */
	FVector LastMovedLocation = FVector::ZeroVector;
	bool bHasLastMovedLocation = false;
};
