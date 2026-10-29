// Camera story cs-002 AC1 (production/epics/camera-system/story-002-view-envelope-pure-function.md; GDD camera-system.md
// AC-CS-03 numeric bullets, F1): StealthGame.Camera.EnvelopeClamp. Body facing 0, fixed dt 1/60 s, 30 frames to converge:
//   - outward yaw input at 90 deg/s for 2 s: the view yaw stays <= 70; the target stays <= 75 and exceeds 70 while the input
//     continues;
//   - pitch input at 60 deg/s for 2 s down and 2 s up: the view pitch stays within -60 and +40 (and reaches both);
//   - input stopped before 0.5 s past the limit: the target is back at 70 within T_latch 0.15 s + 0.09 s;
//   - while own IsConcealed the overshoot is 0 (target <= 70); the CS-R3.5 head-lead clamp is a no-op while disabled.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CameraEnvelopeTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraEnvelopeClampTest, "StealthGame.Camera.EnvelopeClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraEnvelopeClampTest::RunTest(const FString& Parameters)
{
	using namespace StealthCameraEnvelopeTest;
	const FViewEnvelopeParams Params = DefaultParams();
	constexpr double Eps = 1e-6;

	// Yaw: 90 deg/s outward for 2 s.
	FViewEnvelopeState State;
	for (int32 F = 0; F < 30; ++F)
	{
		Frame(State, Params, 0.0);
	}
	double MaxView = 0.0;
	double MaxTarget = 0.0;
	bool bTargetAbove70WhileInput = false;
	int32 FramesAtLimit = 0;
	for (int32 F = 0; F < FramesOf(2.0); ++F)
	{
		Frame(State, Params, 90.0);
		MaxView = FMath::Max(MaxView, State.ViewYawOffsetDeg);
		MaxTarget = FMath::Max(MaxTarget, State.TargetYawOffsetDeg);
		if (State.ViewYawOffsetDeg >= 70.0 - Eps)
		{
			++FramesAtLimit;
			bTargetAbove70WhileInput |= State.TargetYawOffsetDeg > 70.0 + Eps;
		}
	}
	TestTrue(TEXT("the view reached the 70 deg limit"), FramesAtLimit > 0);
	TestTrue(TEXT("view yaw <= 70.0"), MaxView <= 70.0 + Eps);
	TestTrue(TEXT("target yaw <= 75.0"), MaxTarget <= 75.0 + Eps);
	TestTrue(TEXT("the target exceeds 70 while the outward input continues"), bTargetAbove70WhileInput);
	TestEqual(TEXT("the target holds at 75 under continued input"), State.TargetYawOffsetDeg, 75.0, 1e-6);

	// Stop the input: the target relaxes to 70 within 0.15 s (latch) + 0.09 s.
	int32 RelaxFrames = 0;
	while (State.TargetYawOffsetDeg > 70.0 + Eps && RelaxFrames < 600)
	{
		Frame(State, Params, 0.0);
		++RelaxFrames;
	}
	TestTrue(*FString::Printf(TEXT("the target relaxes to 70 within 0.24 s (took %.3f s)"), RelaxFrames * Dt), RelaxFrames * Dt <= 0.15 + 0.09 + Dt);
	TestEqual(TEXT("the view stays at the limit (the body pivot is SCL's)"), State.ViewYawOffsetDeg, 70.0, 1e-6);

	// Pitch: 60 deg/s down for 2 s, then up for 2 s.
	double MinPitch = 0.0;
	double MaxPitch = 0.0;
	for (int32 F = 0; F < FramesOf(2.0); ++F)
	{
		Frame(State, Params, 0.0, -60.0);
		MinPitch = FMath::Min(MinPitch, State.ViewPitchDeg);
	}
	for (int32 F = 0; F < FramesOf(2.0); ++F)
	{
		Frame(State, Params, 0.0, 60.0);
		MinPitch = FMath::Min(MinPitch, State.ViewPitchDeg);
		MaxPitch = FMath::Max(MaxPitch, State.ViewPitchDeg);
	}
	TestTrue(*FString::Printf(TEXT("view pitch within -60.0 and +40.0 (%.3f .. %.3f)"), MinPitch, MaxPitch), MinPitch >= -60.0 - Eps && MaxPitch <= 40.0 + Eps);
	TestTrue(TEXT("both pitch limits are reached"), MinPitch <= -60.0 + 0.01 && MaxPitch >= 40.0 - 0.01);

	// Concealed: no overshoot.
	FViewEnvelopeState Hidden;
	for (int32 F = 0; F < FramesOf(2.0); ++F)
	{
		Frame(Hidden, Params, 90.0, 0.0, /*bConcealed*/ true);
	}
	TestEqual(TEXT("own IsConcealed: E_eff 0, the target stays at 70"), Hidden.TargetYawOffsetDeg, 70.0, 1e-6);

	// CS-R3.5 head-lead clamp: a no-op while disabled; with it enabled the view leads the head by at most the key.
	FViewEnvelopeState Free;
	FViewEnvelopeState Clamped;
	FViewEnvelopeParams Enabled = Params;
	Enabled.HeadLeadMaxDeg = 35.0;
	for (int32 F = 0; F < FramesOf(1.0); ++F)
	{
		FViewEnvelopeInput In;
		In.DeltaYawDeg = 90.0 * Dt;
		In.DtSec = Dt;
		In.HeadYawOffsetDeg = 0.0; // the head never turns in this fixture
		StealthViewEnvelope::Step(Free, In, Params);
		StealthViewEnvelope::Step(Clamped, In, Enabled);
	}
	TestTrue(TEXT("disabled (the default sentinel): no clamp (the view went past 35)"), Free.ViewYawOffsetDeg > 36.0);
	TestTrue(TEXT("enabled: the view leads the head by at most 35"), Clamped.ViewYawOffsetDeg <= 35.0 + Eps);
	return true;
}

#endif
