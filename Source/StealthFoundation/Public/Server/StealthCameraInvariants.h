#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningInvariants.h"

struct FStealthServerTuning;

/**
 * The Camera System invariants I-CS-1..5 as ADR-0004 Check rows (GDD camera-system.md Tuning Knobs "Invariants", AC-CS-22;
 * ADR-0015 section 7; story cs-001). Every row is Reject: a violating set is rejected whole at preset load and the next
 * rung (Default, then the schema defaults) loads; nothing clamps or repairs a camera value at runtime (TDC CR-6). Pure.
 *
 *   I-CS-1  CameraToggleBlendSec <= CameraToggleLockoutSec - 1/60 s       (the 60 fps floor; IIF owns the lockout)
 *   I-CS-2  BoomHeightOffsetCm <= 25
 *   I-CS-3  EyeConeHalfHDeg >= FovHMaxDeg / 2  and  EyeConeHalfVDeg >= FovVMaxDeg / 2
 *   I-CS-4  ConcealedBoomCm <= bush dome inner radius - 20 cm (90 - 20 = 70)  and  ConcealedBoomCm <= BoomLengthCm
 *   I-CS-5  CameraEdgePushDeg >= 2
 *
 * I-CS-2, I-CS-4 (its 70 cm clause) and I-CS-5 coincide with the keys' hard ranges, so at preset load F-1's range gate meets
 * an out-of-range value first; the rows remain the reject backstop for a schema whose range drifts. I-CS-1 reads the IIF
 * lockout as a pending owner key (StealthTuningPendingOwnerKeys::CameraToggleLockoutSec) until IIF registers it.
 *
 * Usage (test, range gate bypassed):
 *   FCameraInvariantInputs In = FCameraInvariantInputs::FromValues(V); In.CameraToggleLockoutSec = 0.15;
 *   const ETuningCheck R = StealthCameraInvariants::CheckICs1(V, In);
 */
struct FCameraInvariantInputs
{
	/** IIF CameraToggleLockoutSec (the pending owner key until IIF registers it), seconds. */
	double CameraToggleLockoutSec = 0.30;
	/** Concealment's bush dome inner radius (CON-R7: the 1.8 m dome), cm. */
	double BushDomeInnerRadiusCm = 90.0;

	/** The inputs a preset load sees. */
	STEALTHFOUNDATION_API static FCameraInvariantInputs FromValues(const FStealthServerTuning& Values);
};

namespace StealthCameraInvariants
{
	/** One frame at the 60 fps floor (I-CS-1). */
	inline constexpr double FrameFloorSec = 1.0 / 60.0;
	/** I-CS-2's ceiling on BoomHeightOffsetCm. */
	inline constexpr double BoomHeightMaxCm = 25.0;
	/** I-CS-4: the concealed lens stays this far inside the bush dome. */
	inline constexpr double DomeClearanceCm = 20.0;
	/** I-CS-5's floor on CameraEdgePushDeg. */
	inline constexpr double EdgePushMinDeg = 2.0;
	/** Comparison tolerance (keys are floats authored in decimals). */
	inline constexpr double CompareToleranceAbs = 1e-4;

	STEALTHFOUNDATION_API ETuningCheck CheckICs1(const FStealthServerTuning& Values, const FCameraInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckICs2(const FStealthServerTuning& Values, const FCameraInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckICs3(const FStealthServerTuning& Values, const FCameraInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckICs4(const FStealthServerTuning& Values, const FCameraInvariantInputs& In);
	STEALTHFOUNDATION_API ETuningCheck CheckICs5(const FStealthServerTuning& Values, const FCameraInvariantInputs& In);

	/** The TDC table adapter: Check at the preset-load inputs. */
	template <ETuningCheck (*Check)(const FStealthServerTuning&, const FCameraInvariantInputs&)>
	ETuningCheck TableCheck(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext&)
	{
		return Check(Values, FCameraInvariantInputs::FromValues(Values));
	}
}
