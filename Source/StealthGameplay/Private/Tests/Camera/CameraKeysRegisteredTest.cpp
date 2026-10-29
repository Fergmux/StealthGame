// Camera story cs-001 AC1 (production/epics/camera-system/story-001-camera-tuning-keys-and-invariants.md; GDD camera-system.md
// AC-CS-22, Tuning Knobs; the TDC CR-15 lint): StealthGame.Camera.KeysRegistered. Every Camera key of the GDD Tuning Knobs
// table (the committed fixture StealthCameraTuningKnobs, copied from the GDD, never from the rows under test) exists in the
// generated schema with owner Camera, the client audience (a _Public row), its latch, default, hard and safe range and unit;
// EyeMaskHideFailCount is an integer; ViewHeadLeadMaxDeg's default is its declared "disabled" sentinel; no other schema row
// claims owner Camera; the six user settings marked * are not schema keys (TDC CR-14) and their hard ranges and defaults are
// registered in FStealthCameraUserSettings (clamped there, the only repair a user setting gets). FStealthCameraParams carries
// every key's schema default.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthTuningSchema.h"
#include "Server/Testing/StealthCameraTuningKnobs.h"
#include "View/Tuning/StealthCameraParams.h"

#include <limits>

namespace CameraKeysRegisteredTestData
{
	const FTuningSchemaRow* FindRow(const ANSICHAR* Key)
	{
		for (const FTuningSchemaRow& Row : GetTuningSchema())
		{
			if (FCStringAnsi::Strcmp(Row.Key, Key) == 0)
			{
				return &Row;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraKeysRegisteredTest, "StealthGame.Camera.KeysRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraKeysRegisteredTest::RunTest(const FString& Parameters)
{
	using namespace CameraKeysRegisteredTestData;
	constexpr double Tolerance = 1e-4;
	for (const StealthCameraTuningKnobs::FKnob& Knob : StealthCameraTuningKnobs::Knobs)
	{
		const FString Name(Knob.Key);
		const FTuningSchemaRow* Row = FindRow(Knob.Key);
		if (!TestNotNull(*FString::Printf(TEXT("%s is a schema row"), *Name), Row))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s: owner Camera"), *Name), Row->Owner != nullptr && FCStringAnsi::Strcmp(Row->Owner, StealthCameraTuningKnobs::Owner) == 0);
		TestTrue(*FString::Printf(TEXT("%s: audience Client (a _Public row)"), *Name), Row->bPublic);
		TestEqual(*FString::Printf(TEXT("%s: latch"), *Name), static_cast<int32>(Row->Latch), static_cast<int32>(Knob.Latch));
		TestEqual(*FString::Printf(TEXT("%s: default"), *Name), Row->Default, static_cast<double>(Knob.Default), Tolerance);
		TestEqual(*FString::Printf(TEXT("%s: hard min"), *Name), Row->HardMin, static_cast<double>(Knob.HardMin), Tolerance);
		TestEqual(*FString::Printf(TEXT("%s: hard max"), *Name), Row->HardMax, static_cast<double>(Knob.HardMax), Tolerance);
		TestEqual(*FString::Printf(TEXT("%s: safe min"), *Name), Row->SafeMin, static_cast<double>(Knob.SafeMin), Tolerance);
		TestEqual(*FString::Printf(TEXT("%s: safe max"), *Name), Row->SafeMax, static_cast<double>(Knob.SafeMax), Tolerance);
		TestTrue(*FString::Printf(TEXT("%s: unit %hs"), *Name, Knob.Unit), Row->Unit != nullptr && FCStringAnsi::Strcmp(Row->Unit, Knob.Unit) == 0);
		TestEqual(*FString::Printf(TEXT("%s: type"), *Name), static_cast<int32>(Row->TypeTag), static_cast<int32>(Knob.bInt ? StealthTuningTypeTag::Int : StealthTuningTypeTag::Float));
		if (Knob.bSentinel)
		{
			TestTrue(*FString::Printf(TEXT("%s: the default is its declared sentinel (disabled)"), *Name), Row->bHasSentinel && FMath::IsNearlyEqual(Row->Sentinel, Row->Default));
		}
		else
		{
			TestTrue(*FString::Printf(TEXT("%s: the default lies in its hard range"), *Name), Row->Default >= Row->HardMin - Tolerance && Row->Default <= Row->HardMax + Tolerance);
		}
	}
	int32 CameraRows = 0;
	for (const FTuningSchemaRow& Row : GetTuningSchema())
	{
		CameraRows += Row.Owner != nullptr && FCStringAnsi::Strcmp(Row.Owner, StealthCameraTuningKnobs::Owner) == 0 ? 1 : 0;
	}
	TestEqual(TEXT("exactly the GDD's Camera keys carry owner Camera"), CameraRows, static_cast<int32>(UE_ARRAY_COUNT(StealthCameraTuningKnobs::Knobs)));
	for (const ANSICHAR* Setting : StealthCameraTuningKnobs::UserSettingNames)
	{
		TestNull(*FString::Printf(TEXT("%hs is a user setting, not a schema key"), Setting), FindRow(Setting));
	}

	// The user settings' defaults and hard ranges (GDD rows marked *), clamped only there.
	const FStealthCameraUserSettings Defaults;
	TestTrue(TEXT("user setting defaults: FOV 60/60, dead zone 0.15, exponent 1.5, rates 180/120"),
		Defaults.FpFovVDeg == 60.0 && Defaults.TpFovVDeg == 60.0 && Defaults.StickDeadZoneFrac == 0.15 && Defaults.StickCurveExp == 1.5
		&& Defaults.StickYawRateDegPerSec == 180.0 && Defaults.StickPitchRateDegPerSec == 120.0);
	FStealthCameraUserSettings Wild;
	Wild.FpFovVDeg = 120.0;
	Wild.TpFovVDeg = 10.0;
	Wild.StickDeadZoneFrac = 0.9;
	Wild.StickCurveExp = std::numeric_limits<double>::quiet_NaN();
	Wild.StickYawRateDegPerSec = 1000.0;
	Wild.StickPitchRateDegPerSec = 1.0;
	const FStealthCameraUserSettings Clamped = Wild.Clamped();
	TestTrue(TEXT("user settings clamp to the hard ranges (55-75, 0.05-0.40, 1-2, 60-360); a non-finite value takes its default"),
		Clamped.FpFovVDeg == 75.0 && Clamped.TpFovVDeg == 55.0 && Clamped.StickDeadZoneFrac == 0.40 && Clamped.StickCurveExp == 1.5
		&& Clamped.StickYawRateDegPerSec == 360.0 && Clamped.StickPitchRateDegPerSec == 60.0);

	// The plain parameter set carries the schema defaults (a spot check of each group) and the consumed keys.
	const FStealthCameraParams Params = FStealthCameraParams::Defaults();
	TestTrue(TEXT("params: envelope, boom, mask, toggle, FOV and stick defaults"), Params.ViewPitchDownLimitDeg == 60.0 && Params.BoomLengthCm == 180.0
		&& Params.CameraMaskOffHysteresisCm == 10.0 && Params.EyeMaskHideFailCount == 2 && FMath::IsNearlyEqual(Params.CameraToggleBlendSec, 0.2, 1e-6)
		&& Params.FovHMaxDeg == 108.0 && FMath::IsNearlyEqual(Params.StickOuterZoneFrac, 0.98, 1e-6));
	TestFalse(TEXT("params: the head-lead clamp is disabled by default (CS-R3.5)"), Params.IsHeadLeadClampEnabled());
	TestTrue(TEXT("params: the consumed SCL yaw limit (70) and the IIF lockout (0.30)"), Params.Consumed.LookYawLimitDeg == 70.0 && Params.Consumed.CameraToggleLockoutSec == 0.30);
	return true;
}

#endif
