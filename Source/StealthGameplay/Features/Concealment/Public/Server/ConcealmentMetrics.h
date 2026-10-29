#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/ConcealmentLog.h"

/**
 * Concealment's metric calculators (GDD concealment-bushes.md F1, F2, CON-R12, CON-R15, AC-CON-24; ADR-0017 section 10;
 * story con-030). Pure functions over plain inputs and the concealment log rows (ConcealmentLog.h); the review of logged
 * matches against them is story con-031. AssassinationRangeCm is not an input of any output here.
 *
 *   F1  E[O] = (n_N * r_N * D_N + n_P * r_P * D_P) / N_B           expected occupants of one bush (Poisson lambda)
 *       m0   = 1 - e^(-lambda)                                     the multi-occupant baseline
 *   CON-R12 review cap = max(40 %, m0 + 10 points); fewer than 30 bush strikes with candidates is inconclusive
 *   F2  r_N,req = r_P * n_P * (1 - k * b) / (k * b * n_N),  b = n_P / (n_P + n_N)
 *   CON-R15 ambush metric: kills from Emerging as a share of all kills, reviewed above 25 %
 *
 * Rates are per minute and dwell times in minutes, as the GDD writes them.
 */
namespace ConcealmentMetrics
{
	/** CON-R12's fixed floor of the review cap (creative-director ruling D1, 2026-10-25). */
	inline constexpr double ReviewCapFloor = 0.40;
	/** CON-R12: the cap tracks the measured baseline by this many points above it. */
	inline constexpr double ReviewCapMarginPoints = 0.10;
	/** CON-R12 / AC-CON-24(c): below this many bush strikes with candidates the multi-occupant share is inconclusive. */
	inline constexpr int32 MinStrikesWithCandidates = 30;
	/** CON-R15 / AC-CON-24(d): the ambush share is flagged strictly above this. */
	inline constexpr double AmbushReviewShare = 0.25;

	/** F1's inputs. */
	struct FOccupancyInputs
	{
		/** N_B: hideable bushes. */
		double NumBushes = 9.0;
		/** n_N: NPCs. */
		double NpcCount = 50.0;
		/** r_N: NPC bush entries per NPC per minute. */
		double NpcRatePerMin = 0.20;
		/** D_N: mean NPC dwell, minutes. */
		double NpcDwellMin = 0.345;
		/** n_P: opponent players. */
		double PlayerCount = 5.0;
		/** r_P: player bush entries per player per minute. */
		double PlayerRatePerMin = 0.24;
		/** D_P: mean player dwell, minutes. */
		double PlayerDwellMin = 0.345;
	};

	/** F1: E[O], the expected occupants of a bush (lambda). */
	STEALTHGAMEPLAY_API double ExpectedOccupancy(const FOccupancyInputs& In);

	/** F1: m0 = 1 - e^(-Lambda). */
	STEALTHGAMEPLAY_API double MultiOccupantBaseline(double Lambda);

	/** CON-R12: max(40 %, m0 + 10 points), as a fraction. */
	STEALTHGAMEPLAY_API double ReviewCap(double M0);

	/** F2: the NPC entry rate (per minute) a player's rate needs to stay covered at the cover factor K. */
	STEALTHGAMEPLAY_API double RequiredNpcRate(double PlayerRatePerMin, double PlayerCount, double NpcCount, double K);

	/** The three-way verdict of a review metric. */
	enum class EReviewVerdict : uint8
	{
		/** Too few samples to say anything. */
		Inconclusive,
		WithinCap,
		/** Goes to the creative director. */
		Review,
	};

	/** One player's concealment over one round, from the log. */
	struct FPlayerConcealment
	{
		/** Commits of the player (each a bush entry). */
		int32 Entries = 0;
		/** Seconds from each Commit to its ExitCross / Emerge / Killed (an open one runs to the round end). */
		double TimeConcealedSec = 0.0;
		/** TimeConcealedSec over the round length. */
		double ShareOfRound = 0.0;
		/** r_P: entries per minute of the round. */
		double RatePerMin = 0.0;
	};

	/** The player's concealment in Rows for a round from RoundStartServerTime lasting RoundSec. */
	STEALTHGAMEPLAY_API FPlayerConcealment SummarisePlayer(TConstArrayView<FConcealmentLogRow> Rows, FParticipantId Player, double RoundStartServerTime, double RoundSec);

	/** CON-R12's multi-occupant share m over a log. */
	struct FMultiOccupantShare
	{
		/** BushStrike rows whose candidate set (after the reach filter, the pick's set) was non-empty. */
		int32 StrikesWithCandidates = 0;
		/** Of those, the ones whose set held at least 2. */
		int32 MultiCandidate = 0;
		double Share = 0.0;
		EReviewVerdict Verdict = EReviewVerdict::Inconclusive;
	};

	/** m and its verdict against the cap at the measured M0 (inconclusive below MinStrikesWithCandidates). */
	STEALTHGAMEPLAY_API FMultiOccupantShare MultiOccupantShare(TConstArrayView<FConcealmentLogRow> Rows, double M0);

	/** The ambush metric: Emerge rows that killed over all kills. */
	struct FAmbushShare
	{
		int32 EmergeKills = 0;
		int32 TotalKills = 0;
		double Share = 0.0;
		/** Inconclusive with no kill; Review strictly above AmbushReviewShare. */
		EReviewVerdict Verdict = EReviewVerdict::Inconclusive;
	};

	/** The ambush share from Rows' Emerge kill flags over TotalKills (the round's KillResolved rows). */
	STEALTHGAMEPLAY_API FAmbushShare AmbushShare(TConstArrayView<FConcealmentLogRow> Rows, int32 TotalKills);

	/** The ambush share from counts (the same rule as AmbushShare). */
	STEALTHGAMEPLAY_API FAmbushShare AmbushShareFromCounts(int32 EmergeKills, int32 TotalKills);
}
