#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

/**
 * The Objectives Chain Tuning Knobs rows, as committed fixture data copied from the GDD table (design/gdd/objectives-chain.md,
 * Tuning Knobs, read 2026-10-28; story oc-001). Never derived from the rows under test: StealthGame.Objectives.ObjectivesTuningRows
 * compares the compiled schema against this table and StealthGame.Tuning.SchemaComplete pins the defaults.
 * ObjectiveInteractSec is the TDC canonical key (story tdc-040): it resolves to the TDC row, is not redeclared and is not
 * part of the ten Objectives-owned keys below (CanonicalKey names it separately).
 */
namespace StealthObjectivesTuningKnobs
{
	struct FKnob
	{
		const ANSICHAR* Key;
		float Default;
		float HardMin;
		float HardMax;
		float SafeMin;
		float SafeMax;
		/** Audience All (true) or ServerOnly (false). */
		bool bPublic;
		ETuningLatch Latch;
	};

	/** The owner id every Objectives row carries. */
	inline constexpr const ANSICHAR* Owner = "Objectives";

	/** The TDC canonical Objectives key (registered once, by TDC story 040). */
	inline constexpr const ANSICHAR* CanonicalKey = "ObjectiveInteractSec";

	/** The ten Objectives-owned keys (a bool row's ranges are 0-1, as the generator writes them). */
	inline constexpr FKnob Knobs[] = {
		{ "TendCueDelaySec", 1.0f, 0.5f, 2.0f, 0.8f, 1.5f, true, ETuningLatch::Action },
		{ "FixtureInteractSideCount", 4.0f, 1.0f, 4.0f, 3.0f, 4.0f, true, ETuningLatch::Round },
		{ "ObjectivesPerStageCount", 1.0f, 1.0f, 3.0f, 1.0f, 1.0f, false, ETuningLatch::Round },
		{ "bRivalSignalEnabled", 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, false, ETuningLatch::Round },
		{ "RivalSignalDelayMinSec", 30.0f, 30.0f, 120.0f, 30.0f, 60.0f, false, ETuningLatch::Round },
		{ "RivalSignalDelayMaxSec", 165.0f, 70.0f, 300.0f, 163.0f, 200.0f, false, ETuningLatch::Round },
		{ "RivalSignalReleaseQuantumSec", 30.0f, 10.0f, 90.0f, 28.0f, 45.0f, false, ETuningLatch::Round },
		{ "ClueMatchMaxCount", 3.0f, 1.0f, 6.0f, 2.0f, 3.0f, false, ETuningLatch::Round },
		{ "ObjectiveStallGapDefectSec", 300.0f, 120.0f, 900.0f, 240.0f, 420.0f, false, ETuningLatch::Use },
		{ "ObjectiveFinalDwellDefectSec", 240.0f, 90.0f, 600.0f, 180.0f, 360.0f, false, ETuningLatch::Use },
	};

	/** The eight ServerOnly key names (the client-module grep of story oc-001 AC4 searches for each). */
	inline constexpr const ANSICHAR* ServerFaceKeys[] = {
		"ObjectivesPerStageCount", "bRivalSignalEnabled", "RivalSignalDelayMinSec", "RivalSignalDelayMaxSec",
		"RivalSignalReleaseQuantumSec", "ClueMatchMaxCount", "ObjectiveStallGapDefectSec", "ObjectiveFinalDwellDefectSec",
	};
}
