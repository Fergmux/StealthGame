// Story SCL-013 (production/epics/shared-character-locomotion/story-013-isrunning-hysteresis.md), AC-SCL-08
// IsRunningHysteresis: GIVEN speed ramps 0 -> 450 -> 0 and then dithers +-10 cm/s around 290 for 5 s, WHEN IsRunning is
// sampled, THEN it turns true above 300, stays true down to 275, turns false below 275, does not toggle during the dither
// (run from both initial states), falls 0.29 s +- 1 frame after Run is released at 450 (the stick stays forward: the
// run -> walk slowdown at the run deceleration), and is false throughout a vault (the traversal movement mode, a stub
// until stories 047/048) and a strike (a stubbed Strike action pose) even when horizontal speed exceeds 300.
// IsRunning is derived from speed, not from the Run state, and starts false at spawn.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Locomotion/LocomotionTestWorld.h"
#include "View/Core/StealthLocomotionModel.h"

namespace IsRunningHysteresisTestData
{
	constexpr float Threshold = 300.0f;
	constexpr float Hysteresis = 25.0f;
	constexpr int32 DitherFrames = 300;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionIsRunningHysteresisTest, "StealthGame.Locomotion.IsRunningHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionIsRunningHysteresisTest::RunTest(const FString& Parameters)
{
	using namespace IsRunningHysteresisTestData;
	const float Frame = StealthLocomotionTest::Step;

	// The pure function over a synthetic ramp 0 -> 450 -> 0.
	{
		bool bRunning = false;
		float FirstTrue = -1.0f;
		float FirstFalse = -1.0f;
		for (float Speed = 0.0f; Speed <= 450.0f; Speed += 1.0f)
		{
			const bool bNow = StealthLocomotion::StepIsRunning(bRunning, Speed, Threshold, Hysteresis, false);
			if (bNow && !bRunning && FirstTrue < 0.0f) { FirstTrue = Speed; }
			bRunning = bNow;
		}
		for (float Speed = 450.0f; Speed >= 0.0f; Speed -= 1.0f)
		{
			const bool bNow = StealthLocomotion::StepIsRunning(bRunning, Speed, Threshold, Hysteresis, false);
			if (!bNow && bRunning && FirstFalse < 0.0f) { FirstFalse = Speed; }
			bRunning = bNow;
		}
		TestEqual(TEXT("ramp up: turns true at the first speed above 300"), FirstTrue, 301.0f);
		TestEqual(TEXT("ramp down: stays true down to 275, false below"), FirstFalse, 274.0f);
		TestTrue(TEXT("at exactly 275 a running pawn is still running"), StealthLocomotion::StepIsRunning(true, 275.0f, Threshold, Hysteresis, false));
		TestFalse(TEXT("at exactly 300 a walking pawn is not running"), StealthLocomotion::StepIsRunning(false, 300.0f, Threshold, Hysteresis, false));
		TestFalse(TEXT("forced false wins at any speed"), StealthLocomotion::StepIsRunning(true, 450.0f, Threshold, Hysteresis, true));
	}

	// Dither +-10 around 290 for 5 s from both initial states: no toggle.
	for (const bool bInitial : { false, true })
	{
		bool bRunning = bInitial;
		int32 Toggles = 0;
		for (int32 Index = 0; Index < DitherFrames; ++Index)
		{
			const float Speed = 290.0f + ((Index / 3) % 2 == 0 ? 10.0f : -10.0f);
			const bool bNow = StealthLocomotion::StepIsRunning(bRunning, Speed, Threshold, Hysteresis, false);
			Toggles += bNow != bRunning ? 1 : 0;
			bRunning = bNow;
		}
		TestEqual(FString::Printf(TEXT("dither from %s: no toggle"), bInitial ? TEXT("true") : TEXT("false")), Toggles, 0);
	}

	// World: a pawn runs to 450 and releases Run (stick forward); IsRunning falls 0.29 s +- 1 frame later.
	StealthLocomotionTest::FWorld TestWorld(TEXT("IsRunningHysteresisTestWorld"));
	AStealthPawn* Pawn = TestWorld.SpawnPawn(FVector2D(-8000.0, 0.0));
	UStealthMovementComponent* Movement = Pawn->GetStealthMovement();
	TestFalse(TEXT("IsRunning starts false at spawn"), Movement->IsRunning());
	StealthLocomotionTest::DriveMove(Pawn, 0.0f, 1.0f, EGait::Run);
	for (int32 Index = 0; Index < 90; ++Index)
	{
		StealthLocomotionTest::StepPawn(Pawn);
	}
	TestTrue(FString::Printf(TEXT("running at 450 (%.1f): IsRunning"), StealthLocomotionTest::Speed2D(Pawn)), Movement->IsRunning() && StealthLocomotionTest::Speed2D(Pawn) >= 449.0);
	StealthLocomotionTest::DriveMove(Pawn, 0.0f, 1.0f, EGait::Walk);
	int32 FallFrame = INDEX_NONE;
	for (int32 Index = 1; Index <= 60 && FallFrame == INDEX_NONE; ++Index)
	{
		StealthLocomotionTest::StepPawn(Pawn);
		if (!Movement->IsRunning())
		{
			FallFrame = Index;
		}
	}
	const float FallSec = FallFrame * Frame;
	AddInfo(FString::Printf(TEXT("ISRUNNING falls %.3f s after Run is released at 450 (frame %d; speed now %.1f)"), FallSec, FallFrame, StealthLocomotionTest::Speed2D(Pawn)));
	TestTrue(FString::Printf(TEXT("IsRunning falls 0.29 s +- 1 frame after release (%.3f)"), FallSec), FallFrame != INDEX_NONE && FMath::Abs(FallSec - 0.29f) <= Frame + 0.005f);

	// A strike pose and the traversal mode force it false while the horizontal speed is still above 300.
	{
		StealthLocomotionTest::DriveMove(Pawn, 0.0f, 1.0f, EGait::Run);
		for (int32 Index = 0; Index < 90; ++Index) { StealthLocomotionTest::StepPawn(Pawn); }
		TestTrue(TEXT("running again"), Movement->IsRunning());
		Movement->SetActionPose(EStealthActionPose::Strike);
		StealthLocomotionTest::StepPawn(Pawn);
		TestTrue(FString::Printf(TEXT("strike pose: IsRunning false at %.1f cm/s"), StealthLocomotionTest::Speed2D(Pawn)), !Movement->IsRunning() && StealthLocomotionTest::Speed2D(Pawn) > 300.0);
		Movement->SetActionPose(EStealthActionPose::None);
		for (int32 Index = 0; Index < 30; ++Index) { StealthLocomotionTest::StepPawn(Pawn); }
		TestTrue(TEXT("pose cleared at speed: IsRunning again"), Movement->IsRunning());
		// The traversal movement mode (MOVE_Flying, spike SCL-045) carries the run velocity.
		Movement->SetMovementMode(UStealthMovementComponent::TraversalMovementMode);
		Movement->Velocity = FVector(450.0, 0.0, 0.0);
		bool bAnyRunning = false;
		for (int32 Index = 0; Index < 10; ++Index)
		{
			StealthLocomotionTest::StepPawn(Pawn);
			bAnyRunning |= Movement->IsRunning();
		}
		TestTrue(TEXT("traversal (vault stub): IsInTraversal"), Movement->IsInTraversal());
		TestFalse(TEXT("traversal (vault stub): IsRunning false throughout"), bAnyRunning);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
