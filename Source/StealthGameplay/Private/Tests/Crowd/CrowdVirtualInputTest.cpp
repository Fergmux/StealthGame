// Story cnb-010 (production/epics/crowd-npc-behaviour/story-010-virtual-input-layer.md): the virtual input layer
// (ADR-0016 section 4.3, 4.5, 4.6; GDD CNB-R3.4, OQ11 decided: input emulation).
//   DeviceProfiles  each pawn draws one profile once from the motion profile's mix (2,000 pawns: the 60 / 40 mix within
//                   a 99 % binomial interval); keyboard input is 8-way relative to the camera yaw at the pace magnitude,
//                   gamepad input analog with the profile's notch step and the client dead zone.
//   EveryTickLook   with steering at 30 Hz and 60 fps the Look changes on consecutive frames during a pan.
//   WalkingGlance   a scripted walking glance turns the camera; the keyboard Move direction is then the 8-way
//                   quantisation of the travel direction relative to the new camera yaw (the path bends by exactly that).
//   Recording       FCrowdVirtualInputFrame records ServerTime, camera, device input and gait (non-Shipping).
//   ProfileAsset    the default asset DA_CrowdMotionProfile_Default exists, is a UCrowdMotionProfileData, and the three
//                   Crowd asset keys validate (stories cnb-003 / cnb-012 mechanism).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "Server/CrowdAssetKeys.h"
#include "Server/CrowdDraws.h"
#include "Server/CrowdMotionProfileData.h"
#include "Server/CrowdVirtualInput.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/StealthTuningPublicSubstructs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdVirtualInputDeviceProfilesTest, "StealthGame.Crowd.VirtualInput.DeviceProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdVirtualInputDeviceProfilesTest::RunTest(const FString& Parameters)
{
	const TArray<FCrowdDeviceProfile> Profiles = UCrowdMotionProfileData::MakeProvisionalProfiles();
	const FLocomotionTuning_Public Locomotion;
	FCrowdDraws Draws(0xD3F1CE00ull);
	int32 Keyboards = 0;
	constexpr int32 Pawns = 2000;
	int32 KeyboardOffGrid = 0;
	int32 PadChecked = 0;
	for (uint32 Ordinal = 0; Ordinal < Pawns; ++Ordinal)
	{
		FCrowdVirtualInput Input;
		Input.Initialize(Ordinal, Profiles, 0.0f, Draws);
		const bool bKeyboard = Input.GetDevice() == ECrowdInputDevice::KeyboardMouse;
		Keyboards += bKeyboard ? 1 : 0;
		// Walk toward a direction 33 degrees off the camera with no gaze: the camera starts at the body yaw (0).
		const FVector2D Travel(FMath::Cos(FMath::DegreesToRadians(33.0)), FMath::Sin(FMath::DegreesToRadians(33.0)));
		const FCrowdVirtualInputOutput Out = Input.Tick(0.0, 0.0f, Travel, 0.9f, EGait::Walk, FCrowdGazeIntent(), Locomotion, Draws);
		if (bKeyboard)
		{
			// 8-way: the input direction is a multiple of 45 degrees and its magnitude is the pace.
			const double Angle = FMath::RadiansToDegrees(FMath::Atan2(Out.Move.X, Out.Move.Y));
			const double Remainder = FMath::Abs(FMath::Fmod(Angle + 360.0, 45.0));
			KeyboardOffGrid += (FMath::Min(Remainder, 45.0 - Remainder) > 1e-6 || !FMath::IsNearlyEqual(Out.Move.Size(), 0.9, 1e-6)) ? 1 : 0;
		}
		else
		{
			// Analog: the exact relative angle at the pace magnitude (no notch step in the provisional profile).
			const double Angle = FMath::RadiansToDegrees(FMath::Atan2(Out.Move.X, Out.Move.Y));
			TestTrue(TEXT("gamepad keeps the analog angle"), FMath::IsNearlyEqual(Angle, 33.0, 1e-4));
			TestTrue(TEXT("gamepad magnitude is the pace"), FMath::IsNearlyEqual(Out.Move.Size(), 0.9, 1e-6));
			++PadChecked;
		}
	}
	double Low = 0.0, High = 0.0;
	const bool bMix = StealthCrowdStats::InBinomial99(Keyboards, Pawns, 0.6, &Low, &High);
	AddInfo(FString::Printf(TEXT("DEVICEMIX %d of %d keyboard (99 %% interval %.3f-%.3f)"), Keyboards, Pawns, Low, High));
	TestTrue(TEXT("the device mix follows the profile (60 % keyboard)"), bMix);
	TestEqual(TEXT("every keyboard input is 8-way at the pace"), KeyboardOffGrid, 0);
	TestTrue(TEXT("gamepads were checked"), PadChecked > 500);
	// The client dead zone: a stick reading below 0.2 is sent as zero (the client builder's function).
	TestTrue(TEXT("the dead zone is the client's"), StealthLocomotion::ApplyStickDeadZone(FVector2D(0.0, 0.19)).IsNearlyZero());
	// Drawn once: the same ordinal and stream give the same profile.
	FCrowdDraws A(0x5);
	FCrowdDraws B(0x5);
	FCrowdVirtualInput First, Second;
	First.Initialize(17, Profiles, 0.0f, A);
	Second.Initialize(17, Profiles, 0.0f, B);
	TestEqual(TEXT("one device profile per pawn"), static_cast<int32>(First.GetDevice()), static_cast<int32>(Second.GetDevice()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdVirtualInputEveryTickLookTest, "StealthGame.Crowd.VirtualInput.EveryTickLook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdVirtualInputEveryTickLookTest::RunTest(const FString& Parameters)
{
	const TArray<FCrowdDeviceProfile> Profiles = UCrowdMotionProfileData::MakeProvisionalProfiles();
	const FLocomotionTuning_Public Locomotion;
	FCrowdDraws Draws(0x1001);
	FCrowdVirtualInput Input;
	Input.Initialize(3, Profiles, 0.0f, Draws);
	// A pan: the gaze target is 120 degrees away; the steering (travel) direction changes only every second frame (30 Hz).
	FCrowdGazeIntent Gaze;
	Gaze.bActive = true;
	Gaze.YawDeg = 120.0f;
	float PreviousYaw = Input.GetCameraYaw();
	int32 Changes = 0;
	constexpr int32 Frames = 20;
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		const FVector2D Travel = (Frame / 2) % 2 == 0 ? FVector2D(1.0, 0.0) : FVector2D(0.0, 1.0);
		const FCrowdVirtualInputOutput Out = Input.Tick(Frame / 60.0, 1.0f / 60.0f, Travel, 1.0f, EGait::Walk, Gaze, Locomotion, Draws);
		Changes += !FMath::IsNearlyEqual(static_cast<float>(Out.Look.Yaw), PreviousYaw, 1e-4f) ? 1 : 0;
		PreviousYaw = static_cast<float>(Out.Look.Yaw);
	}
	TestEqual(TEXT("Look changes on every consecutive frame of the pan (not at the 30 Hz steering rate)"), Changes, Frames);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdVirtualInputWalkingGlanceTest, "StealthGame.Crowd.VirtualInput.WalkingGlance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdVirtualInputWalkingGlanceTest::RunTest(const FString& Parameters)
{
	// A keyboard walker heading +X while a glance turns its camera to +60 degrees.
	FCrowdDeviceProfile Keyboard;
	Keyboard.Device = ECrowdInputDevice::KeyboardMouse;
	Keyboard.MixPct = 100.0f;
	Keyboard.CameraTurnRateMinDegPerSec = 600.0f;
	Keyboard.CameraTurnRateMaxDegPerSec = 600.0f;
	Keyboard.CameraLeadJitterDeg = 0.0f;
	const TArray<FCrowdDeviceProfile> Profiles = { Keyboard };
	const FLocomotionTuning_Public Locomotion;
	FCrowdDraws Draws(0x6141);
	FCrowdVirtualInput Input;
	Input.Initialize(1, Profiles, 0.0f, Draws);
	const FVector2D Travel(1.0, 0.0);
	FCrowdVirtualInputOutput Straight = Input.Tick(0.0, 1.0f / 60.0f, Travel, 1.0f, EGait::Walk, FCrowdGazeIntent(), Locomotion, Draws);
	TestTrue(TEXT("walking straight: the world Move direction is the travel direction"), FMath::IsNearlyZero(FCrowdVirtualInput::WorldYawOfInput(Straight.Move, Straight.Look.Yaw), 1e-3));
	FCrowdGazeIntent Glance;
	Glance.bActive = true;
	Glance.YawDeg = 60.0f;
	FCrowdVirtualInputOutput During;
	for (int32 Frame = 1; Frame <= 30; ++Frame)
	{
		During = Input.Tick(Frame / 60.0, 1.0f / 60.0f, Travel, 1.0f, EGait::Walk, Glance, Locomotion, Draws);
	}
	TestTrue(TEXT("the camera reached the glance"), FMath::IsNearlyEqual(static_cast<float>(During.Look.Yaw), 60.0f, 0.01f));
	// Relative travel -60 degrees: the nearest 8-way key combination is -45 (W+A): the world direction becomes +15.
	const double Predicted = 60.0 + FMath::RadiansToDegrees(FMath::Atan2(StealthCrowdView::EightWayKeys(-60.0).X, StealthCrowdView::EightWayKeys(-60.0).Y));
	const double Actual = FCrowdVirtualInput::WorldYawOfInput(During.Move, During.Look.Yaw);
	AddInfo(FString::Printf(TEXT("WALKINGGLANCE world Move yaw %.2f deg during a 60 deg glance (quantisation predicts %.2f)"), Actual, Predicted));
	TestTrue(TEXT("the Move bends by exactly the keyboard quantisation (AC 3)"), FMath::IsNearlyEqual(Actual, Predicted, 1e-3));
	TestTrue(TEXT("and the path does bend"), FMath::Abs(Actual) > 10.0);
	return true;
}

#if !UE_BUILD_SHIPPING
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdVirtualInputRecordingTest, "StealthGame.Crowd.VirtualInput.Recording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdVirtualInputRecordingTest::RunTest(const FString& Parameters)
{
	const TArray<FCrowdDeviceProfile> Profiles = UCrowdMotionProfileData::MakeProvisionalProfiles();
	const FLocomotionTuning_Public Locomotion;
	FCrowdDraws Draws(0x7);
	FCrowdVirtualInput Input;
	Input.Initialize(9, Profiles, 0.0f, Draws);
	Input.SetRecording(true);
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		Input.Tick(Frame / 60.0, 1.0f / 60.0f, FVector2D(0.0, 1.0), 0.8f, EGait::Walk, FCrowdGazeIntent(), Locomotion, Draws);
	}
	const TConstArrayView<FCrowdVirtualInputFrame> Frames = Input.GetRecorded();
	if (TestEqual(TEXT("one frame per tick"), Frames.Num(), 10))
	{
		TestEqual(TEXT("ServerTime"), Frames[9].ServerTime, 9.0 / 60.0);
		TestTrue(TEXT("device input at the pace"), FMath::IsNearlyEqual(Frames[9].DeviceInput.Size(), 0.8, 1e-6));
		TestEqual(TEXT("gait"), static_cast<int32>(Frames[9].Gait), static_cast<int32>(EGait::Walk));
	}
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdVirtualInputProfileAssetTest, "StealthGame.Crowd.VirtualInput.ProfileAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdVirtualInputProfileAssetTest::RunTest(const FString& Parameters)
{
	const FCrowdAssetKeys Keys;
	const UCrowdMotionProfileData* Profile = Cast<UCrowdMotionProfileData>(Keys.CrowdMotionProfileAsset.TryLoad());
	if (TestNotNull(TEXT("DA_CrowdMotionProfile_Default loads"), Profile))
	{
		TestEqual(TEXT("it holds the provisional profiles"), Profile->Profiles.Num(), 2);
	}
	TArray<FString> Problems;
	const bool bValid = StealthCrowdTuning::ValidateAssetKeys(Keys, &Problems);
	for (const FString& Problem : Problems)
	{
		AddError(Problem);
	}
	TestTrue(TEXT("the three Crowd asset keys validate (class, crowd schema)"), bValid);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
