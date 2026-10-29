// Camera story cs-006 AC3 (production/epics/camera-system/story-006-eye-pivot-boom-collision.md; GDD camera-system.md
// AC-CS-30, F2): StealthGame.Camera.SqueezeAndSwap.
//   - a wall 30 cm from the right shoulder: the offset reaches 0 within ShoulderSqueezeInSec 0.15 s and returns within
//     ShoulderSqueezeOutSec 0.40 s after clearing, with no automatic flip to the left shoulder;
//   - a shoulder swap mirrors the offset (+45 -> -45) over ShoulderSwapBlendSec 0.25 s; a second press during the blend is
//     buffered and runs once (back to +45 after the first blend), a third press during the blend is not queued twice; the
//     2 cm guard ray stays clear on every frame of the blend; under Reduce camera motion the swap is a cut.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CameraCoreTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraSqueezeAndSwapTest, "StealthGame.Camera.SqueezeAndSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraSqueezeAndSwapTest::RunTest(const FString& Parameters)
{
	using namespace StealthCameraCoreTest;
	const FStealthCameraParams P = Params();

	// Squeeze.
	{
		FStealthCameraCore Core(P);
		FScriptedTraces Traces;
		FStealthCameraOutput Out;
		for (int32 F = 0; F < 30; ++F)
		{
			Out = Core.Step(Body(), Traces);
		}
		TestEqual(TEXT("open: +45 cm offset (right shoulder)"), Out.ShoulderOffsetCm, 45.0, 1e-6);
		Traces.bShoulderBlocked = true;
		int32 Frames = 0;
		while (Out.ShoulderOffsetCm > 1e-9 && Frames < 120)
		{
			Out = Core.Step(Body(), Traces);
			++Frames;
			TestTrue(TEXT("no automatic flip (the offset never goes left)"), Out.ShoulderOffsetCm >= -1e-9);
		}
		TestTrue(*FString::Printf(TEXT("the offset reaches 0 within 0.15 s (%.3f)"), Frames * Dt), Frames * Dt <= 0.15 + Dt);
		TestEqual(TEXT("the side is still right"), Core.GetShoulderSide(), 1);
		Traces.bShoulderBlocked = false;
		Frames = 0;
		while (Out.ShoulderOffsetCm < 45.0 - 1e-9 && Frames < 120)
		{
			Out = Core.Step(Body(), Traces);
			++Frames;
		}
		TestTrue(*FString::Printf(TEXT("the offset returns within 0.40 s after clearing (%.3f)"), Frames * Dt), Frames * Dt <= 0.40 + Dt);
	}

	// Swap.
	{
		FStealthCameraCore Core(P);
		FScriptedTraces Traces;
		FStealthCameraOutput Out;
		for (int32 F = 0; F < 30; ++F)
		{
			Out = Core.Step(Body(), Traces);
		}
		Core.RequestShoulderSwap();
		bool bGuardAlwaysClear = true;
		int32 Frames = 0;
		bool bPressedAgain = false;
		while (Core.IsSwapBlending() && Frames < 120)
		{
			Out = Core.Step(Body(), Traces);
			bGuardAlwaysClear &= Out.bGuardClear;
			++Frames;
			if (Frames == 5 && !bPressedAgain)
			{
				bPressedAgain = true;
				Core.RequestShoulderSwap(); // buffered
				Core.RequestShoulderSwap(); // a third press: still one buffered swap
			}
		}
		TestTrue(*FString::Printf(TEXT("the swap blends over 0.25 s (%.3f)"), Frames * Dt), FMath::Abs(Frames * Dt - 0.25) <= Dt + 1e-9);
		TestEqual(TEXT("mirrored: -45 cm at the end of the blend"), Out.ShoulderOffsetCm, -45.0, 1e-6);
		TestTrue(TEXT("the second press is buffered"), Core.HasBufferedSwap());
		Out = Core.Step(Body(), Traces);
		TestTrue(TEXT("the buffered press runs once when the blend ends"), Core.IsSwapBlending() && !Core.HasBufferedSwap() && Core.GetShoulderSide() == 1);
		while (Core.IsSwapBlending() && Frames < 240)
		{
			Out = Core.Step(Body(), Traces);
			bGuardAlwaysClear &= Out.bGuardClear;
			++Frames;
		}
		Out = Core.Step(Body(), Traces);
		TestEqual(TEXT("back to +45 cm, and no third swap"), Out.ShoulderOffsetCm, 45.0, 1e-6);
		TestFalse(TEXT("nothing more buffered"), Core.HasBufferedSwap() || Core.IsSwapBlending());
		TestTrue(TEXT("the guard ray stays clear on every frame of the blends"), bGuardAlwaysClear && Traces.RayCalls > 0);
	}

	// Reduce camera motion: the swap is a cut.
	{
		FStealthCameraCore Core(P);
		FScriptedTraces Traces;
		FStealthCameraBodyInput In = Body();
		In.bReduceMotion = true;
		Core.Step(In, Traces);
		Core.RequestShoulderSwap();
		const FStealthCameraOutput Out = Core.Step(In, Traces);
		TestEqual(TEXT("Reduce camera motion: the swap lands on the next frame"), Out.ShoulderOffsetCm, -45.0, 1e-6);
	}
	return true;
}

#endif
