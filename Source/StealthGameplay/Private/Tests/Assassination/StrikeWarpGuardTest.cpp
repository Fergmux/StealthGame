// 2026-10-28 Assassination PIE flake fix (MotionWarping NIL-matrix ensure): StealthGame.Assassination.StrikeWarpGuard.
// The engine skew warp builds its working space from the root motion still to come in the window; a step whose remaining
// window has no root translation while the step itself moves the root (a montage position that moved back from the
// window's end) yields an all-zero matrix and the "InverseFast ... NIL matrix" ensure. Asserted:
//   - the pure guard (FStrikeWarpGuard::IsDegenerate) on the boundary cases: the degenerate step, an ordinary step, a
//     window or a step without root translation (the engine does not skew those), translation warp off;
//   - live, on a spawned Shared Pawn's warping component with the strike montage: the guarded modifier passes the
//     degenerate step's root motion through unchanged (and counts it), and still warps an ordinary step (finite output).
// The unguarded engine modifier raises the ensure on the same live state (watched failing 2026-10-28, see the evidence in
// production/qa/evidence/asn-pie-flake-fix-2026-10-28/).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimMontage.h"
#include "MotionWarpingComponent.h"
#include "Tests/Assassination/AssassinationTestWorld.h"
#include "View/EncounterPresentationSettings.h"
#include "View/StealthStrikeSkewWarp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationStrikeWarpGuardTest, "StealthGame.Assassination.StrikeWarpGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationStrikeWarpGuardTest::RunTest(const FString& Parameters)
{
	// Pure boundaries (root-motion space, cm).
	{
		FStrikeWarpStep Step;
		Step.WindowTranslation = FVector(40.0, 0.0, 0.0);
		Step.DeltaTranslation = FVector(-12.0, 0.0, 0.0);
		Step.RemainingTranslation = FVector::ZeroVector;
		TestTrue(TEXT("a step that moves the root with nothing left in the window is degenerate"), FStrikeWarpGuard::IsDegenerate(Step));
		Step.RemainingTranslation = FVector(0.00005, 0.0, 0.0);
		TestTrue(TEXT("a remaining translation below GetSafeNormal's tolerance is degenerate"), FStrikeWarpGuard::IsDegenerate(Step));
		Step.RemainingTranslation = FVector(0.5, 0.0, 0.0);
		TestFalse(TEXT("an ordinary step (root motion left in the window) is not degenerate"), FStrikeWarpGuard::IsDegenerate(Step));
		Step.RemainingTranslation = FVector::ZeroVector;
		Step.DeltaTranslation = FVector(0.0001, 0.0, 0.0);
		TestFalse(TEXT("a step without root translation is never skewed by the engine"), FStrikeWarpGuard::IsDegenerate(Step));
		Step.DeltaTranslation = FVector(-12.0, 0.0, 0.0);
		Step.WindowTranslation = FVector(0.0001, 0.0, 0.0);
		TestFalse(TEXT("a window without root translation takes the engine's add-translation path"), FStrikeWarpGuard::IsDegenerate(Step));
		Step.WindowTranslation = FVector(40.0, 0.0, 0.0);
		Step.bWarpTranslation = false;
		TestFalse(TEXT("translation warp off never skews"), FStrikeWarpGuard::IsDegenerate(Step));
	}

	// Live: the strike montage on a Shared Pawn's warping component.
	UAnimMontage* Montage = UEncounterPresentationSettings::Get()->StrikeFMontage.LoadSynchronous();
	if (!TestNotNull(TEXT("the strike montage loads"), Montage))
	{
		return false;
	}
	StealthAssassinationTest::FScopedWorld World(TEXT("StrikeWarpGuard"));
	AStealthPawn* Pawn = World.SpawnPawn(0.0);
	UMotionWarpingComponent* Warping = Pawn != nullptr ? Pawn->GetWarpingForStrike() : nullptr;
	if (!TestNotNull(TEXT("the pawn has its warping component"), Warping))
	{
		return false;
	}
	const float WindowEnd = FMath::Min(UEncounterPresentationSettings::Get()->StrikeClipContactSec, Montage->GetPlayLength());
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(AStealthPawn::StrikeWarpTargetName, Pawn->GetActorLocation() + FVector(150.0, 0.0, 0.0), FRotator::ZeroRotator);
	UStealthStrikeSkewWarp* Modifier = UStealthStrikeSkewWarp::AddStrikeSkewWarp(Warping, Montage, 0.0f, WindowEnd, AStealthPawn::StrikeWarpTargetName, FTransform::Identity, true);
	if (!TestNotNull(TEXT("the guarded modifier is added"), Modifier))
	{
		return false;
	}
	Modifier->TotalRootMotionWithinWindow = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Montage, 0.0f, WindowEnd);
	const FVector Window = Modifier->TotalRootMotionWithinWindow.GetTranslation();
	AddInfo(FString::Printf(TEXT("StrikeWarpGuard: window [0, %.3f] root translation %s"), WindowEnd, *Window.ToString()));
	if (!TestFalse(TEXT("the strike clip moves the root inside its warp window (the engine skews it)"), Window.IsNearlyZero(FStrikeWarpGuard::RootMotionThreshold)))
	{
		return false;
	}

	// The degenerate step: the position moved back from the window's end (previous = end, current earlier).
	const float Back = WindowEnd * 0.5f;
	Modifier->PreviousPosition = WindowEnd;
	Modifier->CurrentPosition = Back;
	const FTransform Step = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Montage, WindowEnd, Back);
	const int32 Before = UStealthStrikeSkewWarp::GetTotalGuardedSteps();
	const FTransform Out = Modifier->ProcessRootMotion(Step, 1.0f / 60.0f);
	TestTrue(TEXT("the degenerate step's root motion passes through unwarped"), Out.Equals(Step, 1e-6));
	TestEqual(TEXT("the modifier counted one guarded step"), Modifier->GetGuardedSteps(), 1);
	TestEqual(TEXT("the process-wide count moved by one"), UStealthStrikeSkewWarp::GetTotalGuardedSteps() - Before, 1);

	// An ordinary step inside the window is still warped by the engine skew (finite, guard not taken).
	Modifier->PreviousPosition = WindowEnd * 0.25f;
	Modifier->CurrentPosition = WindowEnd * 0.25f + 1.0f / 60.0f;
	const FTransform Ordinary = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Montage, Modifier->PreviousPosition, Modifier->CurrentPosition);
	const FTransform Warped = Modifier->ProcessRootMotion(Ordinary, 1.0f / 60.0f);
	TestFalse(TEXT("the ordinary step is finite"), Warped.ContainsNaN());
	TestEqual(TEXT("the ordinary step is not guarded"), Modifier->GetGuardedSteps(), 1);
	return true;
}

#endif
