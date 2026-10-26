#include "Server/CrowdMotionProfileData.h"

TArray<FCrowdDeviceProfile> UCrowdMotionProfileData::MakeProvisionalProfiles()
{
	// Provisional profiles from internal play (GDD OQ12): replaced by logged human telemetry, never by code.
	TArray<FCrowdDeviceProfile> Out;
	FCrowdDeviceProfile& Keyboard = Out.AddDefaulted_GetRef();
	Keyboard.Device = ECrowdInputDevice::KeyboardMouse;
	Keyboard.MixPct = 60.0f;
	Keyboard.CameraTurnRateMinDegPerSec = 120.0f;
	Keyboard.CameraTurnRateMaxDegPerSec = 320.0f;
	Keyboard.CameraLeadJitterDeg = 10.0f;
	Keyboard.CameraPushIntervalSec = 2.0f;

	FCrowdDeviceProfile& Pad = Out.AddDefaulted_GetRef();
	Pad.Device = ECrowdInputDevice::Gamepad;
	Pad.MixPct = 40.0f;
	Pad.StickDeadZone = 0.2f;
	Pad.StickResponseExp = 1.6f;
	Pad.StickAngleStepDeg = 0.0f;
	Pad.CameraTurnRateMinDegPerSec = 70.0f;
	Pad.CameraTurnRateMaxDegPerSec = 180.0f;
	Pad.CameraLeadJitterDeg = 6.0f;
	Pad.CameraPushIntervalSec = 3.0f;
	return Out;
}
