#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

/**
 * The Camera System Tuning Knobs rows, as committed fixture data copied from the GDD table (design/gdd/camera-system.md,
 * Tuning Knobs, read 2026-10-29; story cs-001). Never derived from the rows under test: StealthGame.Camera.KeysRegistered
 * compares the compiled schema against this table and StealthGame.Tuning.SchemaComplete pins the defaults. Every row is
 * owner Camera, audience Client (the _Public face). ViewHeadLeadMaxDeg's default is its "disabled" sentinel 0 (35 when
 * enabled); CameraPivotLagTraversalSec is the row's parenthesised traversal value (0.12). The six user settings marked * in
 * the GDD are not keys (View/Tuning/StealthCameraParams.h, FStealthCameraUserSettings).
 */
namespace StealthCameraTuningKnobs
{
	struct FKnob
	{
		const ANSICHAR* Key;
		float Default;
		float HardMin;
		float HardMax;
		float SafeMin;
		float SafeMax;
		ETuningLatch Latch;
		const ANSICHAR* Unit;
		bool bInt;
		/** The default is a declared sentinel (valid by itself, outside the hard range). */
		bool bSentinel;
	};

	/** The owner id every Camera row carries. */
	inline constexpr const ANSICHAR* Owner = "Camera";

	/** The Camera-owned keys (GDD Tuning Knobs table order). */
	inline constexpr FKnob Knobs[] = {
		{ "ViewHeadLeadMaxDeg", 0.0f, 15.0f, 70.0f, 25.0f, 45.0f, ETuningLatch::Round, "Deg", false, true },
		{ "ViewPitchDownLimitDeg", 60.0f, 20.0f, 75.0f, 50.0f, 65.0f, ETuningLatch::Round, "Deg", false, false },
		{ "ViewPitchUpLimitDeg", 40.0f, 20.0f, 60.0f, 30.0f, 45.0f, ETuningLatch::Round, "Deg", false, false },
		{ "CameraEdgePushDeg", 5.0f, 2.0f, 10.0f, 3.0f, 8.0f, ETuningLatch::Use, "Deg", false, false },
		{ "EdgePushMinRateDegPerSec", 15.0f, 5.0f, 40.0f, 10.0f, 25.0f, ETuningLatch::Use, "PerSec", false, false },
		{ "EdgePushLatchSec", 0.15f, 0.05f, 0.3f, 0.1f, 0.2f, ETuningLatch::Use, "Sec", false, false },
		{ "CameraEdgeSoftMinGain", 0.1f, 0.05f, 0.3f, 0.08f, 0.15f, ETuningLatch::Use, "Gain", false, false },
		{ "EdgePushRelaxDegPerSec", 60.0f, 20.0f, 200.0f, 40.0f, 120.0f, ETuningLatch::Use, "PerSec", false, false },
		{ "CameraEdgeSoftYawDeg", 10.0f, 0.0f, 15.0f, 6.0f, 12.0f, ETuningLatch::Use, "Deg", false, false },
		{ "CameraEdgeSoftPitchDeg", 5.0f, 0.0f, 10.0f, 3.0f, 6.0f, ETuningLatch::Use, "Deg", false, false },
		{ "CameraPivotLagSec", 0.1f, 0.0f, 0.25f, 0.06f, 0.14f, ETuningLatch::Use, "Sec", false, false },
		{ "CameraPivotLagTraversalSec", 0.12f, 0.0f, 0.25f, 0.06f, 0.14f, ETuningLatch::Use, "Sec", false, false },
		{ "PivotMaxVertCmPerSec", 300.0f, 100.0f, 600.0f, 200.0f, 400.0f, ETuningLatch::Use, "CmPerSec", false, false },
		{ "BoomLengthCm", 180.0f, 100.0f, 250.0f, 150.0f, 200.0f, ETuningLatch::Use, "Cm", false, false },
		{ "ShoulderOffsetCm", 45.0f, 0.0f, 60.0f, 35.0f, 50.0f, ETuningLatch::Use, "Cm", false, false },
		{ "BoomHeightOffsetCm", 15.0f, 0.0f, 25.0f, 5.0f, 20.0f, ETuningLatch::Use, "Cm", false, false },
		{ "CameraGuardRadiusCm", 2.0f, 1.0f, 5.0f, 2.0f, 2.0f, ETuningLatch::Use, "Cm", false, false },
		{ "CameraProbeRadiusCm", 14.0f, 10.0f, 20.0f, 12.0f, 16.0f, ETuningLatch::Use, "Cm", false, false },
		{ "BoomRecoverHoldSec", 0.15f, 0.0f, 0.5f, 0.1f, 0.2f, ETuningLatch::Use, "Sec", false, false },
		{ "BoomRecoverTauSec", 0.3f, 0.1f, 0.6f, 0.25f, 0.4f, ETuningLatch::Use, "Sec", false, false },
		{ "BoomRecoverCmPerSec", 300.0f, 100.0f, 800.0f, 200.0f, 450.0f, ETuningLatch::Use, "CmPerSec", false, false },
		{ "ShoulderSqueezeInSec", 0.15f, 0.05f, 0.5f, 0.1f, 0.2f, ETuningLatch::Use, "Sec", false, false },
		{ "ShoulderSqueezeOutSec", 0.4f, 0.1f, 1.0f, 0.3f, 0.5f, ETuningLatch::Use, "Sec", false, false },
		{ "BoomCloseCm", 60.0f, 30.0f, 100.0f, 50.0f, 80.0f, ETuningLatch::Use, "Cm", false, false },
		{ "ConcealedBoomCm", 60.0f, 0.0f, 70.0f, 40.0f, 60.0f, ETuningLatch::Use, "Cm", false, false },
		{ "FpForwardOffsetCm", 8.0f, 0.0f, 20.0f, 5.0f, 12.0f, ETuningLatch::Use, "Cm", false, false },
		{ "CameraMaskOffHysteresisCm", 10.0f, 5.0f, 30.0f, 8.0f, 15.0f, ETuningLatch::Use, "Cm", false, false },
		{ "CameraNearClipCm", 5.0f, 1.0f, 15.0f, 3.0f, 8.0f, ETuningLatch::Use, "Cm", false, false },
		{ "EyeConeHalfHDeg", 55.0f, 40.0f, 70.0f, 54.0f, 60.0f, ETuningLatch::Round, "Deg", false, false },
		{ "EyeConeHalfVDeg", 38.0f, 25.0f, 50.0f, 38.0f, 42.0f, ETuningLatch::Round, "Deg", false, false },
		{ "EyeMaskHideFailCount", 2.0f, 1.0f, 4.0f, 2.0f, 3.0f, ETuningLatch::Use, "Count", true, false },
		{ "EyeMaskFadeSec", 0.1f, 0.05f, 0.3f, 0.08f, 0.12f, ETuningLatch::Use, "Sec", false, false },
		{ "EyeMaskNearCm", 2000.0f, 1000.0f, 4000.0f, 1000.0f, 4000.0f, ETuningLatch::Use, "Cm", false, false },
		{ "EyeMaskRangeCm", 6000.0f, 3000.0f, 10000.0f, 3000.0f, 10000.0f, ETuningLatch::Use, "Cm", false, false },
		{ "EyeMaskNearHz", 60.0f, 30.0f, 60.0f, 45.0f, 60.0f, ETuningLatch::Use, "Hz", false, false },
		{ "EyeMaskFarHz", 15.0f, 10.0f, 30.0f, 12.0f, 20.0f, ETuningLatch::Use, "Hz", false, false },
		{ "EyeMaskBeyondHz", 5.0f, 2.0f, 15.0f, 4.0f, 8.0f, ETuningLatch::Use, "Hz", false, false },
		{ "CameraToggleBlendSec", 0.2f, 0.0f, 0.28f, 0.15f, 0.25f, ETuningLatch::Use, "Sec", false, false },
		{ "ShoulderSwapBlendSec", 0.25f, 0.0f, 0.5f, 0.2f, 0.35f, ETuningLatch::Use, "Sec", false, false },
		{ "NoPawnFadeSec", 0.5f, 0.0f, 1.5f, 0.3f, 0.8f, ETuningLatch::Use, "Sec", false, false },
		{ "FovVMinDeg", 55.0f, 45.0f, 90.0f, 55.0f, 55.0f, ETuningLatch::Round, "Deg", false, false },
		{ "FovVMaxDeg", 75.0f, 45.0f, 90.0f, 75.0f, 75.0f, ETuningLatch::Round, "Deg", false, false },
		{ "FovHMaxDeg", 108.0f, 90.0f, 120.0f, 100.0f, 110.0f, ETuningLatch::Round, "Deg", false, false },
		{ "StickOuterZoneFrac", 0.98f, 0.9f, 1.0f, 0.95f, 0.99f, ETuningLatch::Use, "Frac", false, false },
	};

	/** The user settings marked * in the GDD: never schema keys (TDC CR-14). */
	inline constexpr const ANSICHAR* UserSettingNames[] = { "FpFovVDeg", "TpFovVDeg", "StickDeadZoneFrac", "StickCurveExp", "StickYawRateDegPerSec", "StickPitchRateDegPerSec" };
}
