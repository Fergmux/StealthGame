#include "View/Core/StealthLocomotionModel.h"

#include "View/Core/WalkSlowerScale.h"

EStealthCopingClass StealthLocomotion::ClassifyCopingHeight(float HeightCm, const FStealthTraversalHeights& Heights)
{
	if (HeightCm <= Heights.MinHeightCm)
	{
		return EStealthCopingClass::Step;
	}
	if (FMath::Abs(HeightCm - Heights.VaultHeightCm) <= Heights.ToleranceCm)
	{
		return EStealthCopingClass::Vault;
	}
	if (FMath::Abs(HeightCm - Heights.ClimbHeightCm) <= Heights.ToleranceCm)
	{
		return EStealthCopingClass::Climb;
	}
	if (FMath::Abs(HeightCm - Heights.HurdleHeightCm) <= Heights.ToleranceCm)
	{
		return EStealthCopingClass::Hurdle;
	}
	return EStealthCopingClass::Block;
}

// ---- Motion model (stories SCL-009 to SCL-017) ---------------------------------------------------------------------

float StealthLocomotion::QuantisePace(float Magnitude, float PreviousPace)
{
	if (Magnitude <= 0.0f)
	{
		return 0.0f;
	}
	static constexpr float Levels[] = { StealthPaceSet::Slow, StealthPaceSet::Middle, StealthPaceSet::Full };
	static constexpr float Boundaries[] = { 0.85f, 0.95f };
	int32 Index = INDEX_NONE;
	for (int32 Level = 0; Level < UE_ARRAY_COUNT(Levels); ++Level)
	{
		if (FMath::IsNearlyEqual(PreviousPace, Levels[Level], 1e-4f))
		{
			Index = Level;
		}
	}
	if (Index == INDEX_NONE)
	{
		// No previous pace: the plain nearest band.
		return Magnitude <= Boundaries[0] ? Levels[0] : (Magnitude <= Boundaries[1] ? Levels[1] : Levels[2]);
	}
	while (Index < 2 && Magnitude > Boundaries[Index] + PaceHysteresis)
	{
		++Index;
	}
	while (Index > 0 && Magnitude < Boundaries[Index - 1] - PaceHysteresis)
	{
		--Index;
	}
	return Levels[Index];
}

FVector2D StealthLocomotion::ApplyStickDeadZone(const FVector2D& Stick)
{
	const double Length = Stick.Size();
	if (Length < StickDeadZone)
	{
		return FVector2D::ZeroVector;
	}
	return Length > 1.0 ? Stick / Length : Stick;
}

EGait StealthLocomotion::EffectiveGait(EGait Requested, EStealthGaitLegality Legality, EStealthActionPose Pose)
{
	if (Requested == EGait::Run && (Legality == EStealthGaitLegality::Informant || IsPlanted(Pose)))
	{
		return EGait::Walk;
	}
	return Requested;
}

float StealthLocomotion::TargetSpeed(const FLocomotionTuning_Public& Tuning, const FStealthMoveInput& Input, float Pace)
{
	if (Input.bDisabled || IsPlanted(Input.Pose) || IsPoseLocked(Input.Pose) || Input.Legality == EStealthGaitLegality::RoundEnded || Input.Magnitude <= 0.0f)
	{
		return 0.0f;
	}
	if (EffectiveGait(Input.Gait, Input.Legality, Input.Pose) == EGait::Run)
	{
		return Tuning.RunSpeedCmPerSec;
	}
	const float Quantised = Pace > 0.0f ? Pace : QuantisePace(Input.Magnitude, 0.0f);
	return Tuning.WalkSpeedCmPerSec * FMath::Max(Quantised, Tuning.WalkSpeedScaleMinMult);
}

float StealthLocomotion::StopDeceleration(const FLocomotionTuning_Public& Tuning, float V0, EGait LastGait)
{
	const bool bFromRun = LastGait == EGait::Run || V0 > Tuning.WalkSpeedCmPerSec + 0.5f;
	const float Base = bFromRun ? RunDeceleration(Tuning) : WalkDeceleration(Tuning);
	return FMath::Min(Base, V0 / FMath::Max(Tuning.StopMinSec, UE_SMALL_NUMBER));
}

bool StealthLocomotion::StepIsRunning(bool bWasRunning, float HorizontalSpeed, float ThresholdCmPerSec, float HysteresisCmPerSec, bool bForcedFalse)
{
	if (bForcedFalse)
	{
		return false;
	}
	if (HorizontalSpeed > ThresholdCmPerSec)
	{
		return true;
	}
	if (HorizontalSpeed < ThresholdCmPerSec - HysteresisCmPerSec)
	{
		return false;
	}
	return bWasRunning;
}

float StealthLocomotion::DeltaYawDeg(float FromDeg, float ToDeg)
{
	const float Delta = FMath::FindDeltaAngleDegrees(FromDeg, ToDeg);
	return Delta <= -180.0f ? 180.0f : Delta;
}

void FStealthLocomotionModel::Step(const FLocomotionTuning_Public& Tuning, const FStealthMoveInput& Input, float Dt, FStealthMotionState& State)
{
	using namespace StealthLocomotion;
	const bool bFrozen = IsPlanted(Input.Pose) || IsPoseLocked(Input.Pose);
	const bool bHasInput = !bFrozen && !Input.bDisabled && Input.Legality != EStealthGaitLegality::RoundEnded && Input.Magnitude > 0.0f;
	const EGait Gait = EffectiveGait(Input.Gait, Input.Legality, Input.Pose);
	if (bHasInput && Gait == EGait::Walk)
	{
		State.Pace = QuantisePace(Input.Magnitude, State.Pace);
	}
	float Target = bHasInput ? TargetSpeed(Tuning, Input, State.Pace) : 0.0f;
	if (bFrozen)
	{
		// Planted brace / pose lock: no yaw from Move, no pivot, no turn-to-look; the baseline stop brings the body to
		// rest (from a walk this is the step-into-stance distance, R5.2).
		State.bReversalLatched = false;
		State.bPivoting = false;
		State.bTurningToLook = false;
		State.LookBeyondSec = 0.0f;
	}
	else
	{
		Target = StepHeading(Tuning, Input, Gait, Target, Dt, State);
	}
	StepSpeed(Tuning, Gait, Target, Dt, State);
	if (bHasInput)
	{
		State.LastGait = Gait;
	}
}

float FStealthLocomotionModel::StepHeading(const FLocomotionTuning_Public& Tuning, const FStealthMoveInput& Input, EGait Gait, float Target, float Dt, FStealthMotionState& State)
{
	using namespace StealthLocomotion;
	if (Target <= 0.0f)
	{
		State.bReversalLatched = false;
		State.bPivoting = false;
		State.bPivotExited = false;
		// R5.5 idle turn-to-look: only a body at rest, from the unclamped look target.
		const bool bIdle = State.Speed <= StealthLocomotion::RestSpeedCmPerSec && Input.bHasLookTarget;
		if (!bIdle)
		{
			State.bTurningToLook = false;
			State.LookBeyondSec = 0.0f;
			return 0.0f;
		}
		const float HeadYaw = FMath::Abs(DeltaYawDeg(State.FacingYawDeg, Input.LookTargetYawDeg));
		if (!State.bTurningToLook)
		{
			if (HeadYaw > Tuning.LookYawLimitDeg)
			{
				State.LookBeyondSec += Dt;
				State.bTurningToLook = State.LookBeyondSec >= Tuning.TurnInPlaceDelaySec - 1e-4f;
			}
			else
			{
				State.LookBeyondSec = 0.0f;
			}
		}
		if (State.bTurningToLook)
		{
			RotateToward(Input.LookTargetYawDeg, Tuning.PivotTurnRateDegPerSec * Dt, State);
			if (FMath::Abs(DeltaYawDeg(State.FacingYawDeg, Input.LookTargetYawDeg)) <= Tuning.TurnInPlaceExitFrac * Tuning.LookYawLimitDeg + 1e-3f)
			{
				State.bTurningToLook = false;
				State.LookBeyondSec = 0.0f;
			}
		}
		return 0.0f;
	}

	State.bTurningToLook = false;
	State.LookBeyondSec = 0.0f;
	if (State.Speed > Tuning.PivotSpeedCmPerSec)
	{
		State.bPivotExited = false;
	}
	const float Error = FMath::Abs(DeltaYawDeg(State.FacingYawDeg, Input.InputYawDeg));
	if (!State.bReversalLatched && !State.bPivoting)
	{
		const float IdlePivotAngle = Tuning.PivotExitAngleDeg + (State.bPivotExited ? Tuning.ReversalHysteresisDeg : 0.0f);
		if (Error > Tuning.ReversalAngleDeg && State.Speed > Tuning.PivotSpeedCmPerSec)
		{
			State.bReversalLatched = true;
		}
		else if (State.Speed <= Tuning.PivotSpeedCmPerSec && Error > IdlePivotAngle)
		{
			State.bPivoting = true;
		}
	}
	if (State.bReversalLatched && !State.bPivoting && State.Speed <= Tuning.PivotSpeedCmPerSec)
	{
		State.bPivoting = true;
	}
	if (State.bPivoting)
	{
		RotateToward(Input.InputYawDeg, Tuning.PivotTurnRateDegPerSec * Dt, State);
		if (FMath::Abs(DeltaYawDeg(State.FacingYawDeg, Input.InputYawDeg)) > Tuning.PivotExitAngleDeg)
		{
			return 0.0f;
		}
		// Pivot exits to a start: the latch is released here and only here.
		State.bPivoting = false;
		State.bReversalLatched = false;
		State.bPivotExited = true;
		return Target;
	}
	if (State.bReversalLatched)
	{
		// The baseline stop before the pivot: facing holds, speed brakes to the pivot speed.
		return 0.0f;
	}
	const float Rate = Gait == EGait::Run ? Tuning.RunTurnRateDegPerSec : Tuning.WalkTurnRateDegPerSec;
	RotateToward(Input.InputYawDeg, Rate * Dt, State);
	return Target;
}

void FStealthLocomotionModel::StepSpeed(const FLocomotionTuning_Public& Tuning, EGait Gait, float Target, float Dt, FStealthMotionState& State)
{
	using namespace StealthLocomotion;
	if (Target > State.Speed)
	{
		const float Acceleration = Gait == EGait::Run ? RunAcceleration(Tuning) : WalkAcceleration(Tuning);
		State.Speed = FMath::Min(Target, State.Speed + Acceleration * Dt);
		State.bStopping = false;
	}
	else if (Target < State.Speed)
	{
		if (Target <= 0.0f)
		{
			if (!State.bStopping)
			{
				State.StopDecel = StopDeceleration(Tuning, State.Speed, State.LastGait);
				State.bStopping = true;
			}
			State.Speed = FMath::Max(0.0f, State.Speed - State.StopDecel * Dt);
			if (State.Speed <= 0.0f)
			{
				State.bStopping = false;
			}
		}
		else
		{
			// A slowdown that does not reach rest is not a stop: run -> walk at the run rate, a pace drop at the walk rate.
			State.bStopping = false;
			const float Deceleration = State.Speed > Tuning.WalkSpeedCmPerSec + 0.5f ? RunDeceleration(Tuning) : WalkDeceleration(Tuning);
			State.Speed = FMath::Max(Target, State.Speed - Deceleration * Dt);
		}
	}
	else if (Target <= 0.0f)
	{
		State.bStopping = false;
	}
}

void FStealthLocomotionModel::RotateToward(float TargetYawDeg, float MaxStepDeg, FStealthMotionState& State)
{
	const float Delta = StealthLocomotion::DeltaYawDeg(State.FacingYawDeg, TargetYawDeg);
	const float Magnitude = FMath::Abs(Delta);
	if (Magnitude <= 0.0f || MaxStepDeg <= 0.0f)
	{
		return;
	}
	// R5.4: an exact 180 degree tie turns in the sign of the last non-zero turn (clockwise first).
	const float Sign = Magnitude >= 180.0f - StealthLocomotion::ReversalTieDeg ? State.LastTurnSign : FMath::Sign(Delta);
	const float StepDeg = FMath::Min(Magnitude, MaxStepDeg);
	State.FacingYawDeg = FRotator::NormalizeAxis(State.FacingYawDeg + Sign * StepDeg);
	State.LastTurnSign = Sign;
}

// ---- Traversal eligibility and kit heights (stories SCL-046, SCL-047) -----------------------------------------------

FStealthTraversalDecision StealthLocomotion::EvaluateTraversal(const FStealthTraversalQuery& Query, TConstArrayView<FStealthCopingCandidate> Copings,
	const FLocomotionTuning_Public& Tuning, float LandingClearCm)
{
	FStealthTraversalDecision Decision;
	if (Query.Legality != EStealthGaitLegality::Active)
	{
		Decision.Refusal = EStealthTraversalRefusal::NotActive;
		return Decision;
	}
	if (!Query.bSlotAllows)
	{
		Decision.Refusal = EStealthTraversalRefusal::SlotBusy;
		return Decision;
	}
	if (!Query.bGrounded)
	{
		Decision.Refusal = EStealthTraversalRefusal::NotGrounded;
		return Decision;
	}
	if (Query.bInTraversal)
	{
		Decision.Refusal = EStealthTraversalRefusal::AlreadyTraversing;
		return Decision;
	}
	const FStealthTraversalHeights Heights = FStealthTraversalHeights::FromTuning(Tuning);
	const FStealthCopingCandidate* Best = nullptr;
	float BestAngle = 0.0f;
	EStealthCopingClass BestClass = EStealthCopingClass::Block;
	for (const FStealthCopingCandidate& Coping : Copings)
	{
		const EStealthCopingClass Class = ClassifyCopingHeight(Coping.HeightCm, Heights);
		const float Angle = FMath::Abs(DeltaYawDeg(Query.BodyFacingYawDeg, Coping.InwardNormalYawDeg));
		if (!IsTraversable(Class) || Coping.DistanceCm > Tuning.TraversalReachCm || Angle > Tuning.TraversalFacingHalfAngleDeg || Coping.LandingClearCm < LandingClearCm)
		{
			continue;
		}
		const bool bBetter = Best == nullptr
			|| Coping.DistanceCm < Best->DistanceCm
			|| (Coping.DistanceCm == Best->DistanceCm && (Angle < BestAngle || (Angle == BestAngle && Coping.Id < Best->Id)));
		if (bBetter)
		{
			Best = &Coping;
			BestAngle = Angle;
			BestClass = Class;
		}
	}
	if (Best == nullptr)
	{
		Decision.Refusal = EStealthTraversalRefusal::NoEligibleCoping;
		return Decision;
	}
	if (Best->LandingOccupants > 0)
	{
		Decision.Refusal = EStealthTraversalRefusal::LandingOccupied;
		Decision.CopingId = Best->Id;
		return Decision;
	}
	Decision.bEligible = true;
	Decision.Type = BestClass;
	Decision.CopingId = Best->Id;
	return Decision;
}

bool StealthLocomotion::ValidateKitCopingHeights(TConstArrayView<FStealthCopingCandidate> Copings, const FStealthTraversalHeights& Heights, TArray<FStealthCopingMismatch>& OutMismatches)
{
	OutMismatches.Reset();
	for (const FStealthCopingCandidate& Coping : Copings)
	{
		if (!IsTraversable(ClassifyCopingHeight(Coping.HeightCm, Heights)))
		{
			OutMismatches.Add({ Coping.Id, Coping.HeightCm });
		}
	}
	return OutMismatches.IsEmpty();
}
