#pragma once

#include "CoreMinimal.h"
#include "View/StealthClientTuning.h"
#include "View/StealthTuningPublicSubstructs.h"

/**
 * The plain camera parameter set (ADR-0015 sections 2 and 7; GDD camera-system.md Tuning Knobs; story cs-001). No UObject:
 * the cores (FViewEnvelope story cs-002, FStealthCameraCore cs-006, FOV cs-010, FEyeMaskCore cs-012) take it by argument,
 * built once from the frozen client snapshot's FCameraTuning_Public plus the consumed keys other systems own. Values are in
 * the units the GDD writes (degrees, centimetres, seconds, hertz).
 *
 * Usage:
 *   const FStealthCameraParams Params = FStealthCameraParams::FromClientTuning(ClientTuning); // or ::Defaults()
 */

/** Keys the camera reads but does not own (SCL, IIF, Assassination, Penalty, Concealment; GDD "Consumed, not owned"). */
struct FStealthCameraConsumedKeys
{
	/** SCL LookYawLimitDeg: the view yaw limit relative to body facing. */
	double LookYawLimitDeg = 0.0;
	/** SCL LookPitchLimitDeg (the replicated head pitch; the local view pitch is Camera's). */
	double LookPitchLimitDeg = 0.0;
	/** SCL TurnInPlaceDelaySec. */
	double TurnInPlaceDelaySec = 0.0;
	/** SCL PawnSmoothSnapThresholdCm. */
	double PawnSmoothSnapThresholdCm = 0.0;
	/** IIF CameraToggleLockoutSec (I-CS-1's other side; IIF has not registered it: its GDD default). */
	double CameraToggleLockoutSec = 0.0;
	/** Assassination DeathFallSec (the dying hold, story cs-018). */
	double DeathFallSec = 0.0;
	/** Assassination StrikeWarpYawLimitDeg. */
	double StrikeWarpYawLimitDeg = 0.0;
	/** Penalty ExileBeatSec and ExileCardTimeoutSec (the exile view, story cs-018). */
	double ExileBeatSec = 0.0;
	double ExileCardTimeoutSec = 0.0;
	/** Concealment's bush dome inner radius (CON-R7, I-CS-4). */
	double BushDomeInnerRadiusCm = 0.0;

	/** IIF's CameraToggleLockoutSec default (pending its schema row; the server-side twin is StealthTuningPendingOwnerKeys). */
	static constexpr double PendingCameraToggleLockoutSec = 0.30;
	/** Concealment CON-R7's 1.8 m leaf dome: inner radius. */
	static constexpr double BushDomeInnerRadiusDefaultCm = 90.0;

	/** The consumed keys from the client snapshot. */
	static FStealthCameraConsumedKeys FromClientTuning(const FStealthClientTuning& Tuning)
	{
		FStealthCameraConsumedKeys K;
		K.LookYawLimitDeg = Tuning.Locomotion_Public.LookYawLimitDeg;
		K.LookPitchLimitDeg = Tuning.Locomotion_Public.LookPitchLimitDeg;
		K.TurnInPlaceDelaySec = Tuning.Locomotion_Public.TurnInPlaceDelaySec;
		K.PawnSmoothSnapThresholdCm = Tuning.Locomotion_Public.PawnSmoothSnapThresholdCm;
		K.CameraToggleLockoutSec = PendingCameraToggleLockoutSec;
		K.DeathFallSec = Tuning.Assassination_Public.DeathFallSec;
		K.StrikeWarpYawLimitDeg = Tuning.Assassination_Public.StrikeWarpYawLimitDeg;
		K.ExileBeatSec = Tuning.Penalty_Public.ExileBeatSec;
		K.ExileCardTimeoutSec = Tuning.Penalty_Public.ExileCardTimeoutSec;
		K.BushDomeInnerRadiusCm = BushDomeInnerRadiusDefaultCm;
		return K;
	}
};

struct FStealthCameraParams
{
	// Envelope (F1)
	double ViewHeadLeadMaxDeg = 0.0;
	double ViewPitchDownLimitDeg = 60.0;
	double ViewPitchUpLimitDeg = 40.0;
	double CameraEdgePushDeg = 5.0;
	double EdgePushMinRateDegPerSec = 15.0;
	double EdgePushLatchSec = 0.15;
	double CameraEdgeSoftMinGain = 0.1;
	double EdgePushRelaxDegPerSec = 60.0;
	double CameraEdgeSoftYawDeg = 10.0;
	double CameraEdgeSoftPitchDeg = 5.0;
	// Pivot and boom (F2)
	double CameraPivotLagSec = 0.10;
	double CameraPivotLagTraversalSec = 0.12;
	double PivotMaxVertCmPerSec = 300.0;
	double BoomLengthCm = 180.0;
	double ShoulderOffsetCm = 45.0;
	double BoomHeightOffsetCm = 15.0;
	double CameraGuardRadiusCm = 2.0;
	double CameraProbeRadiusCm = 14.0;
	double BoomRecoverHoldSec = 0.15;
	double BoomRecoverTauSec = 0.30;
	double BoomRecoverCmPerSec = 300.0;
	double ShoulderSqueezeInSec = 0.15;
	double ShoulderSqueezeOutSec = 0.40;
	double BoomCloseCm = 60.0;
	double ConcealedBoomCm = 60.0;
	double FpForwardOffsetCm = 8.0;
	double CameraMaskOffHysteresisCm = 10.0;
	double CameraNearClipCm = 5.0;
	// Eye mask (F3)
	double EyeConeHalfHDeg = 55.0;
	double EyeConeHalfVDeg = 38.0;
	int32 EyeMaskHideFailCount = 2;
	double EyeMaskFadeSec = 0.10;
	double EyeMaskNearCm = 2000.0;
	double EyeMaskRangeCm = 6000.0;
	double EyeMaskNearHz = 60.0;
	double EyeMaskFarHz = 15.0;
	double EyeMaskBeyondHz = 5.0;
	// Toggle, swap, fades (F4, CS-R13)
	double CameraToggleBlendSec = 0.20;
	double ShoulderSwapBlendSec = 0.25;
	double NoPawnFadeSec = 0.5;
	// Field of view (F5)
	double FovVMinDeg = 55.0;
	double FovVMaxDeg = 75.0;
	double FovHMaxDeg = 108.0;
	// Stick (F6; the rest are user settings)
	double StickOuterZoneFrac = 0.98;
	/** The consumed keys. */
	FStealthCameraConsumedKeys Consumed;

	/** True when ViewHeadLeadMaxDeg holds its "disabled" sentinel (the CS-R3.5 fallback is off). */
	bool IsHeadLeadClampEnabled() const { return ViewHeadLeadMaxDeg > 0.0; }

	/** The parameters of the client snapshot (the camera keys and the consumed keys). */
	static FStealthCameraParams FromClientTuning(const FStealthClientTuning& Client)
	{
		return FromTuning(Client.Camera_Public, FStealthCameraConsumedKeys::FromClientTuning(Client));
	}

	/** The schema defaults (until the client tuning store feeds the camera). */
	static FStealthCameraParams Defaults() { return FromClientTuning(FStealthClientTuning()); }

	/** The parameters of a tuning face and the consumed keys. */
	static FStealthCameraParams FromTuning(const FCameraTuning_Public& Tuning, const FStealthCameraConsumedKeys& InConsumed)
	{
		FStealthCameraParams P;
		P.ViewHeadLeadMaxDeg = Tuning.ViewHeadLeadMaxDeg;
		P.ViewPitchDownLimitDeg = Tuning.ViewPitchDownLimitDeg;
		P.ViewPitchUpLimitDeg = Tuning.ViewPitchUpLimitDeg;
		P.CameraEdgePushDeg = Tuning.CameraEdgePushDeg;
		P.EdgePushMinRateDegPerSec = Tuning.EdgePushMinRateDegPerSec;
		P.EdgePushLatchSec = Tuning.EdgePushLatchSec;
		P.CameraEdgeSoftMinGain = Tuning.CameraEdgeSoftMinGain;
		P.EdgePushRelaxDegPerSec = Tuning.EdgePushRelaxDegPerSec;
		P.CameraEdgeSoftYawDeg = Tuning.CameraEdgeSoftYawDeg;
		P.CameraEdgeSoftPitchDeg = Tuning.CameraEdgeSoftPitchDeg;
		P.CameraPivotLagSec = Tuning.CameraPivotLagSec;
		P.CameraPivotLagTraversalSec = Tuning.CameraPivotLagTraversalSec;
		P.PivotMaxVertCmPerSec = Tuning.PivotMaxVertCmPerSec;
		P.BoomLengthCm = Tuning.BoomLengthCm;
		P.ShoulderOffsetCm = Tuning.ShoulderOffsetCm;
		P.BoomHeightOffsetCm = Tuning.BoomHeightOffsetCm;
		P.CameraGuardRadiusCm = Tuning.CameraGuardRadiusCm;
		P.CameraProbeRadiusCm = Tuning.CameraProbeRadiusCm;
		P.BoomRecoverHoldSec = Tuning.BoomRecoverHoldSec;
		P.BoomRecoverTauSec = Tuning.BoomRecoverTauSec;
		P.BoomRecoverCmPerSec = Tuning.BoomRecoverCmPerSec;
		P.ShoulderSqueezeInSec = Tuning.ShoulderSqueezeInSec;
		P.ShoulderSqueezeOutSec = Tuning.ShoulderSqueezeOutSec;
		P.BoomCloseCm = Tuning.BoomCloseCm;
		P.ConcealedBoomCm = Tuning.ConcealedBoomCm;
		P.FpForwardOffsetCm = Tuning.FpForwardOffsetCm;
		P.CameraMaskOffHysteresisCm = Tuning.CameraMaskOffHysteresisCm;
		P.CameraNearClipCm = Tuning.CameraNearClipCm;
		P.EyeConeHalfHDeg = Tuning.EyeConeHalfHDeg;
		P.EyeConeHalfVDeg = Tuning.EyeConeHalfVDeg;
		P.EyeMaskHideFailCount = Tuning.EyeMaskHideFailCount;
		P.EyeMaskFadeSec = Tuning.EyeMaskFadeSec;
		P.EyeMaskNearCm = Tuning.EyeMaskNearCm;
		P.EyeMaskRangeCm = Tuning.EyeMaskRangeCm;
		P.EyeMaskNearHz = Tuning.EyeMaskNearHz;
		P.EyeMaskFarHz = Tuning.EyeMaskFarHz;
		P.EyeMaskBeyondHz = Tuning.EyeMaskBeyondHz;
		P.CameraToggleBlendSec = Tuning.CameraToggleBlendSec;
		P.ShoulderSwapBlendSec = Tuning.ShoulderSwapBlendSec;
		P.NoPawnFadeSec = Tuning.NoPawnFadeSec;
		P.FovVMinDeg = Tuning.FovVMinDeg;
		P.FovVMaxDeg = Tuning.FovVMaxDeg;
		P.FovHMaxDeg = Tuning.FovHMaxDeg;
		P.StickOuterZoneFrac = Tuning.StickOuterZoneFrac;
		P.Consumed = InConsumed;
		return P;
	}
};

/**
 * The six camera user settings (GDD Tuning Knobs rows marked *): local settings, never tuning keys (TDC CR-14; stored by the
 * local settings store, story tdc-093). This struct registers their hard ranges and defaults; Clamped() is the only repair a
 * user setting gets (a tuning key is never clamped at runtime).
 */
struct FStealthCameraUserSettings
{
	/** Hard ranges and defaults (GDD Tuning Knobs). */
	static constexpr double FovVDefaultDeg = 60.0;
	static constexpr double FovVMinDeg = 55.0;
	static constexpr double FovVMaxDeg = 75.0;
	static constexpr double StickDeadZoneDefault = 0.15;
	static constexpr double StickDeadZoneMin = 0.05;
	static constexpr double StickDeadZoneMax = 0.40;
	static constexpr double StickCurveExpDefault = 1.5;
	static constexpr double StickCurveExpMin = 1.0;
	static constexpr double StickCurveExpMax = 2.0;
	static constexpr double StickYawRateDefault = 180.0;
	static constexpr double StickPitchRateDefault = 120.0;
	static constexpr double StickRateMin = 60.0;
	static constexpr double StickRateMax = 360.0;

	double FpFovVDeg = FovVDefaultDeg;
	double TpFovVDeg = FovVDefaultDeg;
	double StickDeadZoneFrac = StickDeadZoneDefault;
	double StickCurveExp = StickCurveExpDefault;
	double StickYawRateDegPerSec = StickYawRateDefault;
	double StickPitchRateDegPerSec = StickPitchRateDefault;

	/** Every setting clamped to its hard range (a non-finite value takes its default). */
	FStealthCameraUserSettings Clamped() const
	{
		auto Clamp = [](double Value, double Min, double Max, double Default) { return FMath::IsFinite(Value) ? FMath::Clamp(Value, Min, Max) : Default; };
		FStealthCameraUserSettings Out;
		Out.FpFovVDeg = Clamp(FpFovVDeg, FovVMinDeg, FovVMaxDeg, FovVDefaultDeg);
		Out.TpFovVDeg = Clamp(TpFovVDeg, FovVMinDeg, FovVMaxDeg, FovVDefaultDeg);
		Out.StickDeadZoneFrac = Clamp(StickDeadZoneFrac, StickDeadZoneMin, StickDeadZoneMax, StickDeadZoneDefault);
		Out.StickCurveExp = Clamp(StickCurveExp, StickCurveExpMin, StickCurveExpMax, StickCurveExpDefault);
		Out.StickYawRateDegPerSec = Clamp(StickYawRateDegPerSec, StickRateMin, StickRateMax, StickYawRateDefault);
		Out.StickPitchRateDegPerSec = Clamp(StickPitchRateDegPerSec, StickRateMin, StickRateMax, StickPitchRateDefault);
		return Out;
	}
};
