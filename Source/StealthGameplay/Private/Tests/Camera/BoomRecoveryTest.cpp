// Camera story cs-006 AC1 (production/epics/camera-system/story-006-eye-pivot-boom-collision.md; GDD camera-system.md
// AC-CS-06, F2): StealthGame.Camera.BoomRecovery. With the default keys the free length is about 186 cm; an obstacle drops it
// to 52 cm and is then removed: the lens is at 52 cm on the obstacle's first frame, holds for BoomRecoverHoldSec 0.15 s
// after the removal, reaches >= 179.3 cm (95 % of the gap back to 186) by 1.15 s, and never moves outward faster than
// BoomRecoverCmPerSec 300. Under Reduce camera motion the recovery is a cut. The mask-off band and close 3P follow the free
// length (off below 25 cm, on again above 35 cm; close below 60 cm).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CameraCoreTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraBoomRecoveryTest, "StealthGame.Camera.BoomRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraBoomRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace StealthCameraCoreTest;
	const FStealthCameraParams P = Params();
	FStealthCameraCore Core(P);
	FScriptedTraces Traces;
	FStealthCameraOutput Out;
	for (int32 F = 0; F < 30; ++F)
	{
		Out = Core.Step(Body(), Traces);
	}
	const double Free = Out.FreeBoomCm;
	TestTrue(*FString::Printf(TEXT("the open free length is ~186 cm (%.1f)"), Free), FMath::IsNearlyEqual(Free, FMath::Sqrt(180.0 * 180.0 + 45.0 * 45.0 + 15.0 * 15.0), 0.5));
	Traces.BoomFreeCm = 52.0;
	Out = Core.Step(Body(), Traces);
	TestEqual(TEXT("pull-in in the same frame: 52 cm"), Out.FreeBoomCm, 52.0, 1e-6);
	TestTrue(TEXT("52 cm is close 3P"), Out.bCloseThirdPerson);
	for (int32 F = 0; F < 10; ++F)
	{
		Out = Core.Step(Body(), Traces);
	}
	Traces.BoomFreeCm = 1.0e6; // the obstacle is removed
	double Previous = Out.FreeBoomCm;
	double HoldEnd = -1.0;
	double MaxRate = 0.0;
	double At115 = -1.0;
	for (int32 F = 1; F <= 120; ++F)
	{
		Out = Core.Step(Body(), Traces);
		const double T = F * Dt;
		if (HoldEnd < 0.0 && Out.FreeBoomCm > Previous + 1e-9)
		{
			HoldEnd = (F - 1) * Dt;
		}
		MaxRate = FMath::Max(MaxRate, (Out.FreeBoomCm - Previous) / Dt);
		Previous = Out.FreeBoomCm;
		if (At115 < 0.0 && T >= 1.15 - 1e-9)
		{
			At115 = Out.FreeBoomCm;
		}
	}
	TestTrue(*FString::Printf(TEXT("the lens holds 0.15 s after the removal (first move after %.3f s)"), HoldEnd), HoldEnd >= 0.15 - Dt - 1e-9 && HoldEnd <= 0.15 + Dt);
	TestTrue(*FString::Printf(TEXT(">= 179.3 cm by 1.15 s (%.2f)"), At115), At115 >= 52.0 + 0.95 * (Free - 52.0) - 1e-6);
	TestTrue(*FString::Printf(TEXT("never faster than 300 cm/s (%.1f)"), MaxRate), MaxRate <= 300.0 + 1e-6);

	// Reduce camera motion: the recovery is a cut.
	{
		FStealthCameraCore Cut(P);
		FScriptedTraces Open;
		FStealthCameraBodyInput In = Body();
		In.bReduceMotion = true;
		Open.BoomFreeCm = 52.0;
		Cut.Step(In, Open);
		Open.BoomFreeCm = 1.0e6;
		const FStealthCameraOutput Back = Cut.Step(In, Open);
		TestEqual(TEXT("Reduce camera motion: back to the full length on the next frame"), Back.FreeBoomCm, Free, 0.5);
	}

	// The mask-off band: off below 25 cm, on again only above 35 cm.
	{
		FStealthCameraCore Band(P);
		FScriptedTraces Wall;
		FStealthCameraBodyInput In = Body();
		In.bReduceMotion = true; // cuts, so each step lands on the scripted length
		Wall.BoomFreeCm = 30.0;
		TestFalse(TEXT("30 cm (from open): mask on"), Band.Step(In, Wall).bMaskOff);
		Wall.BoomFreeCm = 21.0;
		TestTrue(TEXT("21 cm (flush against a wall): mask off"), Band.Step(In, Wall).bMaskOff);
		Wall.BoomFreeCm = 30.0;
		TestTrue(TEXT("30 cm after being off: still off (hysteresis)"), Band.Step(In, Wall).bMaskOff);
		Wall.BoomFreeCm = 36.0;
		TestFalse(TEXT("36 cm: on again"), Band.Step(In, Wall).bMaskOff);
	}
	return true;
}

#endif
