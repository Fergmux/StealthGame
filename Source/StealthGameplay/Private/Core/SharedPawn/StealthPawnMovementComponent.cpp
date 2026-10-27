#include "View/Core/StealthPawnMovementComponent.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Server/StealthLogFaultSink.h"
#include "Server/StealthTuningSubstructs.h"
#include "Core/Movement/StealthSavedMove.h"

namespace StealthMovementLocal
{
	const FName FaultSource(TEXT("SharedPawnMovement"));

	/** The process fault sink used when none is injected. */
	FStealthLogFaultSink& DefaultFaultSink()
	{
		static FStealthLogFaultSink Sink;
		return Sink;
	}

	/** Displacement history sampling interval (R16 bucket granularity). */
	constexpr double DisplacementSampleSec = 0.05;
}

UStealthMovementComponent::UStealthMovementComponent()
{
	// Kind-blind defaults (ADR-0010 section 2). RVO would steer bodies by a local avoidance pass the owner prediction
	// cannot reproduce; the same value applies to every pawn. The body faces its direction of travel (R5.4); the model
	// owns the facing in the walking modes (PhysicsRotation), the engine orient-to-movement covers the other modes.
	bOrientRotationToMovement = true;
	bUseRVOAvoidance = false;
	// R5.7: the engine path follower never drives a pawn (RequestDirectMove/RequestPathMove are fault backstops).
	NavMovementProperties.bUseAccelerationForPaths = false;
	// The MVP has no jump (R5.1).
	NavAgentProps.bCanJump = false;

	// The ServerOnly IsRunning keys default to the schema defaults; the server applies the frozen snapshot.
	const FLocomotionTuning_Server ServerDefaults;
	RunThresholdCmPerSec = ServerDefaults.RunSpeedThresholdCmPerSec;
	RunExitHysteresisCmPerSec = ServerDefaults.RunExitHysteresisCmPerSec;

	// R17.1 / ADR-0010 Body: MaxStepHeight is TraversalMinHeightCm (one key, two uses); the walkable floor angle
	// stays at the engine default (44.765 degrees).
	ApplyLocomotionTuning(FLocomotionTuning_Public());
}

void UStealthMovementComponent::ApplyTraversalHeights(const FStealthTraversalHeights& Heights)
{
	MaxStepHeight = Heights.MinHeightCm;
}

void UStealthMovementComponent::ApplyLocomotionTuning(const FLocomotionTuning_Public& InTuning)
{
	Tuning = InTuning;
	ApplyTraversalHeights(FStealthTraversalHeights::FromTuning(Tuning));
	// The engine fields other code may read mirror the model (the model, not these fields, shapes motion).
	MaxWalkSpeed = Tuning.WalkSpeedCmPerSec;
	MaxAcceleration = FMath::Max(StealthLocomotion::WalkAcceleration(Tuning), StealthLocomotion::RunAcceleration(Tuning));
	BrakingDecelerationWalking = StealthLocomotion::WalkDeceleration(Tuning);
}

void UStealthMovementComponent::SetRunHysteresis(float ThresholdCmPerSec, float HysteresisCmPerSec)
{
	RunThresholdCmPerSec = ThresholdCmPerSec;
	RunExitHysteresisCmPerSec = HysteresisCmPerSec;
}

EGait UStealthMovementComponent::GetEffectiveGait() const
{
	return StealthLocomotion::EffectiveGait(GetRequestedGait(), GetGaitLegality(), ActionPose);
}

bool UStealthMovementComponent::IsCommandedMotionFrozen() const
{
	if (FrozenFromServerTime < 0.0)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	const double Now = GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0);
	return Now + 0.001 >= FrozenFromServerTime; // ADR-0002 DueEpsilonSec (1 ms)
}

EStealthGaitLegality UStealthMovementComponent::GetGaitLegality() const
{
	const APawn* Pawn = PawnOwner.Get();
	return LegalitySource != nullptr && Pawn != nullptr ? LegalitySource->GetGaitLegality(*Pawn) : EStealthGaitLegality::Active;
}

bool UStealthMovementComponent::IsModelActive() const
{
	return IsMovingOnGround() && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity();
}

float UStealthMovementComponent::GetMaxSpeed() const
{
	if (IsModelActive())
	{
		const FStealthMoveInput Input = BuildStepInput();
		const float Target = StealthLocomotion::TargetSpeed(Tuning, Input, MotionState.Pace);
		// Idle: report the gait ceiling (consumers read it as "how fast could this pawn go"), never 0 for a standing pawn.
		if (Target > 0.0f || Input.Magnitude <= 0.0f)
		{
			return Target > 0.0f ? Target : (GetEffectiveGait() == EGait::Run ? Tuning.RunSpeedCmPerSec : Tuning.WalkSpeedCmPerSec);
		}
		return 0.0f;
	}
	return Super::GetMaxSpeed();
}

float UStealthMovementComponent::GetMaxAcceleration() const
{
	// One constant (the larger gait acceleration), so the analog input modifier |a| / a_max equals the input magnitude
	// on every machine whatever the gait; the model applies the gait acceleration itself.
	return FMath::Max(StealthLocomotion::WalkAcceleration(Tuning), StealthLocomotion::RunAcceleration(Tuning));
}

float UStealthMovementComponent::GetMaxBrakingDeceleration() const
{
	if (IsModelActive())
	{
		return MotionState.bStopping ? MotionState.StopDecel : StealthLocomotion::WalkDeceleration(Tuning);
	}
	return Super::GetMaxBrakingDeceleration();
}

bool UStealthMovementComponent::CanAttemptJump() const
{
	return false;
}

void UStealthMovementComponent::SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode)
{
	// R6 backstop: informants and braced pawns never enter the traversal mode, whoever asks.
	if (NewMovementMode == TraversalMovementMode && MovementMode != TraversalMovementMode
		&& (GetGaitLegality() == EStealthGaitLegality::Informant || StealthLocomotion::IsPlanted(ActionPose)))
	{
		ReportMisuse(EStealthMovementMisuse::RefusedTraversalMode);
		OnTraversalModeRefused();
		return;
	}
	Super::SetMovementMode(NewMovementMode, NewCustomMode);
}

void UStealthMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	// R5.7 / ADR-0010 section 6: AI motion enters only through Move; a direct velocity request moves nothing.
	ReportMisuse(EStealthMovementMisuse::DirectMove);
}

void UStealthMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	ReportMisuse(EStealthMovementMisuse::PathMove);
}

void UStealthMovementComponent::ReportMisuse(EStealthMovementMisuse Misuse)
{
	++BackstopHits;
	IStealthFaultSink& Sink = FaultSink != nullptr ? *FaultSink : static_cast<IStealthFaultSink&>(StealthMovementLocal::DefaultFaultSink());
	Sink.Report(EStealthFault::MovementMisuse, StealthMovementLocal::FaultSource, static_cast<int64>(Misuse));
}

void UStealthMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	// The requested gait rides ServerMove (and a replayed move) as FLAG_Custom_0.
	bWantsToRun = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FStealthMoveInput UStealthMovementComponent::BuildStepInput() const
{
	FStealthMoveInput Input;
	const FVector Accel2D(Acceleration.X, Acceleration.Y, 0.0);
	const float MaxAccel = GetMaxAcceleration();
	Input.Magnitude = MaxAccel > 0.0f ? FMath::Min(static_cast<float>(Accel2D.Size()) / MaxAccel, 1.0f) : 0.0f;
	if (Input.Magnitude <= UE_KINDA_SMALL_NUMBER)
	{
		Input.Magnitude = 0.0f;
	}
	else
	{
		Input.InputYawDeg = static_cast<float>(Accel2D.Rotation().Yaw);
	}
	Input.Gait = GetRequestedGait();
	Input.Legality = GetGaitLegality();
	Input.Pose = ActionPose;
	Input.bDisabled = IsCommandedMotionDisabled() || MovementMode == MOVE_None;
	if (const APawn* Pawn = PawnOwner.Get())
	{
		// R5.5: the unclamped look target is the control rotation (the F6 filter input) for every pawn; a pawn with no
		// one driving it (the instant of a hand-over) has no look target, so it never turns toward a default rotation.
		Input.bHasLookTarget = Pawn->IsPawnControlled();
		Input.LookTargetYawDeg = static_cast<float>(Pawn->GetControlRotation().Yaw);
	}
	return Input;
}

void UStealthMovementComponent::PerformMovement(float DeltaTime)
{
	bModelSteppedThisMove = false;
	if (UpdatedComponent != nullptr)
	{
		MotionState.FacingYawDeg = static_cast<float>(UpdatedComponent->GetComponentRotation().Yaw);
	}
	Super::PerformMovement(DeltaTime);
}

void UStealthMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (!IsModelActive() || !HasValidData() || DeltaTime < MIN_TICK_TIME)
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}
	// The pawn actual horizontal speed starts the step (a blocked pawn brakes from what it really does, R5.6).
	MotionState.Speed = static_cast<float>(Velocity.Size2D());
	FStealthLocomotionModel::Step(Tuning, BuildStepInput(), DeltaTime, MotionState);
	const FVector Facing = FRotator(0.0, MotionState.FacingYawDeg, 0.0).Vector();
	Velocity = FVector(Facing.X * MotionState.Speed, Facing.Y * MotionState.Speed, 0.0);
	bModelSteppedThisMove = true;
}

void UStealthMovementComponent::PhysicsRotation(float DeltaTime)
{
	// Only where this machine simulated the move (never a simulated proxy, which takes the replicated rotation). While the
	// model owns the body (walking, no root motion) a step it skipped (a sub-minimum frame) keeps the facing: the engine
	// orient-to-movement is a different turn law.
	const bool bSimulatedHere = UpdatedComponent != nullptr && CharacterOwner != nullptr && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy;
	if (bSimulatedHere && IsModelActive() && !bModelSteppedThisMove)
	{
		return;
	}
	if (bSimulatedHere && bModelSteppedThisMove && IsModelActive())
	{
		const FRotator Current = UpdatedComponent->GetComponentRotation();
		const FRotator Desired(0.0, MotionState.FacingYawDeg, 0.0);
		if (!Current.Equals(Desired, 1e-3))
		{
			MoveUpdatedComponent(FVector::ZeroVector, Desired, /*bSweep*/ false);
		}
		return;
	}
	Super::PhysicsRotation(DeltaTime);
}

void UStealthMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	// F4 IsRunning from commanded horizontal speed; forced false in traversal, a pose lock, falling or disabled (R19).
	const bool bForcedFalse = IsInTraversal() || IsFalling() || MovementMode == MOVE_None || HasAnimRootMotion() || StealthLocomotion::IsPoseLocked(ActionPose);
	bIsRunning = StealthLocomotion::StepIsRunning(bIsRunning, static_cast<float>(Velocity.Size2D()), RunThresholdCmPerSec, RunExitHysteresisCmPerSec, bForcedFalse);

	// R16: the step displacement goes to the commanded bucket under the pawn own non-zero Move input, or while the pawn
	// was still moving under its own model (the baseline stop after a release is the commanded motion ending); a pawn at
	// rest with zero input that moves was moved by something else (a push, depenetration, a platform).
	// R16 is server state: replayed moves on an owning client and corrections must not count (authority only).
	if (UpdatedComponent != nullptr && CharacterOwner != nullptr && CharacterOwner->HasAuthority())
	{
		// Anything that moved the capsule between two moves (a depenetration push, a teleport) is external.
		if (bHasLastMovedLocation)
		{
			ExternalTotalCm += static_cast<float>(FVector::Dist2D(LastMovedLocation, OldLocation));
		}
		LastMovedLocation = UpdatedComponent->GetComponentLocation();
		bHasLastMovedLocation = true;
		const FVector Delta = UpdatedComponent->GetComponentLocation() - OldLocation;
		const float Moved = static_cast<float>(FVector(Delta.X, Delta.Y, 0.0).Size());
		if (!Acceleration.IsNearlyZero() || (bModelSteppedThisMove && OldVelocity.Size2D() > UE_KINDA_SMALL_NUMBER))
		{
			CommandedTotalCm += Moved;
		}
		else
		{
			ExternalTotalCm += Moved;
		}
	}
	const UWorld* World = GetWorld();
	const double Now = World != nullptr ? World->GetTimeSeconds() : 0.0;
	const int32 Last = (DisplacementHead + DisplacementHistorySize - 1) % DisplacementHistorySize;
	if (DisplacementCount == 0 || Now - DisplacementHistory[Last].Time >= StealthMovementLocal::DisplacementSampleSec)
	{
		DisplacementHistory[DisplacementHead] = { Now, CommandedTotalCm };
		DisplacementHead = (DisplacementHead + 1) % DisplacementHistorySize;
		DisplacementCount = FMath::Min(DisplacementCount + 1, DisplacementHistorySize);
	}
}

float UStealthMovementComponent::GetCommandedDisplacementCm(double SinceServerTime) const
{
	// The cumulative total recorded at the newest sample at or before SinceServerTime (samples are ~50 ms apart, so the
	// window may start up to one sample early). The ring keeps about 12.8 s; an older SinceServerTime is clamped to the
	// oldest kept sample (and to 0 before the first wrap), never extended to the pawn's whole life.
	float Base = 0.0f;
	if (DisplacementCount == DisplacementHistorySize)
	{
		Base = DisplacementHistory[DisplacementHead].CumulativeCm;
	}
	for (int32 Offset = 0; Offset < DisplacementCount; ++Offset)
	{
		const int32 Index = (DisplacementHead + DisplacementHistorySize - DisplacementCount + Offset) % DisplacementHistorySize;
		if (DisplacementHistory[Index].Time > SinceServerTime)
		{
			break;
		}
		Base = DisplacementHistory[Index].CumulativeCm;
	}
	return FMath::Max(0.0f, CommandedTotalCm - Base);
}

FNetworkPredictionData_Client* UStealthMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UStealthMovementComponent* MutableThis = const_cast<UStealthMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Stealth(*this);
	}
	return ClientPredictionData;
}

bool UStealthMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	// The replay applies every saved move's own gait (UpdateFromCompressedFlags) and pose (PrepMoveFor); the live intent
	// is what the pawn command interface last asked for, so it is restored after the replay.
	const bool bLiveWantsToRun = bWantsToRun;
	const EStealthActionPose LivePose = ActionPose;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToRun = bLiveWantsToRun;
	ActionPose = LivePose;
	return bResult;
}
