#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/CrowdMotionProfileData.h"
#include "View/Core/StealthGait.h"

class FCrowdDraws;
struct FLocomotionTuning_Public;

/**
 * One step of the virtual input layer as it can be recorded and replayed through a bot client's Enhanced Input path
 * (ADR-0016 section 4.6; story cnb-010, replayed by story cnb-034). POD; recorded in non-Shipping builds only.
 */
#if !UE_BUILD_SHIPPING
struct FCrowdVirtualInputFrame
{
	double ServerTime = 0.0;
	float CameraYaw = 0.0f;
	float CameraPitch = 0.0f;
	/** The device input as sent: Move in input space (X right, Y forward of the camera yaw). */
	FVector2D DeviceInput = FVector2D::ZeroVector;
	EGait Gait = EGait::Walk;
};
#endif

/** What the brain wants the head and camera to do this tick. */
struct FCrowdGazeIntent
{
	/** True when a gaze target holds the camera (a glance, hold, partner, goods or salient look). */
	bool bActive = false;
	/** World yaw and pitch of the gaze target, degrees. */
	float YawDeg = 0.0f;
	float PitchDeg = 0.0f;
};

/** The Move and Look the layer emits this tick (through the sink). */
struct FCrowdVirtualInputOutput
{
	FVector2D Move = FVector2D::ZeroVector;
	EGait Gait = EGait::Walk;
	FRotator Look = FRotator::ZeroRotator;
};

namespace StealthCrowdView
{
	/**
	 * The shared view envelope (Camera F1, CS-R3, Camera change request 5) as the crowd needs it: the camera pitch clamp
	 * a human's view obeys. The Camera epic's exported EvaluateViewEnvelope does not exist yet; this stand-in clamps the
	 * pitch to the SCL look pitch limit and normalises the yaw, and is replaced by the Camera function when it lands
	 * (story cnb-010 deviation).
	 */
	STEALTHGAMEPLAY_API FRotator EvaluateViewEnvelope(const FRotator& Desired, const FLocomotionTuning_Public& Locomotion);

	/** The 8-way key combination nearest RelativeYawDeg (0 = forward, 90 = right), as a raw unit-or-diagonal key vector. */
	STEALTHGAMEPLAY_API FVector2D EightWayKeys(double RelativeYawDeg);
}

/**
 * FCrowdVirtualInput: the crowd's input emulation (ADR-0016 section 4.3, OQ11 decided; GDD CNB-R3.4; story cnb-010).
 * The follower never emits the world direction it wants. Each pawn keeps a virtual camera (yaw, pitch) and a device
 * profile drawn once (ECrowdDecision::DeviceProfile) from UCrowdMotionProfileData's mix. Every controller tick (not at
 * the steering rate):
 *   (a) the camera moves toward the gaze target, or toward the travel direction plus a drawn lead, at a turn rate drawn
 *       per push from the profile, through EvaluateViewEnvelope;
 *   (b) the travel direction becomes the input a human with that camera would press: keyboard and mouse quantise it to
 *       8 ways relative to the camera yaw and scale it with the walk-slower modifier (StealthWalkSlower::ScaleMove, the
 *       client builder's own function); a gamepad stick keeps the angle (quantised by the profile's notch step) at the
 *       pace magnitude and passes the client dead zone (StealthLocomotion::ApplyStickDeadZone);
 *   (c) Look is the camera rotation.
 * A walking glance therefore bends the path exactly as a human's does. Plain C++.
 */
class STEALTHGAMEPLAY_API FCrowdVirtualInput
{
public:
	/** Draws the pawn's device profile (once) from Profiles' mix and sets the camera to the body facing. */
	void Initialize(uint32 InOrdinal, TConstArrayView<FCrowdDeviceProfile> Profiles, float BodyYawDeg, FCrowdDraws& Draws);

	/**
	 * One controller tick. TravelDirection: the follower's unit world direction (zero when still); Magnitude: its pace
	 * or 0; Gaze: the brain's look intent.
	 */
	FCrowdVirtualInputOutput Tick(double Now, float Dt, const FVector2D& TravelDirection, float Magnitude, EGait Gait, const FCrowdGazeIntent& Gaze,
		const FLocomotionTuning_Public& Locomotion, FCrowdDraws& Draws);

	const FCrowdDeviceProfile& GetProfile() const { return Profile; }
	ECrowdInputDevice GetDevice() const { return Profile.Device; }
	float GetCameraYaw() const { return CameraYaw; }
	float GetCameraPitch() const { return CameraPitch; }

	/** The world yaw a device input produces under a camera yaw (the inverse of (b); the command component's mapping). */
	static double WorldYawOfInput(const FVector2D& Input, double CameraYawDeg);

#if !UE_BUILD_SHIPPING
	/** Starts or stops recording FCrowdVirtualInputFrame (non-Shipping). */
	void SetRecording(bool bRecord) { bRecording = bRecord; if (!bRecord) { Recorded.Reset(); } }
	TConstArrayView<FCrowdVirtualInputFrame> GetRecorded() const { return Recorded; }
#endif

private:
	void BeginPush(double Now, FCrowdDraws& Draws);

	uint32 Ordinal = 0;
	FCrowdDeviceProfile Profile;
	float CameraYaw = 0.0f;
	float CameraPitch = 0.0f;
	/** The current push: its turn rate and the travel lead it settles on. */
	float PushTurnRate = 120.0f;
	float PushLeadDeg = 0.0f;
	double NextPushTime = 0.0;
	bool bWasGazing = false;
#if !UE_BUILD_SHIPPING
	bool bRecording = false;
	TArray<FCrowdVirtualInputFrame> Recorded;
#endif
};
