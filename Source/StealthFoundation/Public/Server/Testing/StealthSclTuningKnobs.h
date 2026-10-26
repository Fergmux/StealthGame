#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

/**
 * The Shared Character & Locomotion Tuning Knobs table as parsed fixture data (GDD design/gdd/shared-character-locomotion.md,
 * Tuning Knobs, re-read 2026-10-26; story SCL-003, AC-SCL-35). Committed constants copied from the GDD table: the
 * KeysRegistered lint compares them with the generated schema rows, and SchemaComplete pins the defaults.
 *
 * Unit column note: the schema's Unit is the CR-15.2 name suffix, so three GDD Unit cells map to the suffix the key name
 * ends with (IdleGazeLookDeadzoneDegPerSec -> PerSec, PawnNetUpdateHz -> Hz, TurnInPlaceExitFrac -> Frac).
 */
namespace StealthSclTuningKnobs
{
	struct FKnob
	{
		const ANSICHAR* Key;
		bool bInt;
		float Default;
		float HardMin;
		float HardMax;
		float SafeMin;
		float SafeMax;
		/** Audience Client (true) or ServerOnly (false). */
		bool bPublic;
		ETuningLatch Latch;
		const ANSICHAR* Unit;
	};

	/** The owner id every SCL row carries. */
	inline constexpr const ANSICHAR* Owner = "SCL";

	/** Every Tuning Knobs row, Client keys first. */
	inline constexpr FKnob Knobs[] = {
		{ "WalkSpeedCmPerSec", false, 140.0f, 100.0f, 200.0f, 131.0f, 160.0f, true, ETuningLatch::Round, "CmPerSec" },
		{ "WalkSpeedScaleMinMult", false, 0.80f, 0.6f, 1.0f, 0.75f, 0.80f, true, ETuningLatch::Round, "Mult" },
		{ "RunSpeedCmPerSec", false, 450.0f, 300.0f, 650.0f, 400.0f, 500.0f, true, ETuningLatch::Round, "CmPerSec" },
		{ "WalkStartSec", false, 0.40f, 0.2f, 1.0f, 0.3f, 0.5f, true, ETuningLatch::Round, "Sec" },
		{ "WalkStopSec", false, 0.60f, 0.5f, 1.2f, 0.5f, 0.6f, true, ETuningLatch::Round, "Sec" },
		{ "StopMinSec", false, 0.5f, 0.2f, 1.0f, 0.4f, 0.6f, true, ETuningLatch::Round, "Sec" },
		{ "RunStartSec", false, 0.60f, 0.3f, 1.5f, 0.5f, 0.8f, true, ETuningLatch::Round, "Sec" },
		{ "RunStopSec", false, 0.75f, 0.4f, 1.5f, 0.6f, 0.9f, true, ETuningLatch::Round, "Sec" },
		{ "WalkTurnRateDegPerSec", false, 60.0f, 30.0f, 120.0f, 50.0f, 64.0f, true, ETuningLatch::Round, "PerSec" },
		{ "RunTurnRateDegPerSec", false, 180.0f, 90.0f, 360.0f, 150.0f, 220.0f, true, ETuningLatch::Round, "PerSec" },
		{ "PivotTurnRateDegPerSec", false, 120.0f, 60.0f, 180.0f, 90.0f, 150.0f, true, ETuningLatch::Round, "PerSec" },
		{ "PivotSpeedCmPerSec", false, 20.0f, 5.0f, 50.0f, 15.0f, 30.0f, true, ETuningLatch::Round, "CmPerSec" },
		{ "ReversalAngleDeg", false, 120.0f, 90.0f, 170.0f, 110.0f, 135.0f, true, ETuningLatch::Round, "Deg" },
		{ "LookYawLimitDeg", false, 70.0f, 30.0f, 90.0f, 60.0f, 75.0f, true, ETuningLatch::Round, "Deg" },
		{ "LookPitchLimitDeg", false, 20.0f, 10.0f, 45.0f, 15.0f, 25.0f, true, ETuningLatch::Round, "Deg" },
		{ "LookMaxRateDegPerSec", false, 140.0f, 60.0f, 300.0f, 120.0f, 160.0f, true, ETuningLatch::Round, "PerSec" },
		{ "LookEaseSec", false, 0.15f, 0.0f, 0.5f, 0.1f, 0.25f, true, ETuningLatch::Round, "Sec" },
		{ "TurnInPlaceDelaySec", false, 0.50f, 0.1f, 2.0f, 0.3f, 0.8f, true, ETuningLatch::Round, "Sec" },
		{ "TraversalVaultHeightCm", false, 80.0f, 60.0f, 120.0f, 75.0f, 85.0f, true, ETuningLatch::Round, "Cm" },
		{ "TraversalClimbHeightCm", false, 160.0f, 120.0f, 200.0f, 150.0f, 170.0f, true, ETuningLatch::Round, "Cm" },
		{ "TraversalHurdleHeightCm", false, 120.0f, 100.0f, 140.0f, 115.0f, 125.0f, true, ETuningLatch::Round, "Cm" }, // spike SCL-045 (not yet in the GDD table when written; GDD updated with it)
		{ "TraversalMinHeightCm", false, 45.0f, 30.0f, 50.0f, 40.0f, 50.0f, true, ETuningLatch::Round, "Cm" },
		{ "TraversalHeightToleranceCm", false, 10.0f, 2.0f, 20.0f, 5.0f, 12.0f, true, ETuningLatch::Round, "Cm" },
		{ "TraversalReachCm", false, 100.0f, 50.0f, 200.0f, 80.0f, 120.0f, true, ETuningLatch::Round, "Cm" },
		{ "TraversalFacingHalfAngleDeg", false, 45.0f, 20.0f, 80.0f, 35.0f, 55.0f, true, ETuningLatch::Round, "Deg" },
		{ "TraversalAnticipationSec", false, 0.15f, 0.0f, 0.3f, 0.1f, 0.2f, true, ETuningLatch::Round, "Sec" },
		{ "PawnSmoothSnapThresholdCm", false, 256.0f, 128.0f, 512.0f, 200.0f, 300.0f, true, ETuningLatch::Round, "Cm" },
		{ "PawnSmoothLocationSec", false, 0.10f, 0.07f, 0.25f, 0.08f, 0.12f, true, ETuningLatch::Round, "Sec" },
		{ "AnimTierAMinScreenPct", false, 19.0f, 10.0f, 30.0f, 17.0f, 21.0f, true, ETuningLatch::Round, "Pct" },
		{ "AnimTierBMinScreenPct", false, 8.0f, 4.0f, 15.0f, 7.0f, 10.0f, true, ETuningLatch::Round, "Pct" },
		{ "AnimTierCMinScreenPct", false, 3.0f, 1.0f, 6.0f, 2.5f, 4.0f, true, ETuningLatch::Round, "Pct" },
		{ "AnimTierHysteresisPct", false, 8.0f, 0.0f, 20.0f, 5.0f, 10.0f, true, ETuningLatch::Round, "Pct" },
		{ "AnimTierMinHoldSec", false, 0.5f, 0.0f, 2.0f, 0.3f, 0.8f, true, ETuningLatch::Round, "Sec" },
		{ "AnimTierACapCount", true, 8.0f, 4.0f, 12.0f, 6.0f, 10.0f, true, ETuningLatch::Round, "Count" },
		{ "AnimTierBCapCount", true, 14.0f, 8.0f, 24.0f, 12.0f, 16.0f, true, ETuningLatch::Round, "Count" },
		{ "AnimTierCCapCount", true, 28.0f, 20.0f, 50.0f, 24.0f, 32.0f, true, ETuningLatch::Round, "Count" },
		{ "AnimSearchIntervalTierBSec", false, 0.067f, 0.033f, 0.2f, 0.05f, 0.1f, true, ETuningLatch::Round, "Sec" },
		{ "FootstepCadenceSlowPerSec", false, 1.8f, 1.6f, 2.0f, 1.75f, 1.9f, true, ETuningLatch::Round, "PerSec" },
		{ "FootstepCadenceFullPerSec", false, 2.0f, 1.8f, 2.2f, 1.9f, 2.05f, true, ETuningLatch::Round, "PerSec" },
		{ "FootstepStartDelaySec", false, 0.30f, 0.2f, 0.5f, 0.25f, 0.4f, true, ETuningLatch::Round, "Sec" },
		{ "FootstepStrideRunCm", false, 130.0f, 90.0f, 180.0f, 115.0f, 145.0f, true, ETuningLatch::Round, "Cm" },
		{ "PivotExitAngleDeg", false, 10.0f, 5.0f, 20.0f, 8.0f, 12.0f, true, ETuningLatch::Round, "Deg" },
		{ "ReversalHysteresisDeg", false, 10.0f, 5.0f, 20.0f, 8.0f, 15.0f, true, ETuningLatch::Round, "Deg" },
		{ "TurnInPlaceExitFrac", false, 0.5f, 0.2f, 0.8f, 0.4f, 0.6f, true, ETuningLatch::Round, "Frac" },
		{ "AnimSearchIntervalTierCSec", false, 0.133f, 0.067f, 0.3f, 0.1f, 0.17f, true, ETuningLatch::Round, "Sec" },
		{ "AnimTierDVisibleMinHz", false, 8.0f, 6.0f, 15.0f, 8.0f, 10.0f, true, ETuningLatch::Round, "Hz" },
		{ "ContactBrushMinRelSpeedCmPerSec", false, 40.0f, 10.0f, 100.0f, 30.0f, 60.0f, true, ETuningLatch::Round, "CmPerSec" },
		{ "RunSpeedThresholdCmPerSec", false, 300.0f, 180.0f, 500.0f, 250.0f, 350.0f, false, ETuningLatch::Use, "CmPerSec" },
		{ "RunExitHysteresisCmPerSec", false, 25.0f, 0.0f, 100.0f, 15.0f, 40.0f, false, ETuningLatch::Use, "CmPerSec" },
		{ "IdleGazeDelaySec", false, 3.0f, 1.0f, 10.0f, 2.0f, 5.0f, false, ETuningLatch::Use, "Sec" },
		{ "IdleGazeLookDeadzoneDegPerSec", false, 0.5f, 0.0f, 3.0f, 0.2f, 1.0f, false, ETuningLatch::Use, "PerSec" },
		{ "PawnNetUpdateHz", false, 15.0f, 12.0f, 17.0f, 13.0f, 16.0f, false, ETuningLatch::Round, "Hz" },
		{ "InputStarvationSec", false, 0.25f, 0.25f, 1.0f, 0.25f, 0.4f, false, ETuningLatch::Use, "Sec" },
		{ "StarvationCatchUpMaxMult", false, 1.15f, 1.0f, 1.5f, 1.05f, 1.25f, false, ETuningLatch::Use, "Mult" },
		{ "HandoverHoldSec", false, 0.8f, 0.3f, 2.0f, 0.5f, 1.2f, false, ETuningLatch::Use, "Sec" },
	};

	/** Keys the GDD retires: they must not exist in the schema (Tuning Knobs, "Retired"; TDC migration table). */
	inline constexpr const ANSICHAR* RetiredKeys[] = { "InformantMaxSpeedCmPerSec", "FootstepStrideWalkCm" };

}
