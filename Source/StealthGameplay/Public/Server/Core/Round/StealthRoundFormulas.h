#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/Round/StealthRoundTypes.h"
#include "View/Random/StealthRandomTag.h"

/**
 * Round's pure formulas (GDD round-match-framework F4, F5, F6; ADR-0006 sections 4-5). Plain C++, no state, no
 * engine contact: every input is an argument, so the Logic tests call them directly.
 *
 *   F4 (stories rmf-040, rmf-060): G_eff = min(G, RejoinWindowSec - GraceMarginSec);
 *      GraceExpiry(p) = max(DisconnectedAt(p), ActivePhaseStart) + G_eff.
 *   F5 (stories rmf-036..038): sigma = FisherYates(D, SubSeed(SpawnOrder, LayoutAttempt)) over the canonical slot list;
 *      ZoneShare_z = P_z / (P_z + N_z) for P_z > 0 only, required <= 0.5; GlobalShare = P / (P + NpcCount).
 *   F6 (story rmf-035): T_prep_max = max(TravelLoadTimeoutSec, T_srvload + LayoutTotalBudgetSec + T_tuning +
 *      TravelAckGraceSec); T_needed = T_prep_max + T_spawn + WorldReadyTimeoutSec + StartLeadSec + 30;
 *      PreparingWatchdogEff = max(PreparingWatchdogSec, T_needed), warning when T_needed wins.
 *
 * Never log or expose a sub-seed or the stream built from it (ADR-0006 section 7).
 */
namespace StealthRoundFormulas
{
	// --- F4 ---------------------------------------------------------------------------------------------------------

	/** F4 GraceMarginSec: a design constant (the guaranteed RejoinWindowSec - G_eff >= 30 for an informant's return). */
	inline constexpr double GraceMarginSec = 30.0;

	/** F4 G_eff = min(G, RejoinWindowSec - GraceMarginSec). */
	STEALTHGAMEPLAY_API double EffectiveGraceSec(double DisconnectGraceSec, double RejoinWindowSec);

	/** F4 GraceExpiry(p) = max(DisconnectedAt(p), ActivePhaseStart) + G_eff. */
	STEALTHGAMEPLAY_API double GraceExpiry(double DisconnectedAt, double ActivePhaseStart, double EffectiveGrace);

	// --- F5 ---------------------------------------------------------------------------------------------------------

	/**
	 * The spawn-order stream tag (ADR-0006 section 5: one fixed uint64 per consumer). Named by the RMF Exit B Preparing
	 * batch (story rmf-036 Open item; neither ADR-0006 section 5 nor ADR-0002 section 8.6 named one; recorded in the
	 * decision log as an ADR-0006 amendment).
	 */
	inline constexpr uint64 SpawnOrderStreamTag = StealthRandom::Tag("RND_SPWN");

	/**
	 * sigma for D canonical slots: Out[k] is the slot spawned k-th. A pure function of (D, SubSeed): which slots are
	 * players never enters it (R23 (a)), so flipping every player flag cannot change it (story rmf-037).
	 */
	STEALTHGAMEPLAY_API void ComputeSpawnOrder(int32 D, uint64 SpawnOrderSubSeed, TArray<int32>& OutSigma);

	/** ZoneShare_z, unset for a zone with no player (an empty zone has no share). */
	STEALTHGAMEPLAY_API TOptional<double> ZoneShare(const FRoundPlacementZone& Zone);

	/** F5 required share: ZoneShare_z <= MaxZoneShare (asserted by Round; guaranteed by LSC R6.2/D6). */
	inline constexpr double MaxZoneShare = 0.5;

	/** True when every zone with players satisfies ZoneShare_z <= 0.5 (an abort PlacementInvariant otherwise). */
	STEALTHGAMEPLAY_API bool PlacementSharesHold(TConstArrayView<FRoundPlacementZone> Zones);

	/** F5 GlobalShare = P / (P + NpcCount); 0 when both are 0. */
	STEALTHGAMEPLAY_API double GlobalShare(int32 Participants, int32 NpcCount);

	// --- F6 ---------------------------------------------------------------------------------------------------------

	/** F6's fixed margin (the "+ 30" of T_needed). */
	inline constexpr double WatchdogMarginSec = 30.0;

	/** F6 evaluated once at freeze. */
	struct FWatchdogBound
	{
		double PrepMaxSec = 0.0;
		double NeededSec = 0.0;
		double EffectiveSec = 0.0;
		/** T_needed > PreparingWatchdogSec: the effective watchdog is T_needed and a warning is logged (a tie is no warning). */
		bool bNeededWins = false;
	};

	STEALTHGAMEPLAY_API FWatchdogBound ComputePreparingWatchdog(const FRoundPreparingTimings& Timings);
}
