// Camera story cs-006 AC2 (production/epics/camera-system/story-006-eye-pivot-boom-collision.md; GDD camera-system.md
// AC-CS-29, CS-R4): StealthGame.Camera.PivotCapAndCorrection.
//   - a 150 cm drop and a 120 cm climb of the body: the pivot's vertical speed never exceeds PivotMaxVertCmPerSec 300 and the
//     mask origin is never above the real eye socket (min(lagged pivot, eye socket));
//   - an injected owner correction below SCL's PawnSmoothSnapThresholdCm moves the pivot smoothly (no step > 5 cm in a frame);
//     one above it snaps the pivot with the body on the same frame.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CameraCoreTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraPivotCapAndCorrectionTest, "StealthGame.Camera.PivotCapAndCorrection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraPivotCapAndCorrectionTest::RunTest(const FString& Parameters)
{
	using namespace StealthCameraCoreTest;
	const FStealthCameraParams P = Params();
	FScriptedTraces Open;

	// Drop 150 cm and climb 120 cm (the body moves at 600 cm/s, faster than the cap).
	for (const double Delta : { -150.0, 120.0 })
	{
		FStealthCameraCore Core(P);
		FVector Centre(0.0, 0.0, 300.0);
		FStealthCameraOutput Out;
		for (int32 F = 0; F < 30; ++F)
		{
			Out = Core.Step(Body(Centre), Open);
		}
		double PreviousZ = Out.Pivot.Z;
		double MaxRate = 0.0;
		bool bMaskAboveEye = false;
		const double Target = Centre.Z + Delta;
		for (int32 F = 0; F < 120; ++F)
		{
			Centre.Z = Delta < 0.0 ? FMath::Max(Target, Centre.Z - 600.0 * Dt) : FMath::Min(Target, Centre.Z + 600.0 * Dt);
			const FStealthCameraBodyInput In = Body(Centre);
			Out = Core.Step(In, Open);
			MaxRate = FMath::Max(MaxRate, FMath::Abs(Out.Pivot.Z - PreviousZ) / Dt);
			bMaskAboveEye |= Out.MaskOrigin.Z > In.EyeSocketZ + 1e-6;
			PreviousZ = Out.Pivot.Z;
		}
		TestTrue(*FString::Printf(TEXT("%+.0f cm: pivot vertical speed <= 300 cm/s (max %.1f)"), Delta, MaxRate), MaxRate <= 300.0 + 1e-6);
		TestFalse(*FString::Printf(TEXT("%+.0f cm: the mask origin is never above the real eye socket"), Delta), bMaskAboveEye);
		TestEqual(*FString::Printf(TEXT("%+.0f cm: the pivot settles at the eye"), Delta), Out.Pivot.Z, Centre.Z + EyeAboveCentreCm, 0.5);
	}

	// Owner corrections.
	const double Threshold = P.Consumed.PawnSmoothSnapThresholdCm;
	for (const double Size : { Threshold * 0.5, Threshold * 1.5 })
	{
		FStealthCameraCore Core(P);
		FVector Centre(0.0, 0.0, 92.0);
		FStealthCameraOutput Out;
		for (int32 F = 0; F < 30; ++F)
		{
			Out = Core.Step(Body(Centre), Open);
		}
		const FVector Before = Out.Pivot;
		const FVector Delta(Size, 0.0, 0.0);
		Centre += Delta;
		FStealthCameraBodyInput In = Body(Centre);
		In.bCorrection = true;
		In.CorrectionDelta = Delta;
		Out = Core.Step(In, Open);
		if (Size < Threshold)
		{
			double Worst = FVector::Dist(Out.Pivot, Before);
			FVector Last = Out.Pivot;
			for (int32 F = 0; F < 120; ++F)
			{
				Out = Core.Step(Body(Centre), Open);
				Worst = FMath::Max(Worst, FVector::Dist(Out.Pivot, Last));
				Last = Out.Pivot;
			}
			TestTrue(*FString::Printf(TEXT("correction %.0f cm (below %.0f): no pivot step > 5 cm (max %.2f)"), Size, Threshold, Worst), Worst <= 5.0 + 1e-6);
			TestEqual(TEXT("... and the pivot reaches the body"), Out.Pivot.X, Centre.X, 0.5);
		}
		else
		{
			TestEqual(*FString::Printf(TEXT("correction %.0f cm (above %.0f): the pivot snaps with the body on the same frame"), Size, Threshold), Out.Pivot.X, Centre.X, 1e-6);
		}
	}
	return true;
}

#endif
