#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

/**
 * The Concealment (Bushes) Tuning Knobs rows, as committed fixture data copied from the GDD table (design/gdd/
 * concealment-bushes.md, Tuning Knobs, read 2026-10-28; story con-001). Never derived from the rows under test:
 * StealthGame.Concealment.TuningRows compares the compiled schema against this table and StealthGame.Tuning.SchemaComplete
 * pins the defaults. BushVictimMode is the first enum key: its default and ranges are value indices (UniformHash 0,
 * EarliestCommitted 1, All 2), its unit "Mode" (the GDD writes "-").
 */
namespace StealthConcealmentTuningKnobs
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
		const ANSICHAR* Unit;
		/** The key is an enum (value indices). */
		bool bEnum;
	};

	/** The owner id every Concealment row carries. */
	inline constexpr const ANSICHAR* Owner = "Concealment";

	/** The eleven Concealment-owned keys (GDD Tuning Knobs table order). */
	inline constexpr FKnob Knobs[] = {
		{ "BushEnterDurSec", 0.9f, 0.6f, 1.5f, 0.8f, 1.3f, true, ETuningLatch::Round, "Sec", false },
		{ "BushEnterSwapSec", 0.5f, 0.3f, 0.8f, 0.46f, 0.65f, true, ETuningLatch::Round, "Sec", false },
		{ "BushExitDurSec", 0.9f, 0.6f, 1.5f, 0.8f, 1.3f, true, ETuningLatch::Round, "Sec", false },
		{ "BushExitCrossSec", 0.45f, 0.3f, 0.8f, 0.35f, 0.7f, true, ETuningLatch::Round, "Sec", false },
		{ "BushStrikeEmergeSec", 0.35f, 0.3f, 0.6f, 0.3f, 0.5f, true, ETuningLatch::Round, "Sec", false },
		{ "BushEntryReachCm", 60.0f, 20.0f, 150.0f, 30.0f, 70.0f, true, ETuningLatch::Round, "Cm", false },
		{ "BushSpotToleranceCm", 75.0f, 20.0f, 200.0f, 40.0f, 120.0f, false, ETuningLatch::Round, "Cm", false },
		{ "BushReenterCooldownSec", 6.0f, 0.0f, 30.0f, 3.0f, 15.0f, false, ETuningLatch::Round, "Sec", false },
		{ "BushExitDepenetrationCapCm", 30.0f, 0.0f, 60.0f, 15.0f, 40.0f, false, ETuningLatch::Round, "Cm", false },
		{ "BushVictimMode", 0.0f, 0.0f, 2.0f, 0.0f, 2.0f, false, ETuningLatch::Round, "Mode", true },
		{ "BushRustleAmplitudeCm", 10.0f, 4.0f, 20.0f, 8.0f, 12.0f, true, ETuningLatch::Use, "Cm", false },
	};

	/** The four ServerOnly key names (absent from every client module). */
	inline constexpr const ANSICHAR* ServerFaceKeys[] = { "BushSpotToleranceCm", "BushReenterCooldownSec", "BushExitDepenetrationCapCm", "BushVictimMode" };

	/** Words no Concealment key may contain, case-insensitive (P4; AC-CON-12 whitelist). */
	inline constexpr const ANSICHAR* ForbiddenWords[] = { "dwell", "capacity", "stay", "reveal", "timeout" };
}
