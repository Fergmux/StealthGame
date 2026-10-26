// Story SCL-016 (production/epics/shared-character-locomotion/story-016-commanded-vs-external-displacement.md),
// AC-SCL-15 CommandedVsExternalDisplacement (R16): GIVEN a tending pawn (ActionPose InteractPose, zero Move input) and
// 20 NPC brushes (NPC pawns walking into its capsule) plus one depenetration push of 10 cm, WHEN CommandedDisplacementCm
// is read (cumulatively since the test start, per the QA clarification), THEN it is 0 and the tending pawn moved <= 1 cm
// from contact (the brushes never move it; the push lands in the external bucket); a 20 cm stick nudge (Move input held,
// then released; the release stop is the commanded motion ending) reads 20 +- 1, the distance the nudge moved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Locomotion/LocomotionTestWorld.h"

namespace CommandedVsExternalDisplacementTestData
{
	constexpr int32 BrushCount = 20;
	constexpr double PushCm = 10.0;
	constexpr double NudgeCm = 20.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionCommandedVsExternalDisplacementTest, "StealthGame.Locomotion.CommandedVsExternalDisplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionCommandedVsExternalDisplacementTest::RunTest(const FString& Parameters)
{
	using namespace CommandedVsExternalDisplacementTestData;
	StealthLocomotionTest::FWorld TestWorld(TEXT("CommandedDisplacementTestWorld"));
	AStealthPawn* Tending = TestWorld.SpawnPawn(FVector2D(0.0, 0.0));
	UStealthMovementComponent* Movement = Tending->GetStealthMovement();
	Movement->SetActionPose(EStealthActionPose::InteractPose);
	const double Since = TestWorld.World->GetTimeSeconds();
	StealthLocomotionTest::StepPawn(Tending);
	TestWorld.World->TimeSeconds += StealthLocomotionTest::Step;
	const FVector Contact = Tending->GetActorLocation();

	// 20 brushes: NPC pawns from around the circle walk into the tending capsule and past it.
	double MaxMoved = 0.0;
	for (int32 Brush = 0; Brush < BrushCount; ++Brush)
	{
		const float FromYaw = 360.0f * Brush / BrushCount;
		const FVector Start = Contact + FRotator(0.0, FromYaw, 0.0).Vector() * 250.0;
		AStealthPawn* Npc = TestWorld.SpawnPawn(FVector2D(Start.X, Start.Y), FromYaw + 180.0f);
		StealthLocomotionTest::DriveMove(Npc, FromYaw + 180.0f + (Brush % 2 == 0 ? 8.0f : -8.0f), 1.0f, EGait::Walk);
		for (int32 Frame = 0; Frame < 240; ++Frame)
		{
			StealthLocomotionTest::StepPawn(Npc);
			StealthLocomotionTest::StepPawn(Tending);
			TestWorld.World->TimeSeconds += StealthLocomotionTest::Step;
			MaxMoved = FMath::Max(MaxMoved, FVector::Dist2D(Contact, Tending->GetActorLocation()));
		}
		Npc->Destroy();
	}
	const float CommandedAfterBrushes = Movement->GetCommandedDisplacementCm(Since);
	AddInfo(FString::Printf(TEXT("DISPLACEMENT 20 brushes: tending pawn moved max %.3f cm, commanded %.3f cm"), MaxMoved, CommandedAfterBrushes));
	TestTrue(FString::Printf(TEXT("brushes never move the tending pawn more than 1 cm (%.3f)"), MaxMoved), MaxMoved <= 1.0);

	// One 10 cm depenetration push (an external move of the capsule with zero input), then a frame of movement.
	const float ExternalBefore = Movement->GetExternalDisplacementTotalCm();
	FHitResult Hit;
	Movement->SafeMoveUpdatedComponent(FVector(PushCm, 0.0, 0.0), Tending->GetActorQuat(), true, Hit);
	const FVector Pushed = Tending->GetActorLocation();
	Movement->Velocity = FVector::ZeroVector;
	StealthLocomotionTest::StepPawn(Tending);
	TestWorld.World->TimeSeconds += StealthLocomotionTest::Step;
	const float Commanded = Movement->GetCommandedDisplacementCm(Since);
	AddInfo(FString::Printf(TEXT("DISPLACEMENT push: moved %.2f cm, commanded %.3f, external delta %.3f"), FVector::Dist2D(Contact, Pushed), Commanded, Movement->GetExternalDisplacementTotalCm() - ExternalBefore));
	TestTrue(FString::Printf(TEXT("tending pawn: CommandedDisplacementCm is 0 after the brushes and the push (%.3f)"), Commanded), Commanded <= 0.01f);
	TestTrue(TEXT("the 10 cm push lands in the external bucket"), FMath::Abs(Movement->GetExternalDisplacementTotalCm() - ExternalBefore - PushCm) <= 0.5);

	// A 20 cm stick nudge: hold Move until the predicted total (distance so far + the stop distance) reaches 20 cm.
	Movement->SetActionPose(EStealthActionPose::None);
	const FVector NudgeStart = Tending->GetActorLocation();
	const float BeforeNudge = Movement->GetCommandedDisplacementCm(Since);
	StealthLocomotionTest::DriveMove(Tending, 0.0f, 1.0f, EGait::Walk);
	const FLocomotionTuning_Public& Tuning = Movement->GetLocomotionTuning();
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		// Release before the step whose distance plus the stop that follows would pass 20 cm.
		const double Next = FMath::Min(StealthLocomotionTest::Speed2D(Tending) + StealthLocomotion::WalkAcceleration(Tuning) * StealthLocomotionTest::Step, static_cast<double>(Tuning.WalkSpeedCmPerSec));
		const double NextStop = Next * Next / (2.0 * StealthLocomotion::StopDeceleration(Tuning, static_cast<float>(Next), EGait::Walk));
		const double Now = StealthLocomotionTest::Speed2D(Tending);
		const double Here = FVector::Dist2D(NudgeStart, Tending->GetActorLocation());
		const double NowTotal = Here + (Now > 0.0 ? Now * Now / (2.0 * StealthLocomotion::StopDeceleration(Tuning, static_cast<float>(Now), EGait::Walk)) : 0.0);
		const double NextTotal = Here + Next * StealthLocomotionTest::Step + NextStop;
		if (NextTotal > NudgeCm && FMath::Abs(NowTotal - NudgeCm) <= FMath::Abs(NextTotal - NudgeCm))
		{
			break;
		}
		StealthLocomotionTest::StepPawn(Tending);
		TestWorld.World->TimeSeconds += StealthLocomotionTest::Step;
	}
	StealthLocomotionTest::DriveMove(Tending, 0.0f, 0.0f, EGait::Walk);
	for (int32 Frame = 0; Frame < 90; ++Frame)
	{
		StealthLocomotionTest::StepPawn(Tending);
		TestWorld.World->TimeSeconds += StealthLocomotionTest::Step;
	}
	const double Nudged = FVector::Dist2D(NudgeStart, Tending->GetActorLocation());
	const float NudgeReading = Movement->GetCommandedDisplacementCm(Since) - BeforeNudge;
	AddInfo(FString::Printf(TEXT("DISPLACEMENT nudge: moved %.2f cm, commanded reading %.2f cm"), Nudged, NudgeReading));
	TestTrue(FString::Printf(TEXT("the nudge moved about 20 cm (%.2f)"), Nudged), FMath::Abs(Nudged - NudgeCm) <= 2.0);
	TestTrue(FString::Printf(TEXT("a 20 cm nudge reads 20 +- 1 (%.2f)"), NudgeReading), FMath::Abs(NudgeReading - NudgeCm) <= 1.0);
	TestTrue(FString::Printf(TEXT("the reading equals the distance moved (%.2f vs %.2f)"), NudgeReading, Nudged), FMath::Abs(NudgeReading - Nudged) <= 0.5);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
