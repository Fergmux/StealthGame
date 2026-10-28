#pragma once

#include "CoreMinimal.h"
#include "Server/EventLog/StealthRoundLogTypes.h"

/**
 * The Objectives reporting metrics (Objectives Chain story oc-017; GDD objectives-chain.md F5 stall metrics and F3 tend
 * posterior; AC-OC-21, AC-OC-23; cross-review C-W35). Pure functions over plain inputs or the server-only Round Event Log
 * entries: no world, no rule depends on them (a stall is an Objectives defect answered only by the SP-7 levers). They run in
 * the playtest analyser (story oc-023) and the telemetry epic unchanged.
 *
 * F5 (times in seconds from Active start, t = 0):
 *   G_mutual      the largest gap between consecutive counted completions of either team, with t = 0 and t_end included
 *   G_team(T)     the same over team T's counted completions only
 *   FinalDwell    t_end - min over teams of t_P2,T (no team completed Prelim2: not defined, never a defect)
 *   StallDefect   StallFlag or G_mutual > ObjectiveStallGapDefectSec or FinalDwell > ObjectiveFinalDwellDefectSec (strict)
 * A counted completion is an ObjectiveStage or FinalCompleted entry; a final Complete rejected by the latch is logged
 * TendCompleted(NoEffect, Latched) and only counts in tend statistics (it never moves t_end or FinalDwell). Debug-flagged entries
 * are left out (R22: debug completions are excluded from playtest metrics).
 *
 * F3: P_full = R_full / (R_full + N_npc * r_N * c), r_N = 60 / CrowdDecoyTendIntervalSec decoy tends per NPC per minute,
 * R_full = n_full / T_round; the band 0.65-0.75 is inclusive.
 */
struct STEALTHGAMEPLAY_API FObjectivesStallInput
{
	/** Counted completion times per team (seconds from Active start, any order): stages and the final. */
	TArray<double> Completions[2];
	/** Prelim2 completion time per team (-1: not completed). */
	double Prelim2Time[2] = { -1.0, -1.0 };
	/** The round's end (seconds from Active start). */
	double TEnd = 0.0;
	/** Round's F7 long-round flag. */
	bool bStallFlag = false;
	/** The round record's exclusion flag (a 1v0 dev round, story oc-021): F5 does not count the round. */
	bool bExcludeFromStallMetrics = false;
};

/** F5's results for one round. */
struct STEALTHGAMEPLAY_API FObjectivesStallMetrics
{
	double GMutual = 0.0;
	double GTeam[2] = { 0.0, 0.0 };
	/** -1 when no team completed Prelim2. */
	double FinalDwell = -1.0;
	bool bStallDefect = false;
	/** The round was excluded (bExcludeFromStallMetrics): nothing else is meaningful. */
	bool bExcluded = false;
};

/** Tend statistics of a log (TendCompleted entries; a latched final Complete counts here, twice when it happens twice). */
struct STEALTHGAMEPLAY_API FObjectivesTendCounts
{
	int32 Counted = 0;
	int32 NoEffect = 0;
	int32 Latched = 0;
};

/** The pure F5 / F3 functions. */
struct STEALTHGAMEPLAY_API FObjectivesMetrics
{
	/** The F3 band edges (inclusive). */
	static constexpr double PosteriorBandLow = 0.65;
	static constexpr double PosteriorBandHigh = 0.75;

	/** F5 over an explicit input. */
	static FObjectivesStallMetrics ComputeStall(const FObjectivesStallInput& Input, double StallGapDefectSec, double FinalDwellDefectSec);

	/**
	 * The F5 input from a round's Event Log: counted completions from ObjectiveStage (D0 team, D1 stage) and FinalCompleted (D0
	 * team) entries minus ActivePhaseStart; TEnd from the WinLatched entry (else LastServerTime); an ObjectivePacing entry with
	 * Code ExcludeCode sets bExcludeFromStallMetrics. Debug-flagged entries are skipped.
	 */
	static FObjectivesStallInput StallInputFromLog(TConstArrayView<FRoundLogEntry> Entries, double ActivePhaseStart, double LastServerTime, bool bStallFlag, uint16 ExcludeCode = 1);

	/** TendCompleted tallies of a log (Counted / NoEffect; Latched counts the NoEffect ones flagged Latched). */
	static FObjectivesTendCounts CountTends(TConstArrayView<FRoundLogEntry> Entries);

	/** F3's decoy rate r_N (decoy tends per NPC per minute) from Crowd's CrowdDecoyTendIntervalSec. */
	static double DecoyRatePerNpcPerMinute(double CrowdDecoyTendIntervalSec);

	/** F3: P_full = R_full / (R_full + N_npc * r_N * c). */
	static double FullTendPosterior(double RFullPerMinute, int32 NpcCount, double DecoyRatePerNpcPerMinute, double CompletionShare);

	/** F3's inverse: the r_N that gives PFull. */
	static double DecoyRateForPosterior(double RFullPerMinute, int32 NpcCount, double CompletionShare, double PFull);

	/** True when P is inside the inclusive band (with a 1e-9 tolerance for the boundary figures of the GDD). */
	static bool IsInPosteriorBand(double P);
};
