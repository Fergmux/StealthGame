#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CrowdMotionProfileData.generated.h"

/** How a virtual human steers (ADR-0016 section 4.3). Values are serialised in the data asset: append only. */
UENUM()
enum class ECrowdInputDevice : uint8
{
	/** 8-way quantised move relative to the camera yaw, mouse-like camera turns. */
	KeyboardMouse,
	/** Analog stick with a dead zone and response curve, stick-like camera turns. */
	Gamepad,
};

/** One device profile (ADR-0016 section 4.3, 4.5). Provisional values from internal play until human telemetry exists (GDD OQ12). */
USTRUCT()
struct FCrowdDeviceProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Crowd")
	ECrowdInputDevice Device = ECrowdInputDevice::KeyboardMouse;

	/** Share of pawns drawing this profile, in percent of the profile list's total. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "100"))
	float MixPct = 50.0f;

	/** Gamepad: stick dead zone (the client builder's dead zone; below it the stick reads zero). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "0.5"))
	float StickDeadZone = 0.2f;

	/** Gamepad: response curve exponent (output = input^exponent over the live range). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.5", ClampMax = "3"))
	float StickResponseExp = 1.0f;

	/** Gamepad: stick angle quantisation in degrees (hardware notch resolution; 0 = continuous). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "45"))
	float StickAngleStepDeg = 0.0f;

	/** Camera turn-rate distribution (deg/s), uniform in [Min, Max], drawn per push. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "10", ClampMax = "720"))
	float CameraTurnRateMinDegPerSec = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "10", ClampMax = "720"))
	float CameraTurnRateMaxDegPerSec = 240.0f;

	/** Camera lead: how far ahead of the travel direction the camera settles while walking (deg, uniform +-). */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "45"))
	float CameraLeadJitterDeg = 8.0f;

	/** Camera push frequency: mean seconds between small camera adjustments while walking straight. */
	UPROPERTY(EditAnywhere, Category = "Crowd", meta = (ClampMin = "0.2", ClampMax = "20"))
	float CameraPushIntervalSec = 2.5f;
};

/**
 * UCrowdMotionProfileData: the ServerOnly data asset of the crowd's virtual input layer (ADR-0016 section 4.5; GDD
 * CNB-R3.4; stories cnb-003 and cnb-010), referenced by the Crowd tuning key CrowdMotionProfileAsset. Replacing a
 * profile never changes code. Default asset: /Game/StealthGame/AI/DA_CrowdMotionProfile_Default.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API UCrowdMotionProfileData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The device mix: each pawn draws one profile once (ECrowdDecision::DeviceProfile), weighted by MixPct. */
	UPROPERTY(EditAnywhere, Category = "Crowd")
	TArray<FCrowdDeviceProfile> Profiles;

	/** The built-in provisional profiles (keyboard and mouse 60 %, gamepad 40 %), used by tests and the default asset. */
	static TArray<FCrowdDeviceProfile> MakeProvisionalProfiles();
};
