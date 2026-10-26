#pragma once

#include "CoreMinimal.h"
#include "View/Core/StealthGait.h"
#include "View/StealthTuningPublicSubstructs.h"

/**
 * Shared Pawn body and traversal-height model (GDD shared-character-locomotion R17.1, F5; ADR-0010 section 2
 * "Body"; story SCL-002). Plain C++, client-safe (the classifier runs on both sides; story SCL-047 reuses it).
 *
 * Usage:
 *   const FStealthTraversalHeights Heights;                       // the TDC canonical defaults
 *   const EStealthCopingClass Class = StealthLocomotion::ClassifyCopingHeight(80.f, Heights);   // Vault
 */

/** The SCL traversal heights (TDC keys TraversalMinHeightCm, TraversalVaultHeightCm, TraversalClimbHeightCm,
 * TraversalHeightToleranceCm; Client audience, latch Round). The initialisers are the GDD canonical defaults and are
 * provisional pending the traversal spike: SCL-003 registers the keys, and the movement component then reads the
 * frozen snapshot. A changed default fails StealthGame.Locomotion.BodyConstantsPublished loudly. */
struct FStealthTraversalHeights
{
	/** At or below: stepped (also the CMC MaxStepHeight: one key, two uses). */
	float MinHeightCm = 45.0f;
	/** Vault capability height (the kit's vault coping). */
	float VaultHeightCm = 80.0f;
	/** Climb / mantle capability height. */
	float ClimbHeightCm = 160.0f;
	/** Hurdle capability height (1.2 m, final per spike SCL-045). */
	float HurdleHeightCm = 120.0f;
	/** Accepted deviation from an authored height. */
	float ToleranceCm = 10.0f;

	/** The heights of a tuning snapshot (the SCL Traversal* keys). */
	static FStealthTraversalHeights FromTuning(const FLocomotionTuning_Public& Tuning)
	{
		FStealthTraversalHeights Out;
		Out.MinHeightCm = Tuning.TraversalMinHeightCm;
		Out.VaultHeightCm = Tuning.TraversalVaultHeightCm;
		Out.ClimbHeightCm = Tuning.TraversalClimbHeightCm;
		Out.HurdleHeightCm = Tuning.TraversalHurdleHeightCm;
		Out.ToleranceCm = Tuning.TraversalHeightToleranceCm;
		return Out;
	}
};

/** What an obstacle of a given top height is to a pawn (F5 height band only; reach, facing and landing are SCL-047). */
enum class EStealthCopingClass : uint8
{
	Step,
	Vault,
	Climb,
	/** The 1.2 m hurdle band (spike SCL-045): a vault-type traversal over a taller wall. */
	Hurdle,
	Block,
};

/** The body constants every pawn shares, published to the Level Socket Contract and the Generator (R17.1). */
struct FStealthBodyConstants
{
	float CapsuleRadiusCm = 0.0f;
	float CapsuleHalfHeightCm = 0.0f;
	float MaxStepHeightCm = 0.0f;
	float WalkableFloorAngleDeg = 0.0f;
};

namespace StealthLocomotion
{
	/**
	 * F5 height bands: <= MinHeight steps; within Tolerance of the vault or climb height is a vault or climb; anything
	 * else (between the step height and the vault band, or between the bands) blocks.
	 */
	STEALTHGAMEPLAY_API EStealthCopingClass ClassifyCopingHeight(float HeightCm, const FStealthTraversalHeights& Heights);

	/** The engine default walkable floor angle the pawn keeps (44.765 degrees; the 40 degree temple stair passes). */
	inline constexpr float EngineDefaultWalkableFloorAngleDeg = 44.765f;
}

// ---- Motion model (GDD F1-F4, R5, R6; ADR-0010 section 6; stories SCL-009 to SCL-017) -------------------------------
//
// FStealthLocomotionModel is the one movement shaping function of every pawn: plain C++, no UWorld, client-safe (the
// owner predicts with it, the server runs it for every pawn, AI and human alike). UStealthMovementComponent calls it
// from CalcVelocity / PhysicsRotation; tests run it headless at a fixed step. Nothing here knows the controller kind.
//
// Usage:
//   FLocomotionTuning_Public Tuning;                 // the frozen snapshot (defaults here)
//   FStealthMotionState State;                       // per pawn, carried in the saved move
//   FStealthMoveInput Input;  Input.Magnitude = 1.f; Input.InputYawDeg = 90.f; Input.Gait = EGait::Walk;
//   FStealthLocomotionModel::Step(Tuning, Input, 1.f / 60.f, State);   // State.Speed, State.FacingYawDeg advance

/** The legality class SCL reads (R6; IIF GetLegalityClass, Unbound maps to Active so it reveals no binding). */
enum class EStealthGaitLegality : uint8
{
	Active,
	Informant,
	RoundEnded,
};

/** SCL view of the IIF action pose (FActionState.Pose, R10). Written only by the IIF on the server. */
enum class EStealthActionPose : uint8
{
	None,
	Brace,
	InteractPose,
	Strike,
	Counter,
	BushEnter,
	BushExit,
};

/** One step of commanded input, the same for every source (owner prediction, server ServerMove, server AI). */
struct FStealthMoveInput
{
	/** |Direction| as received (0 = no input; a stick reading below the dead zone was already sent as 0). */
	float Magnitude = 0.0f;
	/** World yaw of the input direction, degrees (ignored when Magnitude is 0). */
	float InputYawDeg = 0.0f;
	EGait Gait = EGait::Walk;
	EStealthGaitLegality Legality = EStealthGaitLegality::Active;
	EStealthActionPose Pose = EStealthActionPose::None;
	/** The pawn itself has commanded motion disabled (dead, Disabled movement): treated as no input. */
	bool bDisabled = false;
	/** True when the unclamped head-look target is known (idle turn-to-look, R5.5). */
	bool bHasLookTarget = false;
	/** World yaw of the unclamped head-look target, degrees. */
	float LookTargetYawDeg = 0.0f;
};

/** Per-pawn model state: everything a replayed move must restore (FSavedMove_Stealth, story SCL-011). */
struct FStealthMotionState
{
	/** Body facing (actor yaw), degrees. */
	float FacingYawDeg = 0.0f;
	/** Horizontal speed along the facing, cm/s. */
	float Speed = 0.0f;
	/** Quantised walk pace of the last walk input (0 before the first; R5.1 hysteresis state). */
	float Pace = 0.0f;
	/** A stop to rest is in progress with the deceleration latched at its start (F2, D2). */
	bool bStopping = false;
	float StopDecel = 0.0f;
	/** A reversal triggered and holds until Pivot exits (R5.4). */
	bool bReversalLatched = false;
	/** Turning in place (reversal after the stop, idle start in a new direction). */
	bool bPivoting = false;
	/**
	 * A pivot just exited into a start: until the pawn is above PivotSpeedCmPerSec again, an idle pivot needs the error to
	 * exceed PivotExitAngleDeg + ReversalHysteresisDeg, so an input wobbling around the exit angle cannot flip the
	 * restart back into a pivot (R5.4 latch hysteresis).
	 */
	bool bPivotExited = false;
	/** Idle turn-to-look in progress (R5.5). */
	bool bTurningToLook = false;
	/** Time the unclamped look target has been beyond LookYawLimitDeg while idle. */
	float LookBeyondSec = 0.0f;
	/** Sign of the last non-zero turn (an exact 180 degree tie turns this way; clockwise first). */
	float LastTurnSign = 1.0f;
	/** The effective gait of the last commanded input (picks the stop deceleration). */
	EGait LastGait = EGait::Walk;

	bool operator==(const FStealthMotionState& Other) const = default;
};

namespace StealthLocomotion
{
	/** Pace quantiser hysteresis around each boundary of the pace set (R5.1: +-0.03 of magnitude). */
	inline constexpr float PaceHysteresis = 0.03f;
	/** The owning client stick dead zone (R5.1); the server applies none. */
	inline constexpr float StickDeadZone = 0.2f;
	/** At or below this horizontal speed a pawn counts as at rest for the idle rules (floor-snap noise stays below it). */
	inline constexpr float RestSpeedCmPerSec = 0.5f;
	/** Tie threshold for an exact reversal (degrees from 180). */
	inline constexpr float ReversalTieDeg = 0.01f;

	/**
	 * F1 q(m): Magnitude quantised to the fixed pace set {0.8, 0.9, 1.0} with +-0.03 hysteresis around the boundaries
	 * 0.85 and 0.95 (PreviousPace is the last result, 0 for none). m in (0, 0.8] maps to 0.8; m <= 0 returns 0.
	 */
	STEALTHGAMEPLAY_API float QuantisePace(float Magnitude, float PreviousPace);

	/** The owning client dead zone: a stick reading below 0.2 is sent as zero, anything else unchanged (length <= 1). */
	STEALTHGAMEPLAY_API FVector2D ApplyStickDeadZone(const FVector2D& Stick);

	/** R5.2 effective gait: Run becomes Walk for an Informant or while Braced. Same function for every pawn. */
	STEALTHGAMEPLAY_API EGait EffectiveGait(EGait Requested, EStealthGaitLegality Legality, EStealthActionPose Pose);

	/** Planted brace: Move input ignored for translation and yaw (R5.2). */
	inline bool IsPlanted(EStealthActionPose Pose) { return Pose == EStealthActionPose::Brace; }
	/** Strike, Counter and the bush transitions: Move input ignored, the pose root motion moves the pawn. */
	inline bool IsPoseLocked(EStealthActionPose Pose)
	{
		return Pose == EStealthActionPose::Strike || Pose == EStealthActionPose::Counter || Pose == EStealthActionPose::BushEnter || Pose == EStealthActionPose::BushExit;
	}

	/** F1 v_max for one input (0 when planted, pose-locked, disabled, round ended or m = 0). Pace from QuantisePace. */
	STEALTHGAMEPLAY_API float TargetSpeed(const FLocomotionTuning_Public& Tuning, const FStealthMoveInput& Input, float Pace);

	/** F2 accelerations and decelerations, cm/s^2. */
	inline float WalkAcceleration(const FLocomotionTuning_Public& T) { return T.WalkSpeedCmPerSec / FMath::Max(T.WalkStartSec, UE_SMALL_NUMBER); }
	inline float RunAcceleration(const FLocomotionTuning_Public& T) { return T.RunSpeedCmPerSec / FMath::Max(T.RunStartSec, UE_SMALL_NUMBER); }
	/** One walk deceleration for every walk pace, from the slowest pace: WalkSpeed x MinMult / WalkStopSec (187 at defaults). */
	inline float WalkDeceleration(const FLocomotionTuning_Public& T) { return T.WalkSpeedCmPerSec * T.WalkSpeedScaleMinMult / FMath::Max(T.WalkStopSec, UE_SMALL_NUMBER); }
	inline float RunDeceleration(const FLocomotionTuning_Public& T) { return T.RunSpeedCmPerSec / FMath::Max(T.RunStopSec, UE_SMALL_NUMBER); }
	/** T_stop,max = max(WalkStopSec / WalkSpeedScaleMinMult, RunStopSec): no stop to rest lasts longer (I-SCL-12/15/16). */
	inline float LongestStopSec(const FLocomotionTuning_Public& T) { return FMath::Max(T.WalkStopSec / FMath::Max(T.WalkSpeedScaleMinMult, UE_SMALL_NUMBER), T.RunStopSec); }

	/**
	 * F2 stop floor: the deceleration latched for a stop to rest from V0 = min(d_g, V0 / StopMinSec), where d_g is the
	 * run deceleration after a Run or from above walk speed, else the walk deceleration.
	 */
	STEALTHGAMEPLAY_API float StopDeceleration(const FLocomotionTuning_Public& Tuning, float V0, EGait LastGait);

	/** F4 IsRunning with hysteresis: true above Threshold, false below Threshold - Hysteresis, else unchanged; false when forced. */
	STEALTHGAMEPLAY_API bool StepIsRunning(bool bWasRunning, float HorizontalSpeed, float ThresholdCmPerSec, float HysteresisCmPerSec, bool bForcedFalse);

	/** Signed shortest angle From -> To in (-180, 180], degrees. */
	STEALTHGAMEPLAY_API float DeltaYawDeg(float FromDeg, float ToDeg);
}

/** The model step (F1-F3, R5.1-R5.5). */
struct STEALTHGAMEPLAY_API FStealthLocomotionModel
{
	/**
	 * Advances State by Dt for Input: pace quantisation, effective gait, turn shaping (rate cap, latched reversal with
	 * stop-pivot-start, idle pivot, idle turn-to-look) and the speed ramp (constant acceleration, latched stop
	 * deceleration with the stop floor, run-to-walk slowdown at the run rate). Pure: the same inputs give the same state.
	 */
	static void Step(const FLocomotionTuning_Public& Tuning, const FStealthMoveInput& Input, float Dt, FStealthMotionState& State);

private:
	static float StepHeading(const FLocomotionTuning_Public& Tuning, const FStealthMoveInput& Input, EGait Gait, float Target, float Dt, FStealthMotionState& State);
	static void StepSpeed(const FLocomotionTuning_Public& Tuning, EGait Gait, float Target, float Dt, FStealthMotionState& State);
	static void RotateToward(float TargetYawDeg, float MaxStepDeg, FStealthMotionState& State);
};

// ---- Traversal eligibility and kit heights (GDD F5, R7; ADR-0010 section 7; stories SCL-046, SCL-047) ----------------
//
// Usage:
//   FStealthTraversalQuery Query;  Query.BodyFacingYawDeg = Pawn->GetActorRotation().Yaw;  (never the camera)
//   FStealthCopingCandidate Coping; Coping.Id = 7; Coping.HeightCm = 83.f; Coping.DistanceCm = 70.f; Coping.InwardNormalYawDeg = 20.f;
//   const FStealthTraversalDecision D = StealthLocomotion::EvaluateTraversal(Query, MakeArrayView(&Coping, 1), Tuning);
//   // D.bEligible, D.Type == EStealthCopingClass::Vault, D.CopingId == 7

/** One coping the pawn query found (a Level Socket Contract coping record, measured from the pawn). */
struct FStealthCopingCandidate
{
	/** The coping record id (public geometry; the tie-breaker is the lowest id). */
	int32 Id = INDEX_NONE;
	/** Coping top above the pawn floor, cm. */
	float HeightCm = 0.0f;
	/** Horizontal distance capsule edge to coping, cm. */
	float DistanceCm = 0.0f;
	/** World yaw of the coping inward normal (the direction a traversal crosses it), degrees. */
	float InwardNormalYawDeg = 0.0f;
	/** Free landing depth beyond the coping (vault, hurdle) or on top of it (climb), cm. */
	float LandingClearCm = 0.0f;
	/** Pawns overlapping the landing cell at validation. */
	int32 LandingOccupants = 0;
};

/** The pawn side of F5: legality, the action slot and the body. */
struct FStealthTraversalQuery
{
	EStealthGaitLegality Legality = EStealthGaitLegality::Active;
	/** The IIF action slot allows Traverse (not Bracing, Striking, HideTransition). */
	bool bSlotAllows = true;
	bool bGrounded = true;
	bool bInTraversal = false;
	/** Body facing (actor yaw), degrees. The camera never enters F5. */
	float BodyFacingYawDeg = 0.0f;
};

/** Why F5 refused (the IIF replies NotNow for every SCL-side refusal). */
enum class EStealthTraversalRefusal : uint8
{
	None,
	NotActive,
	SlotBusy,
	NotGrounded,
	AlreadyTraversing,
	NoEligibleCoping,
	LandingOccupied,
};

/** The F5 result: the chosen coping and traversal type, or the refusal. */
struct FStealthTraversalDecision
{
	bool bEligible = false;
	EStealthCopingClass Type = EStealthCopingClass::Block;
	int32 CopingId = INDEX_NONE;
	EStealthTraversalRefusal Refusal = EStealthTraversalRefusal::None;
};

/** One kit coping height that matches no capability height (SCL-046 LSC cross-check). */
struct FStealthCopingMismatch
{
	int32 CopingId = INDEX_NONE;
	float HeightCm = 0.0f;
};

namespace StealthLocomotion
{
	/**
	 * The Level Socket Contract landing clearance (LSC key TraversalLandingClearCm, default 100). LSC owns the key; until the
	 * LSC epic registers it as a schema row F5 reads this value equal to its registered default (tech debt TD-SCL-LAND).
	 */
	inline constexpr float PendingTraversalLandingClearCm = 100.0f;

	/** True for the traversable coping classes (vault, hurdle, climb). */
	inline bool IsTraversable(EStealthCopingClass Class)
	{
		return Class == EStealthCopingClass::Vault || Class == EStealthCopingClass::Hurdle || Class == EStealthCopingClass::Climb;
	}

	/**
	 * F5: A and S and grounded and not already traversing, and a coping whose height is in a capability band, within reach
	 * (DistanceCm <= TraversalReachCm), within the body-facing cone (|facing - inward normal| <= TraversalFacingHalfAngleDeg)
	 * and with a clear landing (LandingClearCm >= LandingClear). Selection: every coping filtered by all predicates, then the
	 * smallest distance, then the smallest facing angle, then the lowest id. A passing coping whose landing cell holds a pawn
	 * is a refusal (LandingOccupied), never a fallback to another coping.
	 */
	STEALTHGAMEPLAY_API FStealthTraversalDecision EvaluateTraversal(const FStealthTraversalQuery& Query, TConstArrayView<FStealthCopingCandidate> Copings,
		const FLocomotionTuning_Public& Tuning, float LandingClearCm = PendingTraversalLandingClearCm);

	/**
	 * SCL-046 LSC cross-check at OnLayoutReady: every kit coping height must be within TraversalHeightToleranceCm of a
	 * capability height (vault, hurdle, climb). Returns false and lists each offending coping id and height otherwise.
	 */
	STEALTHGAMEPLAY_API bool ValidateKitCopingHeights(TConstArrayView<FStealthCopingCandidate> Copings, const FStealthTraversalHeights& Heights, TArray<FStealthCopingMismatch>& OutMismatches);
}
