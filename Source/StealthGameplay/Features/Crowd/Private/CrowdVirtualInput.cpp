#include "Server/CrowdVirtualInput.h"

#include "Server/CrowdDraws.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/Core/WalkSlowerScale.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace CrowdVirtualInputLocal
{
	/** The camera settles within this of its target before a new push is drawn. */
	constexpr float SettleDeg = 0.5f;
}

FRotator StealthCrowdView::EvaluateViewEnvelope(const FRotator& Desired, const FLocomotionTuning_Public& Locomotion)
{
	FRotator Out = Desired;
	Out.Yaw = FRotator::NormalizeAxis(Out.Yaw);
	Out.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Out.Pitch), -Locomotion.LookPitchLimitDeg, Locomotion.LookPitchLimitDeg);
	Out.Roll = 0.0;
	return Out;
}

FVector2D StealthCrowdView::EightWayKeys(double RelativeYawDeg)
{
	const double Quantised = FMath::RoundToDouble(FRotator::NormalizeAxis(RelativeYawDeg) / 45.0) * 45.0;
	const double Rad = FMath::DegreesToRadians(Quantised);
	// W/S on Y, A/D on X; a diagonal is two keys (1, 1), clamped to 1 by the client builder's ScaleMove.
	double X = FMath::Sin(Rad);
	double Y = FMath::Cos(Rad);
	X = FMath::Abs(X) < 0.3 ? 0.0 : FMath::Sign(X);
	Y = FMath::Abs(Y) < 0.3 ? 0.0 : FMath::Sign(Y);
	return FVector2D(X, Y);
}

double FCrowdVirtualInput::WorldYawOfInput(const FVector2D& Input, double CameraYawDeg)
{
	// UPawnCommandComponent::GetWorldMoveInput: Forward(yaw) * Y + Right(yaw) * X.
	return FRotator::NormalizeAxis(CameraYawDeg + FMath::RadiansToDegrees(FMath::Atan2(Input.X, Input.Y)));
}

void FCrowdVirtualInput::Initialize(uint32 InOrdinal, TConstArrayView<FCrowdDeviceProfile> Profiles, float BodyYawDeg, FCrowdDraws& Draws)
{
	Ordinal = InOrdinal;
	TArray<uint32, TInlineAllocator<8>> Cumulative;
	uint32 Total = 0;
	for (const FCrowdDeviceProfile& Candidate : Profiles)
	{
		Total += static_cast<uint32>(FMath::Max(FMath::RoundToInt32(Candidate.MixPct * 100.0f), 0));
		Cumulative.Add(Total);
	}
	const int32 Pick = Draws.DrawWeighted(Ordinal, ECrowdDecision::DeviceProfile, Cumulative);
	Profile = Profiles.IsValidIndex(Pick) ? Profiles[Pick] : FCrowdDeviceProfile();
	CameraYaw = BodyYawDeg;
	CameraPitch = 0.0f;
	NextPushTime = 0.0;
}

void FCrowdVirtualInput::BeginPush(double Now, FCrowdDraws& Draws)
{
	PushTurnRate = static_cast<float>(Draws.DrawUniform(Ordinal, ECrowdDecision::TurnRate, Profile.CameraTurnRateMinDegPerSec, FMath::Max(Profile.CameraTurnRateMaxDegPerSec, Profile.CameraTurnRateMinDegPerSec)));
	PushLeadDeg = static_cast<float>(Draws.DrawUniform(Ordinal, ECrowdDecision::CameraLead, -Profile.CameraLeadJitterDeg, Profile.CameraLeadJitterDeg));
	NextPushTime = Now + Draws.DrawExponential(Ordinal, ECrowdDecision::CameraPush, 1.0 / FMath::Max(static_cast<double>(Profile.CameraPushIntervalSec), 0.1));
}

FCrowdVirtualInputOutput FCrowdVirtualInput::Tick(double Now, float Dt, const FVector2D& TravelDirection, float Magnitude, EGait Gait, const FCrowdGazeIntent& Gaze,
	const FLocomotionTuning_Public& Locomotion, FCrowdDraws& Draws)
{
	using namespace CrowdVirtualInputLocal;
	const bool bMoving = Magnitude > 0.0f && !TravelDirection.IsNearlyZero();
	if (Now >= NextPushTime || Gaze.bActive != bWasGazing)
	{
		BeginPush(Now, Draws);
	}
	bWasGazing = Gaze.bActive;

	// (a) The camera: the gaze target holds it; walking without a gaze it settles on the travel direction plus the
	// drawn lead; standing without a gaze it stays (the SCL idle gaze layer, not the crowd, scans an idle head).
	float TargetYaw = CameraYaw;
	float TargetPitch = 0.0f;
	if (Gaze.bActive)
	{
		TargetYaw = Gaze.YawDeg;
		TargetPitch = Gaze.PitchDeg;
	}
	else if (bMoving)
	{
		TargetYaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(TravelDirection.Y, TravelDirection.X))) + PushLeadDeg;
	}
	const float MaxStep = PushTurnRate * Dt;
	const float YawError = StealthLocomotion::DeltaYawDeg(CameraYaw, TargetYaw);
	CameraYaw = FRotator::NormalizeAxis(CameraYaw + FMath::Clamp(YawError, -MaxStep, MaxStep));
	CameraPitch += FMath::Clamp(TargetPitch - CameraPitch, -MaxStep, MaxStep);
	const FRotator View = StealthCrowdView::EvaluateViewEnvelope(FRotator(CameraPitch, CameraYaw, 0.0f), Locomotion);
	CameraYaw = static_cast<float>(View.Yaw);
	CameraPitch = static_cast<float>(View.Pitch);

	// (b) The device input for the travel direction under this camera.
	FCrowdVirtualInputOutput Out;
	Out.Gait = Gait;
	if (bMoving)
	{
		const double TravelYaw = FMath::RadiansToDegrees(FMath::Atan2(TravelDirection.Y, TravelDirection.X));
		const double Relative = StealthLocomotion::DeltaYawDeg(CameraYaw, static_cast<float>(TravelYaw));
		if (Profile.Device == ECrowdInputDevice::KeyboardMouse)
		{
			Out.Move = StealthWalkSlower::ScaleMove(StealthCrowdView::EightWayKeys(Relative), EMoveInputDevice::Keyboard, Magnitude, Gait == EGait::Run);
		}
		else
		{
			double Angle = Relative;
			if (Profile.StickAngleStepDeg > 0.0f)
			{
				Angle = FMath::RoundToDouble(Angle / Profile.StickAngleStepDeg) * Profile.StickAngleStepDeg;
			}
			const double Rad = FMath::DegreesToRadians(Angle);
			const FVector2D Stick(FMath::Sin(Rad) * Magnitude, FMath::Cos(Rad) * Magnitude);
			Out.Move = StealthLocomotion::ApplyStickDeadZone(StealthWalkSlower::ScaleMove(Stick, EMoveInputDevice::Gamepad, Magnitude, Gait == EGait::Run));
		}
	}
	// (c) Look is the camera.
	Out.Look = FRotator(CameraPitch, CameraYaw, 0.0f);
#if !UE_BUILD_SHIPPING
	if (bRecording)
	{
		FCrowdVirtualInputFrame& Frame = Recorded.AddDefaulted_GetRef();
		Frame.ServerTime = Now;
		Frame.CameraYaw = CameraYaw;
		Frame.CameraPitch = CameraPitch;
		Frame.DeviceInput = Out.Move;
		Frame.Gait = Out.Gait;
	}
#endif
	(void)SettleDeg;
	return Out;
}
