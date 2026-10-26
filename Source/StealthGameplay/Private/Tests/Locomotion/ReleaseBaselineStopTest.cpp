// Story SCL-011 (production/epics/shared-character-locomotion/story-011-release-baseline-stop.md), AC-SCL-06
// ReleaseBaselineStop, headless half: GIVEN a pawn at 140 cm/s (and a second run at 112 cm/s) at 60 fps, WHEN input is
// released, THEN it reaches 0 in 0.75 s (0.60 s at 112) +- 1 frame over 53 +- 3 cm (34 +- 3 cm), peak deceleration
// <= 187 cm/s^2 + 5 %, and an AI pawn given the same commands (UPawnCommandComponent::Move) differs from the owner input
// path by <= 1 frame and 1 cm. The saved move carries the requested gait (FLAG_Custom_0) and restores the model latches
// on replay (FSavedMove_Stealth), checked here directly. The owner-prediction-at-100-ms-lag half is the PIE test
// StealthGame.Locomotion.PIE.ReleaseBaselineStopUnderLag (StealthTestHarness).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/Movement/StealthSavedMove.h"
#include "Tests/Locomotion/LocomotionTestWorld.h"
#include "View/Core/StealthLocomotionModel.h"

namespace ReleaseBaselineStopTestData
{
	struct FCase
	{
		float Magnitude;
		double SteadySpeed;
		double StopSec;
		double StopCm;
	};
	const FCase Cases[] = {
		{ 1.0f, 140.0, 0.75, 53.0 },
		{ 0.8f, 112.0, 0.60, 34.0 },
	};
	constexpr int32 WalkFrames = 120;
	constexpr int32 MaxStopFrames = 120;
	constexpr double DistanceTolerance = 3.0;
	constexpr double PeakDecelLimit = 187.0 * 1.05;

	struct FStopTrace
	{
		int32 Frames = 0;
		double Distance = 0.0;
		double PeakDecel = 0.0;
		double SpeedAtRelease = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionReleaseBaselineStopTest, "StealthGame.Locomotion.ReleaseBaselineStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionReleaseBaselineStopTest::RunTest(const FString& Parameters)
{
	using namespace ReleaseBaselineStopTestData;
	const float Frame = StealthLocomotionTest::Step;
	StealthLocomotionTest::FWorld TestWorld(TEXT("ReleaseBaselineStopTestWorld"));
	int32 Lane = 0;
	for (const FCase& Case : Cases)
	{
		AStealthPawn* Owner = TestWorld.SpawnPawn(FVector2D(-3000.0, -3000.0 + 700.0 * Lane++));
		AStealthPawn* Ai = TestWorld.SpawnPawn(FVector2D(-3000.0, -3000.0 + 700.0 * Lane++));
		StealthLocomotionTest::DriveMove(Ai, 0.0f, Case.Magnitude, EGait::Walk);
		for (int32 Index = 0; Index < WalkFrames; ++Index)
		{
			StealthLocomotionTest::DriveOwner(Owner, 0.0f, Case.Magnitude, EGait::Walk);
			StealthLocomotionTest::StepPawn(Owner);
			StealthLocomotionTest::StepPawn(Ai);
		}
		// Release on the same frame.
		StealthLocomotionTest::DriveMove(Ai, 0.0f, 0.0f, EGait::Walk);
		FStopTrace Traces[2];
		AStealthPawn* Pawns[2] = { Owner, Ai };
		for (int32 Which = 0; Which < 2; ++Which)
		{
			Traces[Which].SpeedAtRelease = StealthLocomotionTest::Speed2D(Pawns[Which]);
		}
		const FVector Start[2] = { Owner->GetActorLocation(), Ai->GetActorLocation() };
		for (int32 Index = 0; Index < MaxStopFrames; ++Index)
		{
			for (int32 Which = 0; Which < 2; ++Which)
			{
				AStealthPawn* Pawn = Pawns[Which];
				const double Before = StealthLocomotionTest::Speed2D(Pawn);
				if (Before <= 0.0)
				{
					continue;
				}
				StealthLocomotionTest::StepPawn(Pawn);
				const double After = StealthLocomotionTest::Speed2D(Pawn);
				Traces[Which].PeakDecel = FMath::Max(Traces[Which].PeakDecel, (Before - After) / Frame);
				++Traces[Which].Frames;
			}
		}
		for (int32 Which = 0; Which < 2; ++Which)
		{
			Traces[Which].Distance = FVector::Dist2D(Start[Which], Pawns[Which]->GetActorLocation());
			const FStopTrace& T = Traces[Which];
			const TCHAR* Kind = Which == 0 ? TEXT("owner path") : TEXT("AI Move path");
			AddInfo(FString::Printf(TEXT("RELEASESTOP %.0f cm/s %s: %.3f s (%d frames) over %.2f cm, peak decel %.2f cm/s^2"), Case.SteadySpeed, Kind, T.Frames * Frame, T.Frames, T.Distance, T.PeakDecel));
			TestTrue(FString::Printf(TEXT("%s at %.0f: steady before release"), Kind, Case.SteadySpeed), FMath::Abs(T.SpeedAtRelease - Case.SteadySpeed) <= 1.0);
			TestTrue(FString::Printf(TEXT("%s at %.0f: reaches 0 in %.2f s +- 1 frame"), Kind, Case.SteadySpeed, Case.StopSec), FMath::Abs(T.Frames * Frame - Case.StopSec) <= Frame + 1e-4);
			TestTrue(FString::Printf(TEXT("%s at %.0f: over %.0f +- 3 cm (%.2f)"), Kind, Case.SteadySpeed, Case.StopCm, T.Distance), FMath::Abs(T.Distance - Case.StopCm) <= DistanceTolerance);
			TestTrue(FString::Printf(TEXT("%s at %.0f: peak deceleration %.1f <= 187 + 5 %%"), Kind, Case.SteadySpeed, T.PeakDecel), T.PeakDecel <= PeakDecelLimit);
		}
		TestTrue(FString::Printf(TEXT("%.0f: AI and owner stop within 1 frame"), Case.SteadySpeed), FMath::Abs(Traces[0].Frames - Traces[1].Frames) <= 1);
		TestTrue(FString::Printf(TEXT("%.0f: AI and owner stop within 1 cm"), Case.SteadySpeed), FMath::Abs(Traces[0].Distance - Traces[1].Distance) <= 1.0);
	}

	// The saved move: FLAG_Custom_0 carries Run, and PrepMoveFor restores the latch state the move started with.
	{
		AStealthPawn* Pawn = TestWorld.SpawnPawn(FVector2D(3000.0, 0.0));
		UStealthMovementComponent* Movement = Pawn->GetStealthMovement();
		FSavedMove_Stealth Move;
		Move.Clear();
		Move.bSavedWantsToRun = true;
		TestTrue(TEXT("a Run move sets FLAG_Custom_0"), (Move.GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0) != 0);
		Move.bSavedWantsToRun = false;
		TestTrue(TEXT("a Walk move clears FLAG_Custom_0"), (Move.GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0) == 0);
		Move.StartMotionState.bStopping = true;
		Move.StartMotionState.StopDecel = 123.0f;
		Move.StartMotionState.bReversalLatched = true;
		Move.StartMotionState.Pace = 0.9f;
		Move.PrepMoveFor(Pawn);
		const FStealthMotionState& Restored = Movement->GetMotionState();
		TestTrue(TEXT("PrepMoveFor restores the stop latch, the reversal latch and the pace"), Restored.bStopping && Restored.StopDecel == 123.0f && Restored.bReversalLatched && Restored.Pace == 0.9f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
