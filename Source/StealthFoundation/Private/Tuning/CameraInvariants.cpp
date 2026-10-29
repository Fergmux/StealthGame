// Story cs-001 (production/epics/camera-system/story-001-camera-tuning-keys-and-invariants.md): the Camera invariants
// I-CS-1..5 as TDC Check rows. See Server/StealthCameraInvariants.h.

#include "Server/StealthCameraInvariants.h"

#include "Server/StealthServerTuning.h"

FCameraInvariantInputs FCameraInvariantInputs::FromValues(const FStealthServerTuning& Values)
{
	FCameraInvariantInputs Out;
	Out.CameraToggleLockoutSec = StealthTuningPendingOwnerKeys::CameraToggleLockoutSec;
	return Out;
}

namespace StealthCameraInvariants
{
	namespace
	{
		ETuningCheck PassIf(bool bHolds) { return bHolds ? ETuningCheck::Pass : ETuningCheck::Reject; }
	}

	ETuningCheck CheckICs1(const FStealthServerTuning& Values, const FCameraInvariantInputs& In)
	{
		return PassIf(static_cast<double>(Values.Camera_Public.CameraToggleBlendSec) <= In.CameraToggleLockoutSec - FrameFloorSec + CompareToleranceAbs);
	}

	ETuningCheck CheckICs2(const FStealthServerTuning& Values, const FCameraInvariantInputs&)
	{
		return PassIf(static_cast<double>(Values.Camera_Public.BoomHeightOffsetCm) <= BoomHeightMaxCm + CompareToleranceAbs);
	}

	ETuningCheck CheckICs3(const FStealthServerTuning& Values, const FCameraInvariantInputs&)
	{
		const FCameraTuning_Public& C = Values.Camera_Public;
		return PassIf(static_cast<double>(C.EyeConeHalfHDeg) + CompareToleranceAbs >= 0.5 * C.FovHMaxDeg
			&& static_cast<double>(C.EyeConeHalfVDeg) + CompareToleranceAbs >= 0.5 * C.FovVMaxDeg);
	}

	ETuningCheck CheckICs4(const FStealthServerTuning& Values, const FCameraInvariantInputs& In)
	{
		const FCameraTuning_Public& C = Values.Camera_Public;
		return PassIf(static_cast<double>(C.ConcealedBoomCm) <= In.BushDomeInnerRadiusCm - DomeClearanceCm + CompareToleranceAbs
			&& C.ConcealedBoomCm <= C.BoomLengthCm + CompareToleranceAbs);
	}

	ETuningCheck CheckICs5(const FStealthServerTuning& Values, const FCameraInvariantInputs&)
	{
		return PassIf(static_cast<double>(Values.Camera_Public.CameraEdgePushDeg) + CompareToleranceAbs >= EdgePushMinDeg);
	}
}
