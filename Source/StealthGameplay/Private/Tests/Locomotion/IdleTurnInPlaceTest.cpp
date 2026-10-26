// Story SCL-017 (production/epics/shared-character-locomotion/story-017-idle-turn-in-place.md), AC-SCL-45
// IdleTurnInPlace (R5.5): GIVEN an idle pawn whose look target is held at 100 degrees yaw, WHEN 0.49 s and then 0.51 s
// pass, THEN the body has not turned at 0.49 s, starts turning at <= 120 deg/s after 0.50 s +- 1 frame, and stops when
// head yaw <= 35 degrees; identical for an AI focus target. At a 1/60 s step (QA clarification): no body turn through
// frame 29 (0.483 s) and turning begun by frame 31 (0.517 s).
// Two pawns, two look paths, the same assertions and the same trace: the human look writes the control rotation as the
// server does from ServerMove (Controller->SetControlRotation), the AI writes it through UPawnCommandComponent::Look.
// The unclamped look target is the control rotation, the F6 filter input (story SCL-021 builds the filter itself).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Tests/Locomotion/LocomotionTestWorld.h"

namespace IdleTurnInPlaceTestData
{
	constexpr float TargetYawDeg = 100.0f;
	constexpr int32 Frames = 120;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionIdleTurnInPlaceTest, "StealthGame.Locomotion.IdleTurnInPlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionIdleTurnInPlaceTest::RunTest(const FString& Parameters)
{
	using namespace IdleTurnInPlaceTestData;
	const FLocomotionTuning_Public Tuning;
	StealthLocomotionTest::FWorld TestWorld(TEXT("IdleTurnInPlaceTestWorld"));
	TArray<TArray<double>> Yaws;
	for (int32 Path = 0; Path < 2; ++Path)
	{
		AStealthPawn* Pawn = TestWorld.SpawnPawn(FVector2D(0.0, 800.0 * Path));
		AAIController* Controller = TestWorld.World->SpawnActor<AAIController>();
		Controller->bSetControlRotationFromPawnOrientation = false;
		Controller->Possess(Pawn);
		const FRotator Target(0.0, TargetYawDeg, 0.0);
		if (Path == 0)
		{
			Controller->SetControlRotation(Target); // the human path: the server writes the ServerMove view rotation
		}
		else
		{
			Pawn->GetPawnCommand()->Look(Target); // the AI focus path
		}
		TArray<double>& Trace = Yaws.AddDefaulted_GetRef();
		for (int32 Frame = 1; Frame <= Frames; ++Frame)
		{
			StealthLocomotionTest::StepPawn(Pawn);
			Trace.Add(Pawn->GetActorRotation().Yaw);
		}
	}

	for (int32 Path = 0; Path < 2; ++Path)
	{
		const TCHAR* Name = Path == 0 ? TEXT("human look") : TEXT("AI focus");
		const TArray<double>& Trace = Yaws[Path];
		int32 FirstTurn = INDEX_NONE;
		double MaxRate = 0.0;
		for (int32 Index = 0; Index < Trace.Num(); ++Index)
		{
			const double Previous = Index == 0 ? 0.0 : Trace[Index - 1];
			const double Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(Previous, Trace[Index]));
			MaxRate = FMath::Max(MaxRate, Delta / StealthLocomotionTest::Step);
			if (FirstTurn == INDEX_NONE && FMath::Abs(Trace[Index]) > 1e-3)
			{
				FirstTurn = Index + 1; // frame number (1-based)
			}
		}
		const double FinalHeadYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(Trace.Last(), TargetYawDeg));
		AddInfo(FString::Printf(TEXT("TURNINPLACE %s: first turned frame %d (%.3f s), max rate %.1f deg/s, final body yaw %.2f, head yaw %.2f"),
			Name, FirstTurn, FirstTurn * StealthLocomotionTest::Step, MaxRate, Trace.Last(), FinalHeadYaw));
		TestTrue(FString::Printf(TEXT("%s: no body turn through frame 29 (0.483 s)"), Name), FirstTurn == INDEX_NONE || FirstTurn > 29);
		TestTrue(FString::Printf(TEXT("%s: turning begun by frame 31 (0.517 s)"), Name), FirstTurn != INDEX_NONE && FirstTurn <= 31);
		TestTrue(FString::Printf(TEXT("%s: turn rate %.1f <= 120 deg/s"), Name, MaxRate), MaxRate <= Tuning.PivotTurnRateDegPerSec + 0.05);
		TestTrue(FString::Printf(TEXT("%s: stops with head yaw <= 35 (%.2f)"), Name, FinalHeadYaw), FinalHeadYaw <= Tuning.TurnInPlaceExitFrac * Tuning.LookYawLimitDeg + 0.01 && FinalHeadYaw >= Tuning.TurnInPlaceExitFrac * Tuning.LookYawLimitDeg - 2.5);
		TestTrue(FString::Printf(TEXT("%s: the body stopped turning (last frames constant)"), Name), FMath::IsNearlyEqual(Trace.Last(), Trace[Trace.Num() - 10], 1e-3));
	}
	double MaxDiff = 0.0;
	for (int32 Index = 0; Index < Frames; ++Index)
	{
		MaxDiff = FMath::Max(MaxDiff, FMath::Abs(FMath::FindDeltaAngleDegrees(Yaws[0][Index], Yaws[1][Index])));
	}
	TestTrue(FString::Printf(TEXT("human and AI traces are identical (max difference %.4f deg)"), MaxDiff), MaxDiff <= 1e-3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
