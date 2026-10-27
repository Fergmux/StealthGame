// Story asn-011, AC3 / AC-ASN-15 PlantedBrace (ASN-R4; SCL BracePlanted observed from the Assassination side). A real
// AStealthPawn on a floor, AI-possessed (the NPC path; the brace is kind-blind), braces through the strike resolver's
// Prepare slot with the production stance adapter (FEncounterSclStanceSink) and the production pawn query and defence
// component. Over 4 s (inside the 4 s window cap plus nothing: CapEnd is resolved only afterwards) Move is held forward,
// reversed at 2 s, and Look sweeps +-90 degrees (past the 70.6 degree head clamp): server displacement <= 0.1 cm and body
// yaw change <= 0.1 degree, while the look target (the control rotation, the SCL head-look filter's input; the filter is
// SCL story 021) follows the sweep. Control: after CapEnd the stance is released and the same Move moves the pawn.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "AIController.h"
#include "Server/DefenceStateComponent.h"
#include "Server/EncounterService.h"
#include "Tests/Assassination/EncounterTestRig.h"
#include "Tests/Locomotion/LocomotionTestWorld.h"

namespace PlantedBraceTestData
{
	constexpr int32 HoldFrames = 240; // 4 s at 60 Hz
	constexpr double SweepDeg = 90.0;
	constexpr double MaxDisplacementCm = 0.1;
	constexpr double MaxYawDeg = 0.1;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationPlantedBraceTest, "StealthGame.Assassination.PlantedBrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationPlantedBraceTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace PlantedBraceTestData;
	StealthLocomotionTest::FWorld TestWorld(TEXT("PlantedBraceWorld"));
	AStealthPawn* Pawn = TestWorld.SpawnPawn(FVector2D::ZeroVector);
	AAIController* Controller = TestWorld.World->SpawnActor<AAIController>();
	Controller->bSetControlRotationFromPawnOrientation = false;
	Controller->Possess(Pawn);
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		StealthLocomotionTest::StepPawn(Pawn); // settle on the floor
	}

	// The resolver with the production pawn query, defence component and stance adapter.
	FFakeStealthClock Clock(100.0, 6000);
	FFakeQueue Queue;
	FFakeRoster Roster;
	FEncounterWorldPawnQuery Pawns(TestWorld.World);
	FEncounterComponentDefenceStore Defence;
	FEncounterSclStanceSink Stance;
	FAttackResolverDeps Deps;
	Deps.Clock = &Clock;
	Deps.Queue = &Queue;
	Deps.Roster = &Roster;
	Deps.Pawns = &Pawns;
	Deps.Defence = &Defence;
	Deps.Stance = &Stance;
	FAttackResolverCore Core(Deps);
	FCommandPayload Prepare;
	Prepare.Pawn.Pawn = Pawn;
	Prepare.Kind = static_cast<uint8>(ECommandKind::PrepareBegin);
	const EResolveResult Begin = Core.Resolve(FQueueItem::Make(EQueueItemType::PawnCommand, FEpochToken(), FMailboxId::ServerLocal(), Prepare), FQueueKey(6000, 1));
	TestTrue(TEXT("the Prepare executed in its slot"), Begin == EResolveResult::Executed && Core.GetRole(Pawn) == EStrikeRole::Bracing);
	TestTrue(TEXT("the stance is the SCL Brace pose"), Pawn->GetStealthMovement()->GetActionPose() == EStealthActionPose::Brace);
	TestTrue(TEXT("the defence component holds the stamp (created by the execution)"), UDefenceStateComponent::Find(*Pawn) != nullptr);

	// 4 s of Move (forward, reversed at 2 s) and a +-90 degree Look sweep.
	const FVector Planted = Pawn->GetActorLocation();
	const double PlantedYaw = Pawn->GetActorRotation().Yaw;
	double MaxDisplacement = 0.0, MaxYaw = 0.0, MaxLookError = 0.0, MaxLook = 0.0;
	for (int32 Frame = 0; Frame < HoldFrames; ++Frame)
	{
		StealthLocomotionTest::DriveMove(Pawn, Frame < HoldFrames / 2 ? 0.0f : 180.0f, 1.0f, EGait::Walk);
		const double LookYaw = SweepDeg * FMath::Sin(2.0 * UE_DOUBLE_PI * Frame / HoldFrames);
		Pawn->GetPawnCommand()->Look(FRotator(0.0, LookYaw, 0.0));
		StealthLocomotionTest::StepPawn(Pawn);
		MaxDisplacement = FMath::Max(MaxDisplacement, FVector::Dist2D(Planted, Pawn->GetActorLocation()));
		MaxYaw = FMath::Max(MaxYaw, FMath::Abs(FMath::FindDeltaAngleDegrees(PlantedYaw, Pawn->GetActorRotation().Yaw)));
		MaxLookError = FMath::Max(MaxLookError, FMath::Abs(FMath::FindDeltaAngleDegrees(LookYaw, Controller->GetControlRotation().Yaw)));
		MaxLook = FMath::Max(MaxLook, FMath::Abs(FMath::FindDeltaAngleDegrees(PlantedYaw, Controller->GetControlRotation().Yaw)));
	}
	AddInfo(FString::Printf(TEXT("PLANTED 4 s: displacement %.4f cm, body yaw %.4f deg, look target error %.4f deg, look reached %.1f deg"), MaxDisplacement, MaxYaw, MaxLookError, MaxLook));
	TestTrue(FString::Printf(TEXT("server displacement %.4f <= 0.1 cm"), MaxDisplacement), MaxDisplacement <= MaxDisplacementCm);
	TestTrue(FString::Printf(TEXT("body yaw change %.4f <= 0.1 deg"), MaxYaw), MaxYaw <= MaxYawDeg);
	TestTrue(TEXT("the look target follows the sweep"), MaxLookError <= 0.01);
	TestTrue(TEXT("the sweep went past the head clamp (no turn-to-look while braced)"), MaxLook >= SweepDeg - 1.0);
	TestTrue(TEXT("still braced after 4 s of Move"), Pawn->GetStealthMovement()->GetActionPose() == EStealthActionPose::Brace);

	// Control: the window's CapEnd releases the stance, and Move moves the pawn again.
	const FQueueItem* CapEnd = Queue.Scheduled.Num() > 0 ? &Queue.Scheduled[0].Item : nullptr;
	TestTrue(TEXT("CapEnd was scheduled at T_act + 4 s"), CapEnd != nullptr && FMath::IsNearlyEqual(Queue.Scheduled[0].Due, 104.0, 1e-6));
	if (CapEnd != nullptr)
	{
		Clock.Advance(4.0, 240);
		Core.Resolve(*CapEnd, FQueueKey(6240, 1));
	}
	TestTrue(TEXT("the window end releases the stance"), Pawn->GetStealthMovement()->GetActionPose() == EStealthActionPose::None);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		StealthLocomotionTest::DriveMove(Pawn, 0.0f, 1.0f, EGait::Walk);
		StealthLocomotionTest::StepPawn(Pawn);
	}
	TestTrue(TEXT("control: Move moves the released pawn"), FVector::Dist2D(Planted, Pawn->GetActorLocation()) > 10.0);
	return true;
}

#endif
