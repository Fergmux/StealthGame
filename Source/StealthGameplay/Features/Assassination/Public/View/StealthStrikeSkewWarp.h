#pragma once

// The strike's local warp (story asn-018) with a guard on the engine skew warp's degenerate step (2026-10-28 flake fix).
//
// URootMotionModifier_SkewWarp::WarpTranslation builds its working space from the direction of the root motion still to
// come in the window (PreviousPosition -> EndTime). When that remaining root motion is (near) zero while this step's root
// motion is not, the direction is the zero vector, FRotationMatrix::MakeFromXZ returns an all-zero matrix and the three
// InverseTransformVector calls raise the engine ensure "TMatrix<T>::InverseFast(), trying to invert a NIL matrix" and feed
// NaNs into the step. A step whose remaining window is empty has nothing left to warp toward, so the guarded modifier
// passes that step's root motion through unwarped (exactly what the window's end does) and logs it once per modifier.

#include "CoreMinimal.h"
#include "RootMotionModifier_SkewWarp.h"

#include "StealthStrikeSkewWarp.generated.h"

class UMotionWarpingComponent;
class UAnimSequenceBase;

/** The inputs of one skew-warp step that decide whether the engine's WarpTranslation can build its working space. */
struct FStrikeWarpStep
{
	/** This step's root translation (PreviousPosition -> min(CurrentPosition, EndTime)), root-motion space. */
	FVector DeltaTranslation = FVector::ZeroVector;
	/** The root translation still to come in the window (PreviousPosition -> EndTime). */
	FVector RemainingTranslation = FVector::ZeroVector;
	/** The window's whole root translation (StartTime -> EndTime). */
	FVector WindowTranslation = FVector::ZeroVector;
	bool bWarpTranslation = true;
};

/** Pure decision of the guard (no engine state): true when the engine's skew step would build a NIL matrix. */
struct STEALTHGAMEPLAY_API FStrikeWarpGuard
{
	/** The engine's thresholds: RootMotionThreshold (2e-4, per component) and GetSafeNormal's squared tolerance (1e-8). */
	static constexpr double RootMotionThreshold = 2e-4;
	static constexpr double SafeNormalSquaredTolerance = 1e-8;

	/**
	 * True when URootMotionModifier_SkewWarp would call WarpTranslation (translation warped, the window and this step move
	 * the root) with a remaining translation too small to normalise: the degenerate step the guard passes through.
	 */
	static bool IsDegenerate(const FStrikeWarpStep& Step);
};

/**
 * URootMotionModifier_SkewWarp for the strike clips: identical except that a degenerate step (FStrikeWarpGuard) returns the
 * step's root motion unwarped instead of reaching the engine's NIL-matrix inverse.
 */
UCLASS()
class STEALTHGAMEPLAY_API UStealthStrikeSkewWarp : public URootMotionModifier_SkewWarp
{
	GENERATED_BODY()

public:
	UStealthStrikeSkewWarp(const FObjectInitializer& ObjectInitializer);

	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;

	/** AddRootMotionModifierSkewWarp's translation-only form (no rotation warp), creating this guarded class. */
	static UStealthStrikeSkewWarp* AddStrikeSkewWarp(UMotionWarpingComponent* Warping, const UAnimSequenceBase* Animation, float StartTime, float EndTime,
		FName WarpTargetName, const FTransform& WarpPointAnimTransform, bool bIgnoreZAxis);

	/** How many degenerate steps this modifier passed through (diagnostics and tests). */
	int32 GetGuardedSteps() const { return GuardedSteps; }

	/** Process-wide count of guarded steps (tests and the flake-fix evidence). */
	static int32 GetTotalGuardedSteps();

private:
	int32 GuardedSteps = 0;
};
