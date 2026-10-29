// Camera story cs-012 AC1 (production/epics/camera-system/story-012-eye-mask-core.md; GDD camera-system.md AC-CS-08, F3):
// StealthGame.Camera.MaskTiming. A near dummy (8 m) with async trace results landing one frame after the test:
//   - when its probe first passes, the fade-in starts on the frame the result lands and completes EyeMaskFadeSec 0.10 s later;
//   - when it fails, the fade-out starts after the 2nd consecutive failed result (EyeMaskHideFailCount 2): within 50 ms of the
//     first failing test at the 60 Hz near rate; never earlier than the 2nd failure.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EyeMaskTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraMaskTimingTest, "StealthGame.Camera.MaskTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraMaskTimingTest::RunTest(const FString& Parameters)
{
	using namespace StealthEyeMaskTest;
	const FStealthCameraParams P = Params();
	FEyeMaskCore Mask(P);
	FDelayedBackend Backend;
	bool bWall = true;
	Backend.Blocked = [&bWall](const FVector&, const FVector&) { return bWall; };
	const int32 Id = Mask.Register();
	const FVector Feet(800.0, 0.0, 0.0);
	auto Probes = [&Feet](int32, FEyeMaskProbes& Out) { Out = CapsuleProbes(Feet); return true; };
	const FEyeMaskView View = ViewAlongX();
	double Now = 0.0;
	auto Frame = [&]()
	{
		Now += Dt;
		Mask.Step(Now, Dt, View, Probes, Backend);
		++Backend.Frame;
	};

	for (int32 F = 0; F < 30; ++F)
	{
		Frame();
	}
	TestEqual(TEXT("behind the wall: hidden"), Mask.GetAlpha(Id), 0.0f);

	// The wall clears: the test submitted on frame N lands on N + 1, and the fade starts there.
	bWall = false;
	int32 FirstClearSubmitFrame = Backend.Frame;
	int32 FadeStartFrame = -1;
	int32 FadeEndFrame = -1;
	for (int32 F = 0; F < 30; ++F)
	{
		const int32 This = Backend.Frame;
		Frame();
		if (FadeStartFrame < 0 && Mask.GetAlpha(Id) > 0.0f)
		{
			FadeStartFrame = This;
		}
		if (FadeEndFrame < 0 && Mask.GetAlpha(Id) >= 1.0f)
		{
			FadeEndFrame = This;
		}
	}
	// The first clear submission happens on the first frame after the change (at most one frame of pending wall results).
	TestTrue(*FString::Printf(TEXT("the fade-in starts on the frame the passing result lands (frame %d, first clear test submitted on %d or %d)"), FadeStartFrame, FirstClearSubmitFrame, FirstClearSubmitFrame + 1),
		FadeStartFrame == FirstClearSubmitFrame + 1 || FadeStartFrame == FirstClearSubmitFrame + 2);
	TestTrue(*FString::Printf(TEXT("and completes 0.10 s later (%d frames)"), FadeEndFrame - FadeStartFrame), FMath::Abs((FadeEndFrame - FadeStartFrame) * Dt - (P.EyeMaskFadeSec - Dt)) <= Dt + 1e-9);

	// The wall returns: the fade-out starts only after the 2nd consecutive failed result.
	bWall = true;
	const int32 WallFrame = Backend.Frame;
	int32 FadeOutFrame = -1;
	for (int32 F = 0; F < 30 && FadeOutFrame < 0; ++F)
	{
		const int32 This = Backend.Frame;
		const float Before = Mask.GetAlpha(Id);
		Frame();
		if (Mask.GetAlpha(Id) < Before)
		{
			FadeOutFrame = This;
		}
	}
	const double Delay = (FadeOutFrame - WallFrame) * Dt;
	TestTrue(*FString::Printf(TEXT("the fade-out starts after the 2nd failed result: %.3f s after the wall (<= 50 ms, not before 2 results)"), Delay),
		FadeOutFrame >= WallFrame + 2 && Delay <= 0.050 + 1e-9);
	return true;
}

#endif
