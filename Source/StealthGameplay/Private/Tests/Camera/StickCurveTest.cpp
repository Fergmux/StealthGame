// Camera story cs-003 AC1 (production/epics/camera-system/story-003-stick-look-response-no-aim-assist.md; GDD camera-system.md
// AC-CS-15, F6): StealthGame.Camera.StickCurve. With the defaults (dead zone 0.15, outer 0.98, exponent 1.5, 180 / 120 deg/s):
// pure-right deflection 0.565, 0.10 and 1.0 give 63.6 +- 0.5, 0 and 180 deg/s yaw; a diagonal of magnitude 0.565 gives
// 45.0 +- 0.5 deg/s yaw and 30.0 +- 0.5 deg/s pitch (the scaled radial dead zone keeps the direction). The mouse is raw delta
// times sensitivity (no smoothing, no acceleration: the same delta every frame gives the same rate).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/Core/StealthLookResponse.h"
#include "View/Tuning/StealthCameraParams.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraStickCurveTest, "StealthGame.Camera.StickCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraStickCurveTest::RunTest(const FString& Parameters)
{
	const FStealthCameraUserSettings Settings;
	const double Outer = FStealthCameraParams::Defaults().StickOuterZoneFrac;
	TestEqual(TEXT("pure right 0.565: 63.6 deg/s yaw"), StealthLookResponse::StickRates(FVector2D(0.565, 0.0), Settings, Outer).X, 63.6, 0.5);
	TestEqual(TEXT("pure right 0.10 (inside the dead zone): 0"), StealthLookResponse::StickRates(FVector2D(0.10, 0.0), Settings, Outer).X, 0.0, 1e-9);
	TestEqual(TEXT("pure right 1.0: 180 deg/s"), StealthLookResponse::StickRates(FVector2D(1.0, 0.0), Settings, Outer).X, 180.0, 1e-9);
	const double Axis = 0.565 / FMath::Sqrt(2.0);
	const FVector2D Diagonal = StealthLookResponse::StickRates(FVector2D(Axis, Axis), Settings, Outer);
	TestEqual(TEXT("diagonal 0.565: 45.0 deg/s yaw"), Diagonal.X, 45.0, 0.5);
	TestEqual(TEXT("diagonal 0.565: 30.0 deg/s pitch"), Diagonal.Y, 30.0, 0.5);
	TestEqual(TEXT("pure left 1.0: -180 deg/s (sign kept)"), StealthLookResponse::StickRates(FVector2D(-1.0, 0.0), Settings, Outer).X, -180.0, 1e-9);

	// Mouse: raw delta times sensitivity, frame after frame (no smoothing, no acceleration ramp).
	FLookFrameInput Mouse;
	Mouse.bStick = false;
	Mouse.MouseDeltaDeg = FVector2D(1.5, -0.5);
	Mouse.Sensitivity.Yaw = 2.0;
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		const FVector2D Delta = StealthLookResponse::FrameDeltaDeg(Mouse, Settings, Outer);
		TestTrue(*FString::Printf(TEXT("mouse frame %d: delta x sensitivity, unsmoothed"), Frame), Delta.Equals(FVector2D(3.0, -0.5), 1e-9));
	}
	return true;
}

#endif
