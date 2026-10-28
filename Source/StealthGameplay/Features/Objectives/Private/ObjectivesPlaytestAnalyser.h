#pragma once

#include "CoreMinimal.h"
#include "ObjectivesMetrics.h"

/**
 * The offline Objectives playtest analyser (Objectives Chain story oc-023; GDD objectives-chain.md F1, F2, F3; AC-OC-19,
 * AC-OC-20, AC-OC-21 playtest part). Pure functions over per-round records extracted from the server Round Event Log
 * (ObjectiveStage, FinalCompleted, TendCompleted, StageSignalReleased, ClueTooBroad) plus the tenders' movement samples and the
 * layout record. Nothing in gameplay reads it; every output is ADVISORY (a playtest report, never a rule). The calibration
 * inputs (kappa, phi, o, q, r, t_settle, N_probe, n_full, T_round) stay provisional until logged leg times exist (GDD OQ4).
 */

/** One objective-ended round for F1 (seconds from Active start). */
struct STEALTHGAMEPLAY_API FObjectivesPacingRound
{
	/** ObjectiveWin round length (t_end), seconds. */
	double DurationSec = 0.0;
	/** Per team, its counted completion times (stages then the final), seconds. */
	TArray<double> Completions[2];
	/** True when the round ended by ObjectiveWin (only those count for F1). */
	bool bObjectiveWin = true;
	/** Debug or dev rounds (excluded). */
	bool bExcluded = false;
};

/** F1's report. */
struct STEALTHGAMEPLAY_API FObjectivesPacingReport
{
	int32 Rounds = 0;
	bool bEnoughData = false;
	double MedianDurationSec = 0.0;
	/** The median below 6 minutes (strict): the F1 pacing levers apply (never the SP-7 stall levers). */
	bool bPacingLeversTriggered = false;
	/** Median per-team leg time per leg index (0 = Active to P1, 1 = P1 to P2, 2 = P2 to final), -1 when no data. */
	double MedianLegSec[3] = { -1.0, -1.0, -1.0 };
	double TChainSec = 0.0;
};

/** One counted completion for F2: the tender's speed samples before it (seconds, cm/s) and its Complete time. */
struct STEALTHGAMEPLAY_API FObjectivesTellSample
{
	double CompleteTime = 0.0;
	TArray<TPair<double, double>> SpeedSamples;
};

/** One hold attempt for F2's probe count and F3's shares. */
struct STEALTHGAMEPLAY_API FObjectivesTendAttempt
{
	int32 Stage = 0;
	double StartTime = 0.0;
	double EndTime = 0.0;
	/** Held to D (a Complete), else an early release (a cancel). */
	bool bFullLength = false;
	/** At the tender's own live court. */
	bool bAtOwnLiveCourt = false;
	/** By a player (else an unbound crowd decoy). */
	bool bByPlayer = true;
	/** A player's full-length tend that counted (else a feint at a non-live court). */
	bool bCounted = false;
};

/** One round for F2 / F3. */
struct STEALTHGAMEPLAY_API FObjectivesTellRound
{
	TArray<FObjectivesTellSample> CountedCompletions;
	TArray<FObjectivesTendAttempt> Attempts;
};

/** F2's report. */
struct STEALTHGAMEPLAY_API FObjectivesTellReport
{
	int32 RoundsUsed = 0;
	bool bEnoughData = false;
	double MeanTellSec = 0.0;
	bool bInBand = false;
	/** Probes (early releases after TendCueDelaySec at non-live courts) per stage 0..2. */
	int32 ProbesPerStage[3] = { 0, 0, 0 };
};

/** F3's measured shares. */
struct STEALTHGAMEPLAY_API FObjectivesShareReport
{
	int32 Rounds = 0;
	bool bEnoughData = false;
	bool bSimulation = false;
	/** Share of opponent-player tends among full-length tends (banded 0.65-0.75). */
	double PFull = 0.0;
	bool bPFullInBand = false;
	/** Share among early releases (never banded; alarm above 0.90). */
	double PShort = 0.0;
	bool bPShortAlarm = false;
	/** Among player full-length tends: counted vs feint. */
	int32 PlayerCounted = 0;
	int32 PlayerFeint = 0;
	/** P_full above the band with most player tends being feints: a chosen tell, recorded, not a defect. */
	bool bHighByFeinting = false;
};

struct STEALTHGAMEPLAY_API FObjectivesPlaytestAnalyser
{
	static constexpr int32 PacingMinRounds = 20;
	static constexpr double PacingFloorSec = 360.0;
	static constexpr int32 TellMinRounds = 10;
	static constexpr int32 ShareMinRounds = 60;
	static constexpr double ShortAlarm = 0.90;

	/** F1: median ObjectiveWin duration and per-team legs next to T_chain; fewer than 20 rounds is NOT ENOUGH DATA. */
	static FObjectivesPacingReport ReportPacing(TConstArrayView<FObjectivesPacingRound> Rounds, double TChainSec);

	/**
	 * F2: the time from the tender's last stop (the last moment its speed fell below WalkThreshold and stayed below it until
	 * Complete) to Complete, averaged over counted completions; in band when |mean - ETellSec| <= 1 s (inclusive). Rounds
	 * with no counted completion are skipped; fewer than 10 used rounds is NOT ENOUGH DATA. Probes: early releases at
	 * non-live courts lasting at least TendCueDelaySec, per stage.
	 */
	static FObjectivesTellReport ReportTellTiming(TConstArrayView<FObjectivesTellRound> Rounds, double WalkThresholdCmPerSec, double ETellSec, double TendCueDelaySec);

	/** F3: the measured shares (bSimulation labels an accelerated simulation; fewer than 60 rounds is NOT ENOUGH DATA). */
	static FObjectivesShareReport ReportShares(TConstArrayView<FObjectivesTellRound> Rounds, bool bSimulation);

	/** The markdown report of the three sections (production/qa/evidence/oc-playtest-metrics.md). */
	static FString ToMarkdown(const FObjectivesPacingReport& Pacing, const FObjectivesTellReport& Tell, const FObjectivesShareReport& Shares);

	/** The median of Values (the mean of the middle two for an even count); 0 for none. */
	static double Median(TArray<double> Values);
};
