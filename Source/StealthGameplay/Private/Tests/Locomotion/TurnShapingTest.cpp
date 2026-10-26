// Story SCL-012 (production/epics/shared-character-locomotion/story-012-turn-shaping.md), AC-SCL-07 TurnShaping: GIVEN a
// walking pawn, WHEN input turns 90 degrees, THEN heading rate never exceeds 60 deg/s and a circle fit over a 1 s arc
// gives r >= 134 cm - 5 % (>= 107 cm - 5 % at 112 cm/s); running, r >= 143 cm - 5 % (fit over the run turn, which lasts
// about 0.6 s at 180 deg/s); WHEN input reverses 119 degrees THEN no pivot occurs; at 121 and 150 degrees THEN the pawn
// stops, pivots at <= 120 deg/s and restarts, and no 0.5 s window contains a heading change > 120 degrees; WHEN the
// input oscillates 115-125 degrees at 10 Hz THEN the pawn enters the stop-pivot at most once (no arc/pivot flip-flop).
// Driven through the Move entry point in a test world (the same function runs for the owner path: SCL-009 parity).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Locomotion/LocomotionTestWorld.h"
#include "View/Core/StealthLocomotionModel.h"

namespace TurnShapingTestData
{
	constexpr int32 SettleFrames = 150;
	constexpr double RateEpsilonDeg = 0.05;

	/** Circumradius of three 2D points. */
	double CircleRadius(const FVector& A, const FVector& B, const FVector& C)
	{
		const double AB = FVector::Dist2D(A, B);
		const double BC = FVector::Dist2D(B, C);
		const double CA = FVector::Dist2D(C, A);
		const double Cross = FMath::Abs((B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X));
		return Cross > UE_DOUBLE_SMALL_NUMBER ? AB * BC * CA / (2.0 * Cross) : TNumericLimits<double>::Max();
	}

	/** Walks Pawn at Magnitude/Gait heading 0 to steady speed. */
	void Settle(AStealthPawn* Pawn, float Magnitude, EGait Gait)
	{
		StealthLocomotionTest::DriveMove(Pawn, 0.0f, Magnitude, Gait);
		for (int32 Frame = 0; Frame < SettleFrames; ++Frame)
		{
			StealthLocomotionTest::StepPawn(Pawn);
		}
	}

	struct FTurnTrace
	{
		TArray<FVector> Positions;
		TArray<double> Yaws;
		TArray<double> Speeds;
		TArray<bool> InPivot;
	};

	FTurnTrace Record(AStealthPawn* Pawn, int32 Frames, TFunctionRef<float(int32)> InputYawAt, float Magnitude, EGait Gait)
	{
		FTurnTrace Trace;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			StealthLocomotionTest::DriveMove(Pawn, InputYawAt(Frame), Magnitude, Gait);
			StealthLocomotionTest::StepPawn(Pawn);
			Trace.Positions.Add(Pawn->GetActorLocation());
			Trace.Yaws.Add(Pawn->GetActorRotation().Yaw);
			Trace.Speeds.Add(StealthLocomotionTest::Speed2D(Pawn));
			const FStealthMotionState& State = Pawn->GetStealthMovement()->GetMotionState();
			Trace.InPivot.Add(State.bReversalLatched || State.bPivoting);
		}
		return Trace;
	}

	double MaxRateDegPerSec(const FTurnTrace& Trace, double StartYaw)
	{
		double Max = 0.0;
		double Previous = StartYaw;
		for (double Yaw : Trace.Yaws)
		{
			Max = FMath::Max(Max, FMath::Abs(FMath::FindDeltaAngleDegrees(Previous, Yaw)) / StealthLocomotionTest::Step);
			Previous = Yaw;
		}
		return Max;
	}

	/** Largest heading change inside any 0.5 s (30-frame) window. */
	double MaxHalfSecondChange(const FTurnTrace& Trace, double StartYaw)
	{
		TArray<double> Unwrapped;
		double Accum = StartYaw;
		double Previous = StartYaw;
		Unwrapped.Add(StartYaw);
		for (double Yaw : Trace.Yaws)
		{
			Accum += FMath::FindDeltaAngleDegrees(Previous, Yaw);
			Previous = Yaw;
			Unwrapped.Add(Accum);
		}
		double Max = 0.0;
		for (int32 I = 0; I < Unwrapped.Num(); ++I)
		{
			for (int32 J = I + 1; J < Unwrapped.Num() && J <= I + 30; ++J)
			{
				Max = FMath::Max(Max, FMath::Abs(Unwrapped[J] - Unwrapped[I]));
			}
		}
		return Max;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionTurnShapingTest, "StealthGame.Locomotion.TurnShaping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionTurnShapingTest::RunTest(const FString& Parameters)
{
	using namespace TurnShapingTestData;
	const FLocomotionTuning_Public Tuning;
	StealthLocomotionTest::FWorld TestWorld(TEXT("TurnShapingTestWorld"));
	int32 Lane = 0;
	auto NextPawn = [&TestWorld, &Lane]() { return TestWorld.SpawnPawn(FVector2D(-8000.0, -8000.0 + 1500.0 * Lane++)); };

	// 90 degree turns: walk at 140 and 112 (1 s arc fit), run at 450 (fit over the run turn).
	struct FArcCase { const TCHAR* Name; float Magnitude; EGait Gait; double MinRadius; double RateCap; int32 FitStart; int32 FitFrames; };
	const FArcCase Arcs[] = {
		{ TEXT("walk 140"), 1.0f, EGait::Walk, 134.0 * 0.95, 60.0, 6, 60 },
		{ TEXT("walk 112"), 0.8f, EGait::Walk, 107.0 * 0.95, 60.0, 6, 60 },
		{ TEXT("run 450"), 1.0f, EGait::Run, 143.0 * 0.95, 180.0, 2, 26 },
	};
	for (const FArcCase& Arc : Arcs)
	{
		AStealthPawn* Pawn = NextPawn();
		Settle(Pawn, Arc.Magnitude, Arc.Gait);
		const double StartYaw = Pawn->GetActorRotation().Yaw;
		const FTurnTrace Trace = Record(Pawn, 120, [](int32) { return 90.0f; }, Arc.Magnitude, Arc.Gait);
		const double Rate = MaxRateDegPerSec(Trace, StartYaw);
		const double Radius = CircleRadius(Trace.Positions[Arc.FitStart], Trace.Positions[Arc.FitStart + Arc.FitFrames / 2], Trace.Positions[Arc.FitStart + Arc.FitFrames]);
		const bool bPivoted = Trace.InPivot.Contains(true);
		AddInfo(FString::Printf(TEXT("TURNSHAPING %s: max heading rate %.2f deg/s, circle fit r %.1f cm, speed %.1f, pivot %d"), Arc.Name, Rate, Radius, Trace.Speeds[Arc.FitStart + Arc.FitFrames / 2], bPivoted ? 1 : 0));
		TestTrue(FString::Printf(TEXT("%s: heading rate %.2f <= %.0f deg/s"), Arc.Name, Rate, Arc.RateCap), Rate <= Arc.RateCap + RateEpsilonDeg);
		TestTrue(FString::Printf(TEXT("%s: radius %.1f >= %.1f cm"), Arc.Name, Radius, Arc.MinRadius), Radius >= Arc.MinRadius);
		TestFalse(FString::Printf(TEXT("%s: a 90 degree turn is an arc, never a pivot"), Arc.Name), bPivoted);
	}

	// Reversals at walk: 119 (arc), 121 and 150 (stop, pivot, restart).
	struct FReversalCase { float AngleDeg; bool bExpectPivot; };
	const FReversalCase Reversals[] = { { 119.0f, false }, { 121.0f, true }, { 150.0f, true } };
	for (const FReversalCase& Case : Reversals)
	{
		AStealthPawn* Pawn = NextPawn();
		Settle(Pawn, 1.0f, EGait::Walk);
		const double StartYaw = Pawn->GetActorRotation().Yaw;
		const float Target = Case.AngleDeg;
		const FTurnTrace Trace = Record(Pawn, 300, [Target](int32) { return Target; }, 1.0f, EGait::Walk);
		const bool bPivoted = Trace.InPivot.Contains(true);
		double MinSpeed = TNumericLimits<double>::Max();
		for (double S : Trace.Speeds) { MinSpeed = FMath::Min(MinSpeed, S); }
		const double Rate = MaxRateDegPerSec(Trace, StartYaw);
		const double Window = MaxHalfSecondChange(Trace, StartYaw);
		AddInfo(FString::Printf(TEXT("TURNSHAPING reversal %.0f: pivot %d, min speed %.1f, max rate %.1f deg/s, max 0.5 s change %.1f deg, final speed %.1f, final yaw %.1f"),
			Case.AngleDeg, bPivoted ? 1 : 0, MinSpeed, Rate, Window, Trace.Speeds.Last(), Trace.Yaws.Last()));
		TestEqual(FString::Printf(TEXT("reversal %.0f: pivot %s"), Case.AngleDeg, Case.bExpectPivot ? TEXT("occurs") : TEXT("does not occur")), bPivoted, Case.bExpectPivot);
		TestTrue(FString::Printf(TEXT("reversal %.0f: no 0.5 s window turns more than 120 degrees (%.1f)"), Case.AngleDeg, Window), Window <= 120.0 + RateEpsilonDeg);
		if (Case.bExpectPivot)
		{
			TestTrue(FString::Printf(TEXT("reversal %.0f: the pawn stops before pivoting (min speed %.1f <= PivotSpeed)"), Case.AngleDeg, MinSpeed), MinSpeed <= Tuning.PivotSpeedCmPerSec);
			TestTrue(FString::Printf(TEXT("reversal %.0f: pivot rate %.1f <= 120 deg/s"), Case.AngleDeg, Rate), Rate <= Tuning.PivotTurnRateDegPerSec + RateEpsilonDeg);
			TestTrue(FString::Printf(TEXT("reversal %.0f: restarts and walks on (final speed %.1f)"), Case.AngleDeg, Trace.Speeds.Last()), Trace.Speeds.Last() >= 139.0);
		}
		else
		{
			TestTrue(TEXT("reversal 119: never stops (arc)"), MinSpeed > Tuning.PivotSpeedCmPerSec);
		}
	}

	// 10 Hz oscillation between 115 and 125 degrees: at most one entry into the stop-pivot.
	{
		AStealthPawn* Pawn = NextPawn();
		Settle(Pawn, 1.0f, EGait::Walk);
		const FTurnTrace Trace = Record(Pawn, 300, [](int32 Frame) { return (Frame / 3) % 2 == 0 ? 115.0f : 125.0f; }, 1.0f, EGait::Walk);
		int32 Entries = 0;
		for (int32 Index = 1; Index < Trace.InPivot.Num(); ++Index)
		{
			Entries += (!Trace.InPivot[Index - 1] && Trace.InPivot[Index]) ? 1 : 0;
		}
		Entries += Trace.InPivot.Num() > 0 && Trace.InPivot[0] ? 1 : 0;
		AddInfo(FString::Printf(TEXT("TURNSHAPING oscillation 115-125 at 10 Hz: %d stop-pivot entries"), Entries));
		TestTrue(FString::Printf(TEXT("oscillation: at most one stop-pivot entry (%d)"), Entries), Entries <= 1);
	}

	// Model: the reversal latch is released only by the pivot exit, and an exact 180 degree tie turns clockwise first.
	{
		FStealthMotionState State;
		State.Speed = 140.0f;
		FStealthMoveInput Input;
		Input.Magnitude = 1.0f;
		Input.InputYawDeg = 180.0f;
		FStealthLocomotionModel::Step(Tuning, Input, StealthLocomotionTest::Step, State);
		TestTrue(TEXT("model: 180 degrees at walk latches the reversal"), State.bReversalLatched);
		Input.InputYawDeg = 100.0f;
		FStealthLocomotionModel::Step(Tuning, Input, StealthLocomotionTest::Step, State);
		TestTrue(TEXT("model: the latch holds when the input swings back below the threshold"), State.bReversalLatched);
		FStealthMotionState Idle;
		FStealthMoveInput Tie;
		Tie.Magnitude = 1.0f;
		Tie.InputYawDeg = 180.0f;
		FStealthLocomotionModel::Step(Tuning, Tie, StealthLocomotionTest::Step, Idle);
		TestTrue(TEXT("model: an idle 180 degree start pivots clockwise (positive yaw) first"), Idle.bPivoting && Idle.FacingYawDeg > 0.0f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
