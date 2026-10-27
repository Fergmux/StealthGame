#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

/**
 * The NPC-Kill Penalty & Informants Tuning Knobs rows registered in the schema so far, as committed fixture data (GDD
 * design/gdd/npc-kill-penalty-informants.md, Tuning Knobs, read 2026-10-27; story nkp-002). StealthGame.Tuning.SchemaComplete
 * pins their defaults. NpcKillStrikeLimit is a TDC canonical key (already pinned there); InformantRespawnDelaySec,
 * ExileBeatSec and ExileCardTimeoutSec are registered by the stories that first read them (story 009 registered
 * InformantRespawnDelaySec; 017 the other two).
 */
namespace StealthPenaltyTuningKnobs
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
	};

	/** The owner id every Penalty row carries. */
	inline constexpr const ANSICHAR* Owner = "Penalty";

	/** The registered Penalty rows that are not TDC canonical keys. */
	inline constexpr FKnob Knobs[] = {
		{ "InformantRespawnDelaySec", 15.0f, 5.0f, 60.0f, 10.0f, 20.0f, true, ETuningLatch::Round, "Sec" },
		{ "StrikeOutReleaseGraceSec", 2.0f, 0.0f, 5.0f, 1.75f, 2.5f, false, ETuningLatch::Round, "Sec" },
	};
}
