// Camera story cs-012 AC3 (production/epics/camera-system/story-012-eye-mask-core.md; ADR-0015 sections 4.3.1-4; GDD F3):
// StealthGame.Camera.MaskConeAndBands.
//   - probes outside the +-55 x +-38 deg eye cone cost no trace and never count as visible (a target behind the eye, one at
//     60 deg off-axis, one 45 deg above the eye line); the cone is the fixed eye cone, not the FOV;
//   - every in-cone probe gets one trace and all probes of a target are submitted together, on the same frame;
//   - the test rate: 60 Hz within EyeMaskNearCm (20 m), 15 Hz to EyeMaskRangeCm (60 m), 5 Hz beyond, and a visible target
//     beyond 60 m still passes (rate bands, never a cutoff).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EyeMaskTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraMaskConeAndBandsTest, "StealthGame.Camera.MaskConeAndBands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraMaskConeAndBandsTest::RunTest(const FString& Parameters)
{
	using namespace StealthEyeMaskTest;
	const FStealthCameraParams P = Params();
	const FEyeMaskView View = ViewAlongX();

	// The cone (pure).
	{
		const FEyeMaskCore Cone(P);
		TestTrue(TEXT("dead ahead: in"), Cone.IsInCone(View, View.Origin + FVector(500.0, 0.0, 0.0)));
		TestTrue(TEXT("54 deg right: in"), Cone.IsInCone(View, View.Origin + FRotator(0.0, 54.0, 0.0).Vector() * 500.0));
		TestFalse(TEXT("56 deg right: out"), Cone.IsInCone(View, View.Origin + FRotator(0.0, 56.0, 0.0).Vector() * 500.0));
		TestTrue(TEXT("37 deg up: in"), Cone.IsInCone(View, View.Origin + FRotator(37.0, 0.0, 0.0).Vector() * 500.0));
		TestFalse(TEXT("39 deg up: out"), Cone.IsInCone(View, View.Origin + FRotator(39.0, 0.0, 0.0).Vector() * 500.0));
		TestFalse(TEXT("behind the eye: out"), Cone.IsInCone(View, View.Origin - FVector(500.0, 0.0, 0.0)));
	}

	// Out-of-cone targets: no trace, never visible; an in-cone target: all its probes submitted together.
	{
		FEyeMaskCore Mask(P);
		FDelayedBackend Backend;
		const int32 Behind = Mask.Register();
		const int32 Wide = Mask.Register();
		const int32 Ahead = Mask.Register();
		const FVector BehindFeet(-800.0, 0.0, 0.0);
		const FVector WideFeet = FRotator(0.0, 60.0, 0.0).Vector() * 800.0;
		const FVector AheadFeet(800.0, 0.0, 0.0);
		double Now = 0.0;
		for (int32 F = 0; F < 60; ++F)
		{
			Now += Dt;
			const int32 Before = Backend.Submitted;
			Mask.Step(Now, Dt, View, [&](int32 Id, FEyeMaskProbes& Out)
			{
				Out = CapsuleProbes(Id == Behind ? BehindFeet : (Id == Wide ? WideFeet : AheadFeet));
				return true;
			}, Backend);
			const int32 ThisFrame = Backend.Submitted - Before;
			TestTrue(*FString::Printf(TEXT("frame %d: traces only for the ahead target's in-cone probes, all together (0 or 5, got %d)"), F, ThisFrame), ThisFrame == 0 || ThisFrame == 5);
			++Backend.Frame;
		}
		TestEqual(TEXT("behind: never visible"), Mask.GetAlpha(Behind), 0.0f);
		TestEqual(TEXT("60 deg off-axis: never visible"), Mask.GetAlpha(Wide), 0.0f);
		TestEqual(TEXT("ahead and clear: visible"), Mask.GetAlpha(Ahead), 1.0f);
		TestTrue(TEXT("the out-of-cone targets were still tested (they fail without a trace)"), Mask.GetTestsStarted(Behind) > 0 && Mask.GetTestsStarted(Wide) > 0);
	}

	// Rate bands over 2 s: 60 Hz (10 m), 15 Hz (40 m), 5 Hz (80 m); the 80 m target is visible (no cutoff).
	{
		FEyeMaskCore Mask(P);
		FDelayedBackend Backend;
		const int32 Near = Mask.Register();
		const int32 Far = Mask.Register();
		const int32 Beyond = Mask.Register();
		double Now = 0.0;
		for (int32 F = 0; F < 120; ++F)
		{
			Now += Dt;
			Mask.Step(Now, Dt, View, [&](int32 Id, FEyeMaskProbes& Out)
			{
				Out = CapsuleProbes(FVector(Id == Near ? 1000.0 : (Id == Far ? 4000.0 : 8000.0), 0.0, 0.0));
				return true;
			}, Backend);
			++Backend.Frame;
		}
		// At 60 fps with results one frame late, the near target is retested every other frame at most (one test in flight).
		const int32 NearTests = Mask.GetTestsStarted(Near);
		const int32 FarTests = Mask.GetTestsStarted(Far);
		const int32 BeyondTests = Mask.GetTestsStarted(Beyond);
		TestTrue(*FString::Printf(TEXT("near (10 m): tested at the 60 Hz band (%d tests in 2 s, one in flight at a time)"), NearTests), NearTests >= 55 && NearTests <= 121);
		TestTrue(*FString::Printf(TEXT("far (40 m): 15 Hz (%d tests in 2 s)"), FarTests), FMath::Abs(FarTests - 30) <= 1);
		TestTrue(*FString::Printf(TEXT("beyond (80 m): 5 Hz (%d tests in 2 s)"), BeyondTests), FMath::Abs(BeyondTests - 10) <= 1);
		TestTrue(TEXT("beyond 60 m a clear target is still drawn (rate bands, never a cutoff)"), Mask.GetAlpha(Beyond) > 0.0f);
		TestTrue(TEXT("period at 10 m / 40 m / 80 m"), FMath::IsNearlyEqual(Mask.TestPeriodSec(1000.0), 1.0 / 60.0) && FMath::IsNearlyEqual(Mask.TestPeriodSec(4000.0), 1.0 / 15.0)
			&& FMath::IsNearlyEqual(Mask.TestPeriodSec(8000.0), 1.0 / 5.0));
	}
	return true;
}

#endif
