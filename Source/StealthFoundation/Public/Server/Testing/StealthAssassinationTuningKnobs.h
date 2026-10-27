#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

/**
 * The Assassination & Defence Reversal Tuning Knobs table as parsed fixture data (GDD
 * design/gdd/assassination-defence-reversal.md, Tuning Knobs, read 2026-10-27; story asn-001, AC1 `AssassinationKeyTable`).
 * Committed constants transcribed from the GDD table: StealthGame.Assassination.TuningInvariants compares them with the
 * generated schema rows, and StealthGame.Tuning.SchemaComplete pins the defaults. Units are the CR-15.2 name suffixes.
 */
namespace StealthAssassinationTuningKnobs
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

	/** The owner id every Assassination row carries. */
	inline constexpr const ANSICHAR* Owner = "Assassination";

	/** Every Tuning Knobs row, in the GDD table's order (all float keys). */
	inline constexpr FKnob Knobs[] = {
		{ "AssassinationRangeCm", 200.0f, 100.0f, 250.0f, 150.0f, 250.0f, true, ETuningLatch::Use, "Cm" },
		{ "StrikeBearingLimitDeg", 90.0f, 60.0f, 105.0f, 80.0f, 90.0f, true, ETuningLatch::Use, "Deg" },
		{ "DefenceDurationSec", 4.0f, 3.0f, 5.0f, 3.0f, 5.0f, true, ETuningLatch::Action, "Sec" },
		{ "DefenceCooldownSec", 60.0f, 30.0f, 120.0f, 45.0f, 75.0f, true, ETuningLatch::Action, "Sec" },
		{ "StrikeVerticalReachCm", 75.0f, 30.0f, 120.0f, 50.0f, 100.0f, false, ETuningLatch::Use, "Cm" },
		{ "StrikeContactSec", 0.40f, 0.25f, 0.80f, 0.30f, 0.50f, true, ETuningLatch::Action, "Sec" },
		{ "StrikeWarpLimitCm", 240.0f, 150.0f, 240.0f, 200.0f, 240.0f, true, ETuningLatch::Use, "Cm" },
		{ "StrikeContactGapCm", 15.0f, 5.0f, 40.0f, 10.0f, 25.0f, true, ETuningLatch::Use, "Cm" },
		{ "StrikeWarpYawLimitDeg", 30.0f, 10.0f, 45.0f, 20.0f, 35.0f, true, ETuningLatch::Use, "Deg" },
		{ "AttackAttemptLockoutSec", 1.75f, 1.2f, 3.0f, 1.5f, 2.0f, true, ETuningLatch::Action, "Sec" },
		{ "OpenSpotToleranceCm", 75.0f, 20.0f, 200.0f, 40.0f, 120.0f, false, ETuningLatch::Round, "Cm" },
		{ "CounterDurationSec", 1.2f, 0.8f, 2.0f, 1.0f, 1.5f, true, ETuningLatch::Action, "Sec" },
		{ "DeathFallSec", 1.2f, 0.8f, 2.0f, 1.1f, 1.6f, true, ETuningLatch::Action, "Sec" },
		{ "BodyLingerSec", 6.0f, 2.0f, 12.0f, 4.0f, 8.0f, true, ETuningLatch::Action, "Sec" },
		{ "BodyRemovalGraceSec", 2.0f, 0.0f, 2.5f, 1.5f, 2.5f, true, ETuningLatch::Action, "Sec" },
		{ "StrikeNetLeadSec", 0.15f, 0.05f, 0.30f, 0.10f, 0.15f, false, ETuningLatch::Round, "Sec" },
	};

	/** The three TDC canonical keys of this owner (already pinned by the TDC Canonical Tunables). */
	inline constexpr const ANSICHAR* CanonicalKeys[] = { "AssassinationRangeCm", "DefenceDurationSec", "DefenceCooldownSec" };

	/** Keys read by the Assassination invariants but owned elsewhere: never registered under this owner. */
	inline constexpr const ANSICHAR* ForeignKeys[] = { "BushStrikeEmergeSec", "BushSpotToleranceCm", "NpcKillStrikeLimit", "CrowdReplacementMinDelaySec",
		"InformantRespawnDelaySec", "ReleaseDebounceSec", "PawnRefAgreementCm" };
}
