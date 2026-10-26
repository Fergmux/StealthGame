// Story SCL-018 (production/epics/shared-character-locomotion/story-018-ai-path-follow-routes-through-move.md),
// AC-SCL-40 AiPathFollowRoutesThroughMove (R5.7): GIVEN an NPC walking a 60 s route with corners, WHEN calls are
// traced, THEN 100 % of its accelerating input comes through Move(Direction, Gait), with 0 direct velocity requests
// (RequestDirectMove / RequestPathMove backstop hits, counted through the fault sink), no position jump or launch
// (per-frame displacement never exceeds speed x dt, the call-recording substitute for SetActorLocation / LaunchCharacter
// spies, which those non-virtual engine calls do not allow), |Direction| in {0.8, 0.9, 1.0}, and its release-to-stop is
// 0.75 s +- 1 frame over 53 +- 3 cm (as AC-SCL-06).
// The Crowd & NPC Behaviour follower does not exist yet: a minimal test follower that only calls Move drives the NPC
// (story note: the full Crowd follower must re-run this test). A negative control fires the backstops directly and
// checks they report and move nothing.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "Tests/Locomotion/LocomotionTestWorld.h"

namespace AiPathFollowTestData
{
	/** A square loop with corners (cm), walked repeatedly; about 60 s at the drawn paces. */
	const FVector2D Corners[] = { { 0.0, 0.0 }, { 1200.0, 0.0 }, { 1200.0, 900.0 }, { 300.0, 900.0 }, { 300.0, 300.0 }, { 0.0, 0.0 } };
	/** The pace of each leg, from the shared set (a leg boundary is the only place a pace changes). */
	const float LegPaces[] = { 1.0f, 0.9f, 1.0f, 0.8f, 1.0f };
	constexpr int32 Laps = 2;
	constexpr double CornerReachCm = 40.0;
	constexpr int32 MaxFrames = 60 * 90;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionAiPathFollowRoutesThroughMoveTest, "StealthGame.Locomotion.AiPathFollowRoutesThroughMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionAiPathFollowRoutesThroughMoveTest::RunTest(const FString& Parameters)
{
	using namespace AiPathFollowTestData;
	const float Frame = StealthLocomotionTest::Step;
	StealthLocomotionTest::FWorld TestWorld(TEXT("AiPathFollowTestWorld"));
	AStealthPawn* Npc = TestWorld.SpawnPawn(Corners[0]);
	UStealthMovementComponent* Movement = Npc->GetStealthMovement();
	FStealthRecordingFaultSink Faults;
	Movement->SetFaultSink(&Faults);
	AAIController* Controller = TestWorld.World->SpawnActor<AAIController>();
	Controller->Possess(Npc);
	UPawnCommandComponent* Command = Npc->GetPawnCommand();

	// The minimal follower: steer to the next corner with Move only; release at the F2 stop distance on the last leg.
	TArray<FVector2D> Route;
	TArray<float> Paces;
	for (int32 Lap = 0; Lap < Laps; ++Lap)
	{
		for (int32 Leg = 0; Leg < UE_ARRAY_COUNT(LegPaces); ++Leg)
		{
			Route.Add(Corners[Leg + 1]);
			Paces.Add(LegPaces[Leg]);
		}
	}
	TSet<float> MagnitudesSeen;
	int32 FramesWithInput = 0;
	int32 FramesWithInputOffSet = 0;
	double MaxJump = 0.0;
	int32 Frames = 0;
	int32 ReleaseFrame = INDEX_NONE;
	FVector ReleaseAt = FVector::ZeroVector;
	double SpeedAtRelease = 0.0;
	while (Frames < MaxFrames)
	{
		const FVector Here = Npc->GetActorLocation();
		if (ReleaseFrame == INDEX_NONE)
		{
			while (Route.Num() > 1 && FVector2D::Distance(FVector2D(Here), Route[0]) <= CornerReachCm)
			{
				Route.RemoveAt(0);
				Paces.RemoveAt(0);
			}
			const double Speed = StealthLocomotionTest::Speed2D(Npc);
			const double StopCm = Speed > 0.0 ? Speed * Speed / (2.0 * StealthLocomotion::StopDeceleration(Movement->GetLocomotionTuning(), static_cast<float>(Speed), EGait::Walk)) : 0.0;
			if (Route.Num() == 1 && FVector2D::Distance(FVector2D(Here), Route[0]) <= StopCm + 0.5)
			{
				Command->Move(FVector2D::ZeroVector, EGait::Walk);
				ReleaseFrame = Frames;
				ReleaseAt = Here;
				SpeedAtRelease = Speed;
			}
			else
			{
				const FVector2D To = (Route[0] - FVector2D(Here)).GetSafeNormal();
				Command->Move(FVector2D(To.Y, To.X) * Paces[0], EGait::Walk); // X right, Y forward at control yaw 0
			}
		}
		const FVector2D Intent = Command->GetMoveIntent();
		if (!Intent.IsNearlyZero())
		{
			++FramesWithInput;
			const float Magnitude = FMath::RoundToFloat(static_cast<float>(Intent.Size()) * 100.0f) / 100.0f;
			MagnitudesSeen.Add(Magnitude);
			FramesWithInputOffSet += (Magnitude == 0.8f || Magnitude == 0.9f || Magnitude == 1.0f) ? 0 : 1;
		}
		// Keep the control yaw at 0 so the follower's Direction basis is the world frame.
		Controller->SetControlRotation(FRotator::ZeroRotator);
		const FVector Before = Npc->GetActorLocation();
		const double SpeedBefore = StealthLocomotionTest::Speed2D(Npc);
		StealthLocomotionTest::StepPawn(Npc);
		const double Moved = FVector::Dist2D(Before, Npc->GetActorLocation());
		MaxJump = FMath::Max(MaxJump, Moved - FMath::Max(SpeedBefore, StealthLocomotionTest::Speed2D(Npc)) * Frame);
		++Frames;
		if (ReleaseFrame != INDEX_NONE && StealthLocomotionTest::Speed2D(Npc) <= 0.0)
		{
			break;
		}
	}
	const double StopSec = (Frames - ReleaseFrame) * Frame;
	const double StopCm = FVector::Dist2D(ReleaseAt, Npc->GetActorLocation());
	TArray<float> Seen = MagnitudesSeen.Array();
	Seen.Sort();
	FString SeenText;
	for (float M : Seen) { SeenText += FString::Printf(TEXT("%.2f "), M); }
	AddInfo(FString::Printf(TEXT("AIPATH %.1f s route, %d frames with Move input, |Direction| seen {%s}, max jump over speed x dt %.4f cm, backstop hits %d; release at %.1f cm/s: stop %.3f s over %.2f cm"),
		Frames * Frame, FramesWithInput, *SeenText, MaxJump, Movement->GetBackstopHitCount(), SpeedAtRelease, StopSec, StopCm));
	TestTrue(TEXT("the route lasts about a minute"), Frames * Frame >= 40.0);
	TestTrue(TEXT("the follower finished the route"), ReleaseFrame != INDEX_NONE);
	TestEqual(TEXT("0 direct velocity or path move requests (backstop hits)"), Movement->GetBackstopHitCount(), 0);
	TestEqual(TEXT("every frame of input has |Direction| in {0.8, 0.9, 1.0}"), FramesWithInputOffSet, 0);
	TestTrue(FString::Printf(TEXT("no position jump or launch: displacement never exceeds speed x dt (%.4f cm over)"), MaxJump), MaxJump <= 0.05);
	TestTrue(FString::Printf(TEXT("release at full pace (%.1f cm/s)"), SpeedAtRelease), FMath::Abs(SpeedAtRelease - 140.0) <= 1.0);
	TestTrue(FString::Printf(TEXT("release-to-stop 0.75 s +- 1 frame (%.3f)"), StopSec), FMath::Abs(StopSec - 0.75) <= Frame + 1e-4);
	TestTrue(FString::Printf(TEXT("release-to-stop over 53 +- 3 cm (%.2f)"), StopCm), FMath::Abs(StopCm - 53.0) <= 3.0);

	// Negative control: the engine path entry points report to the fault sink and move nothing.
	{
		const FVector Before = Npc->GetActorLocation();
		Movement->RequestDirectMove(FVector(300.0, 0.0, 0.0), true);
		Movement->RequestPathMove(FVector(1.0, 0.0, 0.0));
		for (int32 Index = 0; Index < 30; ++Index)
		{
			StealthLocomotionTest::StepPawn(Npc);
		}
		TestEqual(TEXT("control: both backstops counted"), Movement->GetBackstopHitCount(), 2);
		TestEqual(TEXT("control: both reached the fault sink"), Faults.GetRecords().FilterByPredicate([](const FStealthFaultRecord& R) { return R.Fault == EStealthFault::MovementMisuse; }).Num(), 2);
		TestTrue(TEXT("control: the requests moved nothing"), FVector::Dist2D(Before, Npc->GetActorLocation()) <= 0.01);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
