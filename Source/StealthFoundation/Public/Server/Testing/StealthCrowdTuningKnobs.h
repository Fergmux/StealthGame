#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

/**
 * The Crowd & NPC Behaviour Tuning Knobs table as committed fixture data (GDD design/gdd/crowd-npc-behaviour.md, Tuning
 * Knobs, re-read 2026-10-26; story cnb-003). Every GDD row plus the OQ13 constants the story registers (R8 shares and
 * durations, reaction delay, C1 leg length, C2 short pause, G1-G6 timings and ranges, R3.2/R3.3 follower and avoidance
 * distances; hard and safe ranges of the OQ13 rows chosen by the build, decision log 2026-10-26). NpcCount is the TDC
 * canonical row (not redefined). Every key is ServerOnly (bPublic false). The CrowdSchema test compares these rows with
 * the generated schema, and SchemaComplete pins the defaults.
 *
 * The three soft-reference asset keys (CrowdMotionProfileAsset, CrowdFullStateTreeAsset, CrowdRestrictedStateTreeAsset)
 * are not numeric schema rows: the schema has no SoftRef row type yet (StealthTuningTypeTag::SoftRef is reserved), so
 * they live in FCrowdAssetKeys_Server and are validated by StealthCrowdTuning::ValidateAssetKeys (story cnb-003).
 */
namespace StealthCrowdTuningKnobs
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
		/** Audience Client (true) or ServerOnly (false): every Crowd key is ServerOnly. */
		bool bPublic;
		ETuningLatch Latch;
		const ANSICHAR* Unit;
	};

	/** The owner id every Crowd row carries. */
	inline constexpr const ANSICHAR* Owner = "Crowd";

	/** Every Crowd row (NpcCount first, then the GDD rows, then the OQ13 constants). */
	inline constexpr FKnob Knobs[] = {
		{ "NpcCount", true, 50.0f, 6.0f, 100.0f, 45.0f, 50.0f, false, ETuningLatch::Round, "Count" },
		{ "CrowdAreaWeightCausewayMult", false, 1.4f, 0.2f, 2.0f, 0.5f, 1.6f, false, ETuningLatch::Round, "Mult" },
		{ "CrowdAreaWeightMarketMult", false, 1.6f, 0.2f, 2.0f, 0.5f, 1.6f, false, ETuningLatch::Round, "Mult" },
		{ "CrowdAreaWeightTempleMult", false, 0.8f, 0.2f, 2.0f, 0.5f, 1.6f, false, ETuningLatch::Round, "Mult" },
		{ "CrowdAreaWeightResidentialMult", false, 0.6f, 0.2f, 2.0f, 0.5f, 1.6f, false, ETuningLatch::Round, "Mult" },
		{ "CrowdFullPacePct", false, 55.0f, 50.0f, 100.0f, 50.0f, 65.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdYieldPct", false, 70.0f, 0.0f, 100.0f, 70.0f, 90.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdLongPausePct", false, 45.0f, 10.0f, 60.0f, 35.0f, 50.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdIdleMinSec", false, 4.0f, 2.0f, 30.0f, 4.0f, 16.0f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdIdleMaxSec", false, 16.0f, 2.0f, 30.0f, 4.0f, 16.0f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdStallCoverMinPct", false, 10.0f, 0.0f, 30.0f, 10.0f, 20.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdMaxIdleSec", false, 20.0f, 10.0f, 60.0f, 20.0f, 25.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdConversationMinSec", false, 6.0f, 2.0f, 60.0f, 5.0f, 20.0f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdConversationMaxSec", false, 20.0f, 2.0f, 60.0f, 5.0f, 20.0f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdConverseStartPct", false, 50.0f, 0.0f, 100.0f, 30.0f, 70.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdConverseReplyPct", false, 60.0f, 0.0f, 100.0f, 40.0f, 80.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdGroupDriftSec", false, 5.0f, 0.0f, 20.0f, 3.0f, 8.0f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdAmbientTouchMinSec", false, 0.8f, 0.3f, 3.0f, 0.6f, 1.5f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdAmbientTouchMaxSec", false, 1.5f, 0.3f, 3.0f, 0.6f, 1.5f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdRunIntervalSec", false, 405.0f, 60.0f, 3600.0f, 300.0f, 600.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdVaultIntervalSec", false, 1443.0f, 120.0f, 7200.0f, 866.0f, 2020.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdClimbIntervalSec", false, 3409.0f, 120.0f, 7200.0f, 2045.0f, 4773.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdBraceIntervalSec", false, 3000.0f, 300.0f, 7200.0f, 2000.0f, 4500.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdBraceCapSharePct", false, 55.0f, 0.0f, 100.0f, 40.0f, 70.0f, false, ETuningLatch::Action, "Pct" },
		{ "CrowdDecoyTendIntervalSec", false, 13125.0f, 1800.0f, 60000.0f, 10500.0f, 16875.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdDecoyCompletePct", false, 75.0f, 0.0f, 100.0f, 60.0f, 90.0f, false, ETuningLatch::Action, "Pct" },
		{ "CrowdDecoyEarlyReleaseMinSec", false, 0.8f, 0.3f, 3.0f, 0.6f, 1.0f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdDecoyEarlyReleaseMaxSec", false, 2.8f, 1.0f, 5.0f, 2.2f, 2.9f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdBushIntervalSec", false, 300.0f, 120.0f, 320.0f, 240.0f, 315.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdBushDwellMedianSec", false, 15.0f, 3.0f, 120.0f, 8.0f, 25.0f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdBushDwellSigma", false, 0.8f, 0.3f, 1.5f, 0.6f, 1.0f, false, ETuningLatch::Action, "Sigma" },
		{ "CrowdReversalIntervalSec", false, 395.0f, 60.0f, 3600.0f, 237.0f, 553.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdHeadHoldIntervalSec", false, 600.0f, 120.0f, 3600.0f, 360.0f, 840.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdAlleyDawdleIntervalSec", false, 1200.0f, 300.0f, 7200.0f, 900.0f, 1800.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdStrikeReactRadiusCm", false, 1500.0f, 500.0f, 3200.0f, 1000.0f, 2000.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdStartleRadiusCm", false, 600.0f, 200.0f, 1000.0f, 400.0f, 800.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdPanicRunPct", false, 3.0f, 0.0f, 20.0f, 2.0f, 5.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdStartleBracePct", false, 8.0f, 0.0f, 20.0f, 5.0f, 12.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdTellChainGapSec", false, 20.0f, 10.0f, 35.0f, 20.0f, 30.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdStrikeResponderLimit", true, 8.0f, 1.0f, 20.0f, 6.0f, 10.0f, false, ETuningLatch::Use, "Limit" },
		{ "CrowdReactRefractorySec", false, 8.0f, 0.0f, 30.0f, 5.0f, 12.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdAmbientCycleIntervalSec", false, 80.0f, 40.0f, 90.0f, 60.0f, 90.0f, false, ETuningLatch::Round, "Sec" },
		{ "CrowdCycleGapMinSec", false, 6.0f, 2.0f, 60.0f, 5.0f, 25.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdCycleGapMaxSec", false, 20.0f, 2.0f, 60.0f, 5.0f, 25.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdReplacementMinDelaySec", false, 10.0f, 3.0f, 30.0f, 8.0f, 15.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdReplacementMaxDelaySec", false, 60.0f, 30.0f, 180.0f, 45.0f, 90.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdExitGiveUpSec", false, 8.0f, 2.0f, 30.0f, 5.0f, 12.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdDecisionPerSec", false, 4.0f, 1.0f, 10.0f, 3.0f, 6.0f, false, ETuningLatch::Use, "PerSec" },
		{ "CrowdSteerPerSec", false, 30.0f, 10.0f, 60.0f, 20.0f, 30.0f, false, ETuningLatch::Use, "PerSec" },
		{ "CrowdPathStartsPerFrameLimit", true, 2.0f, 1.0f, 8.0f, 2.0f, 4.0f, false, ETuningLatch::Use, "Limit" },
		{ "CrowdReactNonePct", false, 40.0f, 0.0f, 100.0f, 30.0f, 50.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdReactGlancePct", false, 30.0f, 0.0f, 100.0f, 20.0f, 40.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdReactPausePct", false, 12.0f, 0.0f, 100.0f, 8.0f, 16.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdReactReroutePct", false, 7.0f, 0.0f, 100.0f, 4.0f, 10.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdReactGlanceMinSec", false, 1.0f, 0.2f, 10.0f, 0.5f, 2.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdReactGlanceMaxSec", false, 3.0f, 0.2f, 10.0f, 2.0f, 4.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdReactPauseMinSec", false, 2.0f, 0.5f, 15.0f, 1.0f, 3.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdReactPauseMaxSec", false, 5.0f, 0.5f, 15.0f, 4.0f, 6.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdPanicRunMaxSec", false, 3.0f, 0.5f, 10.0f, 2.0f, 4.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdReactDelayMinSec", false, 0.3f, 0.05f, 3.0f, 0.2f, 0.4f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdReactDelayMaxSec", false, 1.2f, 0.05f, 3.0f, 1.0f, 1.5f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdLegMinCm", false, 1000.0f, 200.0f, 6000.0f, 800.0f, 1200.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdLegMaxCm", false, 3000.0f, 200.0f, 6000.0f, 2500.0f, 3500.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdShortPauseMinSec", false, 1.0f, 0.5f, 4.0f, 0.8f, 1.5f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdShortPauseMaxSec", false, 3.0f, 0.5f, 4.0f, 2.5f, 3.5f, false, ETuningLatch::Action, "Sec" },
		{ "CrowdLongPauseMaxPct", false, 90.0f, 50.0f, 100.0f, 80.0f, 90.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdPauseContextRadiusCm", false, 300.0f, 100.0f, 600.0f, 250.0f, 350.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdGlanceRangeCm", false, 600.0f, 200.0f, 1500.0f, 500.0f, 700.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdGlanceHoldMinSec", false, 0.4f, 0.1f, 3.0f, 0.3f, 0.5f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdGlanceHoldMaxSec", false, 2.5f, 0.5f, 6.0f, 2.0f, 2.5f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdGlanceIntervalMinSec", false, 4.0f, 1.0f, 30.0f, 3.0f, 5.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdGlanceIntervalMaxSec", false, 10.0f, 1.0f, 30.0f, 8.0f, 12.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdGoodsHoldMinSec", false, 1.0f, 0.2f, 10.0f, 0.8f, 1.5f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdGoodsHoldMaxSec", false, 4.0f, 0.2f, 10.0f, 3.0f, 5.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdGoodsPitchMinDeg", false, 10.0f, 0.0f, 30.0f, 8.0f, 12.0f, false, ETuningLatch::Use, "Deg" },
		{ "CrowdGoodsPitchMaxDeg", false, 20.0f, 0.0f, 30.0f, 18.0f, 20.0f, false, ETuningLatch::Use, "Deg" },
		{ "CrowdSalientRangeCm", false, 1500.0f, 500.0f, 3200.0f, 1000.0f, 2000.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdSalientGlancePct", false, 50.0f, 0.0f, 100.0f, 40.0f, 60.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdPartnerLookMinPct", false, 60.0f, 0.0f, 100.0f, 55.0f, 65.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdPartnerLookMaxPct", false, 80.0f, 0.0f, 100.0f, 75.0f, 85.0f, false, ETuningLatch::Use, "Pct" },
		{ "CrowdHeadHoldMinSec", false, 2.5f, 1.0f, 10.0f, 2.5f, 3.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdHeadHoldMaxSec", false, 6.0f, 1.0f, 10.0f, 5.0f, 7.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdConverseRangeCm", false, 150.0f, 50.0f, 400.0f, 120.0f, 180.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdConverseFaceDeg", false, 60.0f, 10.0f, 90.0f, 50.0f, 70.0f, false, ETuningLatch::Use, "Deg" },
		{ "CrowdConverseTurnAwayMinSec", false, 2.0f, 0.5f, 10.0f, 1.5f, 2.5f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdConverseTurnAwayMaxSec", false, 5.0f, 0.5f, 10.0f, 4.0f, 6.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdGroupDriftRangeCm", false, 200.0f, 50.0f, 500.0f, 150.0f, 250.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdGroupDriftHeadingDeg", false, 30.0f, 5.0f, 90.0f, 20.0f, 40.0f, false, ETuningLatch::Use, "Deg" },
		{ "CrowdRecentreAngleDeg", false, 70.0f, 30.0f, 120.0f, 60.0f, 80.0f, false, ETuningLatch::Use, "Deg" },
		{ "CrowdRecentreDelaySec", false, 0.5f, 0.1f, 3.0f, 0.4f, 0.7f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdAvoidTriggerCm", false, 100.0f, 30.0f, 300.0f, 80.0f, 120.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdAvoidOffsetCm", false, 50.0f, 10.0f, 150.0f, 40.0f, 60.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdAvoidWideOffsetCm", false, 100.0f, 20.0f, 300.0f, 80.0f, 120.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdYieldJitterMinSec", false, 0.2f, 0.0f, 2.0f, 0.15f, 0.25f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdYieldJitterMaxSec", false, 0.6f, 0.0f, 2.0f, 0.5f, 0.7f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdBlockedWidenSec", false, 0.5f, 0.1f, 3.0f, 0.4f, 0.7f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdBlockedRepathSec", false, 3.0f, 1.0f, 10.0f, 2.5f, 4.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdProgressMinCm", false, 50.0f, 10.0f, 200.0f, 40.0f, 60.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdProgressWindowSec", false, 5.0f, 1.0f, 15.0f, 4.0f, 6.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdStaticObstacleCm", false, 50.0f, 20.0f, 150.0f, 40.0f, 60.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdLaneOffsetCm", false, 50.0f, 0.0f, 150.0f, 40.0f, 60.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdCarrotMinCm", false, 150.0f, 50.0f, 400.0f, 120.0f, 180.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdCarrotLeadSec", false, 1.0f, 0.2f, 3.0f, 0.8f, 1.2f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdRetryBlockSec", false, 10.0f, 1.0f, 60.0f, 8.0f, 15.0f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdDryRunReachCm", false, 10.0f, 0.0f, 50.0f, 5.0f, 15.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdDryRunFacingDeg", false, 10.0f, 0.0f, 45.0f, 5.0f, 15.0f, false, ETuningLatch::Use, "Deg" },
		{ "CrowdPathInvalidHoldSec", false, 0.5f, 0.1f, 2.0f, 0.4f, 0.6f, false, ETuningLatch::Use, "Sec" },
		{ "CrowdHashCellCm", false, 200.0f, 50.0f, 800.0f, 200.0f, 200.0f, false, ETuningLatch::Use, "Cm" },
		{ "CrowdRaysPerFrameLimit", true, 8.0f, 1.0f, 64.0f, 6.0f, 10.0f, false, ETuningLatch::Use, "Limit" },
	};
}
