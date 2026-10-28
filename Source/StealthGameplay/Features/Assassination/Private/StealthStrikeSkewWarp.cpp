// The strike's guarded skew warp (2026-10-28 flake fix). See View/StealthStrikeSkewWarp.h.

#include "View/StealthStrikeSkewWarp.h"

#include <atomic>

#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "StealthGameplayModule.h"

namespace StealthStrikeSkewWarpLocal
{
	std::atomic<int32> TotalGuardedSteps{ 0 };
}

bool FStrikeWarpGuard::IsDegenerate(const FStrikeWarpStep& Step)
{
	// The engine's own conditions for reaching WarpTranslation's skew (RootMotionModifier_SkewWarp.cpp, UE 5.8.4).
	if (!Step.bWarpTranslation || Step.WindowTranslation.IsNearlyZero(RootMotionThreshold) || Step.DeltaTranslation.IsNearlyZero(RootMotionThreshold))
	{
		return false;
	}
	// WarpTranslation's ToRootNormalized = RemainingTranslation.GetSafeNormal(): zero below this squared length.
	return Step.RemainingTranslation.SizeSquared() <= SafeNormalSquaredTolerance;
}

UStealthStrikeSkewWarp::UStealthStrikeSkewWarp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UStealthStrikeSkewWarp::GetTotalGuardedSteps()
{
	return StealthStrikeSkewWarpLocal::TotalGuardedSteps.load();
}

FTransform UStealthStrikeSkewWarp::ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds)
{
	const UAnimSequenceBase* Anim = Animation.Get();
	if (Anim != nullptr && bWarpTranslation)
	{
		FStrikeWarpStep Step;
		Step.bWarpTranslation = bWarpTranslation;
		Step.WindowTranslation = TotalRootMotionWithinWindow.GetTranslation();
		Step.RemainingTranslation = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Anim, PreviousPosition, EndTime).GetTranslation();
		Step.DeltaTranslation = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Anim, PreviousPosition, FMath::Min(CurrentPosition, EndTime)).GetTranslation();
		if (FStrikeWarpGuard::IsDegenerate(Step))
		{
			++GuardedSteps;
			++StealthStrikeSkewWarpLocal::TotalGuardedSteps;
			if (GuardedSteps == 1)
			{
				const AActor* Owner = GetActorOwner();
				const ACharacter* Character = Cast<ACharacter>(Owner);
				UE_LOGF(LogStealthGameplay, Log, "[StrikeWarp] %ls: degenerate skew step passed through unwarped (window [%.4f %.4f], previous %.4f, current %.4f, rate %.3f, weight %.3f, step %ls, remaining %ls, role %d, net mode %d, replaying %d)",
					*GetNameSafe(Owner), StartTime, EndTime, PreviousPosition, CurrentPosition, PlayRate, Weight, *Step.DeltaTranslation.ToString(), *Step.RemainingTranslation.ToString(),
					Owner != nullptr ? static_cast<int32>(Owner->GetLocalRole()) : -1, Owner != nullptr && Owner->GetWorld() != nullptr ? static_cast<int32>(Owner->GetWorld()->GetNetMode()) : -1,
					Character != nullptr && Character->bClientUpdating ? 1 : 0);
			}
			return InRootMotion;
		}
	}
	return Super::ProcessRootMotion(InRootMotion, DeltaSeconds);
}

UStealthStrikeSkewWarp* UStealthStrikeSkewWarp::AddStrikeSkewWarp(UMotionWarpingComponent* Warping, const UAnimSequenceBase* Animation, float StartTime, float EndTime,
	FName WarpTargetName, const FTransform& WarpPointAnimTransform, bool bIgnoreZAxis)
{
	if (Warping == nullptr)
	{
		return nullptr;
	}
	UStealthStrikeSkewWarp* Modifier = NewObject<UStealthStrikeSkewWarp>(Warping);
	Modifier->Animation = Animation;
	Modifier->StartTime = StartTime;
	Modifier->EndTime = EndTime;
	Modifier->WarpTargetName = WarpTargetName;
	Modifier->WarpPointAnimProvider = EWarpPointAnimProvider::Static;
	Modifier->WarpPointAnimTransform = WarpPointAnimTransform;
	Modifier->WarpPointAnimBoneName = NAME_None;
	Modifier->bWarpTranslation = true;
	Modifier->bIgnoreZAxis = bIgnoreZAxis;
	Modifier->bWarpRotation = false;
	Warping->AddModifier(Modifier);
	return Modifier;
}
