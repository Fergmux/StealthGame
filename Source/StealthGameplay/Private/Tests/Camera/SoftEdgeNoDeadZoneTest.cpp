// Camera story cs-002 AC2 (production/epics/camera-system/story-002-view-envelope-pure-function.md; GDD camera-system.md
// AC-CS-04, F1): StealthGame.Camera.SoftEdgeNoDeadZone.
//   - the F1 gain at |v| = 60, 65 and 70 is 1.0, 0.5 and 0.1 (the floor);
//   - from a held 75 deg target, one frame of inward input moves the view inward on that same frame (no dead zone);
//   - at 180 deg/s raw outward input the view reaches 70 within 0.25 s of crossing 60.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CameraEnvelopeTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraSoftEdgeNoDeadZoneTest, "StealthGame.Camera.SoftEdgeNoDeadZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraSoftEdgeNoDeadZoneTest::RunTest(const FString& Parameters)
{
	using namespace StealthCameraEnvelopeTest;
	const FViewEnvelopeParams Params = DefaultParams();
	TestEqual(TEXT("gain at |v| = 60"), StealthViewEnvelope::SoftGain(60.0, Params.YawLimitDeg, Params.SoftYawDeg, Params.MinGain), 1.0, 1e-9);
	TestEqual(TEXT("gain at |v| = 65"), StealthViewEnvelope::SoftGain(65.0, Params.YawLimitDeg, Params.SoftYawDeg, Params.MinGain), 0.5, 1e-9);
	TestEqual(TEXT("gain at |v| = 70 (the floor)"), StealthViewEnvelope::SoftGain(70.0, Params.YawLimitDeg, Params.SoftYawDeg, Params.MinGain), 0.1, 1e-6);

	// Hold the target at 75, then one inward frame.
	FViewEnvelopeState State;
	for (int32 F = 0; F < FramesOf(2.0); ++F)
	{
		Frame(State, Params, 180.0);
	}
	TestEqual(TEXT("the target is held at 75"), State.TargetYawOffsetDeg, 75.0, 1e-6);
	const double Before = State.ViewYawOffsetDeg;
	Frame(State, Params, -30.0);
	TestTrue(*FString::Printf(TEXT("one inward frame moves the view inward on that frame (%.3f -> %.3f)"), Before, State.ViewYawOffsetDeg), State.ViewYawOffsetDeg < Before - 0.4);
	TestTrue(TEXT("the inward input pulled the target back to the view"), State.TargetYawOffsetDeg <= 70.0 + 1e-6);

	// 180 deg/s outward from rest: the time from crossing 60 to reaching 70.
	FViewEnvelopeState Sweep;
	double Crossed60 = -1.0;
	double Reached70 = -1.0;
	for (int32 F = 1; F <= FramesOf(2.0) && Reached70 < 0.0; ++F)
	{
		const double Was = Sweep.ViewYawOffsetDeg;
		Frame(Sweep, Params, 180.0);
		if (Crossed60 < 0.0 && Was < 60.0 && Sweep.ViewYawOffsetDeg >= 60.0)
		{
			Crossed60 = F * Dt;
		}
		if (Sweep.ViewYawOffsetDeg >= 70.0 - 1e-6)
		{
			Reached70 = F * Dt;
		}
	}
	TestTrue(*FString::Printf(TEXT("70 within 0.25 s of crossing 60 (crossed %.3f, reached %.3f)"), Crossed60, Reached70),
		Crossed60 >= 0.0 && Reached70 >= 0.0 && Reached70 - Crossed60 <= 0.25 + 1e-9);
	return true;
}

#endif
