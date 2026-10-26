// Story SCL-015 (production/epics/shared-character-locomotion/story-015-brace-planted.md), AC-SCL-11 BracePlanted,
// movement half: GIVEN a pawn walking at 140 cm/s, WHEN ActionPose = Brace is written (a stub pose write; the IIF
// FActionState writer is not built), THEN the entry moves it 53 cm +- 10 % (the F2 stop distance at the entry speed,
// which the distance-matched brace entry clip must equal) and it then holds; WHEN Move (Walk and Run, 8 directions) is
// issued for 2 s, THEN horizontal displacement is 0 +- 1 cm, yaw change is 0 +- 0.5 degrees and the brace is not
// cancelled (Run is clamped to Walk). The head-look clause (head yaw within +-70.6, pitch +-20.1, <= 140 deg/s) is the
// F6 filter of story SCL-021 and is asserted there; the step-into-stance clip itself is animation content (visible half).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Locomotion/LocomotionTestWorld.h"

namespace BracePlantedTestData
{
	constexpr double EntryCm = 53.0;
	constexpr double EntryTolerance = 0.10;
	constexpr int32 HoldFrames = 120;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionBracePlantedTest, "StealthGame.Locomotion.BracePlanted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionBracePlantedTest::RunTest(const FString& Parameters)
{
	using namespace BracePlantedTestData;
	StealthLocomotionTest::FWorld TestWorld(TEXT("BracePlantedTestWorld"));
	AStealthPawn* Pawn = TestWorld.SpawnPawn(FVector2D(-5000.0, 0.0));
	UStealthMovementComponent* Movement = Pawn->GetStealthMovement();

	// Walk to 140, then write the brace while the stick stays forward.
	StealthLocomotionTest::DriveMove(Pawn, 0.0f, 1.0f, EGait::Walk);
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		StealthLocomotionTest::StepPawn(Pawn);
	}
	TestTrue(TEXT("walking at 140 before the brace"), FMath::Abs(StealthLocomotionTest::Speed2D(Pawn) - 140.0) <= 1.0);
	const FVector EntryStart = Pawn->GetActorLocation();
	Movement->SetActionPose(EStealthActionPose::Brace);
	for (int32 Frame = 0; Frame < 90; ++Frame)
	{
		StealthLocomotionTest::StepPawn(Pawn);
	}
	const double Entry = FVector::Dist2D(EntryStart, Pawn->GetActorLocation());
	AddInfo(FString::Printf(TEXT("BRACE entry from 140 cm/s: %.2f cm, then speed %.2f"), Entry, StealthLocomotionTest::Speed2D(Pawn)));
	TestTrue(FString::Printf(TEXT("the brace entry moves %.2f cm, 53 +- 10 %%"), Entry), FMath::Abs(Entry - EntryCm) <= EntryCm * EntryTolerance);
	TestTrue(TEXT("then it holds (speed 0)"), StealthLocomotionTest::Speed2D(Pawn) <= 0.01);

	// 8 directions x {Walk, Run} for 2 s: no translation, no yaw, the brace kept.
	const FVector Planted = Pawn->GetActorLocation();
	const double PlantedYaw = Pawn->GetActorRotation().Yaw;
	double MaxDisplacement = 0.0;
	double MaxYawChange = 0.0;
	for (const EGait Gait : { EGait::Walk, EGait::Run })
	{
		for (int32 Direction = 0; Direction < 8; ++Direction)
		{
			StealthLocomotionTest::DriveMove(Pawn, 45.0f * Direction, 1.0f, Gait);
			for (int32 Frame = 0; Frame < HoldFrames / 8; ++Frame)
			{
				StealthLocomotionTest::StepPawn(Pawn);
				MaxDisplacement = FMath::Max(MaxDisplacement, FVector::Dist2D(Planted, Pawn->GetActorLocation()));
				MaxYawChange = FMath::Max(MaxYawChange, FMath::Abs(FMath::FindDeltaAngleDegrees(PlantedYaw, Pawn->GetActorRotation().Yaw)));
			}
		}
	}
	AddInfo(FString::Printf(TEXT("BRACE hold under Move: max displacement %.3f cm, max yaw change %.3f deg"), MaxDisplacement, MaxYawChange));
	TestTrue(FString::Printf(TEXT("braced: horizontal displacement %.3f <= 1 cm"), MaxDisplacement), MaxDisplacement <= 1.0);
	TestTrue(FString::Printf(TEXT("braced: yaw change %.3f <= 0.5 deg"), MaxYawChange), MaxYawChange <= 0.5);
	TestTrue(TEXT("braced: Move never cancels the brace"), Movement->GetActionPose() == EStealthActionPose::Brace);
	TestTrue(TEXT("braced: a requested Run is clamped to Walk"), Movement->GetEffectiveGait() == EGait::Walk);

	// Releasing the brace lets Move act again (control).
	Movement->SetActionPose(EStealthActionPose::None);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		StealthLocomotionTest::StepPawn(Pawn);
	}
	TestTrue(TEXT("control: after the brace ends Move moves the pawn"), FVector::Dist2D(Planted, Pawn->GetActorLocation()) > 10.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
