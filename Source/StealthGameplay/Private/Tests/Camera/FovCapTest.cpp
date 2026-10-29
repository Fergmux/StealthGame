// Camera story cs-010 AC1 (production/epics/camera-system/story-010-field-of-view.md; GDD camera-system.md AC-CS-18, F5):
// StealthGame.Camera.FovCap. At 16:9, 21:9 and 32:9 with the slider at 75 and 60 deg vertical the horizontal FOV is <= 108
// deg in every case; at 32:9 and 60 deg VFOV_eff is 42.3 +- 0.2; the slider top is 75 at 16:9 and 61.1 +- 0.2 at 21:9, and
// the slider is disabled at 32:9 (every position capped).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/Core/StealthFov.h"
#include "View/Tuning/StealthCameraParams.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraFovCapTest, "StealthGame.Camera.FovCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraFovCapTest::RunTest(const FString& Parameters)
{
	const FStealthCameraParams Params = FStealthCameraParams::Defaults();
	const double Aspect169 = 16.0 / 9.0;
	const double Aspect219 = 21.0 / 9.0;
	const double Aspect329 = 32.0 / 9.0;
	for (const double Aspect : { Aspect169, Aspect219, Aspect329 })
	{
		for (const double Vertical : { 75.0, 60.0 })
		{
			const FStealthFovResult Fov = StealthFov::Compute(Vertical, Aspect, Params);
			TestTrue(*FString::Printf(TEXT("aspect %.3f, %.0f deg vertical: horizontal %.2f <= 108"), Aspect, Vertical, Fov.HorizontalDeg), Fov.HorizontalDeg <= 108.0 + 1e-9);
		}
	}
	const FStealthFovResult Uncapped = StealthFov::Compute(75.0, Aspect169, Params);
	TestTrue(TEXT("16:9 at 75: uncapped (107.5 deg horizontal), VFOV_eff = 75"), !Uncapped.bCapped && FMath::IsNearlyEqual(Uncapped.VerticalEffDeg, 75.0) && FMath::IsNearlyEqual(Uncapped.HorizontalDeg, 107.5, 0.2));
	const FStealthFovResult Wide = StealthFov::Compute(60.0, Aspect329, Params);
	TestTrue(*FString::Printf(TEXT("32:9 at 60: capped, VFOV_eff %.2f = 42.3 +- 0.2"), Wide.VerticalEffDeg), Wide.bCapped && FMath::IsNearlyEqual(Wide.VerticalEffDeg, 42.3, 0.2));
	const FStealthFovSliderRange Top169 = StealthFov::SliderRange(Aspect169, Params);
	const FStealthFovSliderRange Top219 = StealthFov::SliderRange(Aspect219, Params);
	const FStealthFovSliderRange Top329 = StealthFov::SliderRange(Aspect329, Params);
	TestTrue(TEXT("slider top 75 at 16:9, enabled"), FMath::IsNearlyEqual(Top169.TopDeg, 75.0) && !Top169.bDisabled);
	TestTrue(*FString::Printf(TEXT("slider top %.2f = 61.1 +- 0.2 at 21:9, enabled"), Top219.TopDeg), FMath::IsNearlyEqual(Top219.TopDeg, 61.1, 0.2) && !Top219.bDisabled);
	TestTrue(TEXT("slider disabled at 32:9"), Top329.bDisabled);
	return true;
}

#endif
