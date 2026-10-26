#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

struct FCrowdTuning_Server;

/**
 * FCrowdStallRegulator: the connection-stall cover and the stationary-stretch watchdog (GDD CNB-R5 C2 and watchdog, F4,
 * Tell Matrix T9 and T14; ADR-0016 section 3; story cnb-014). Plain C++ inside the Director.
 *
 * Stretches: each driven pawn's current VISIBLE stationary stretch is tracked from movement state (speed at rest), not
 * from its StateTree state, so a pause, a conversation and a reaction pause chain into one stretch. While the pawn is
 * concealed the clock is suspended (the stretch neither grows nor ends).
 *
 * F4 regulator: while fewer than ceil(CrowdStallCoverMinPct / 100 * N_live) pawns are in a stretch already at least
 * P_th = SessionSoftTimeoutSec + 1 old, the long-pause share is raised to min(2 * CrowdLongPausePct,
 * CrowdLongPauseMaxPct). It reads only stretch ages (public motion), never a pawn's kind.
 *
 * Watchdog: a stretch reaching CrowdMaxIdleSec is reported due so the pawn starts a leg in that tick.
 */
class STEALTHGAMEPLAY_API FCrowdStallRegulator
{
public:
	/** Below this horizontal speed (cm/s) a pawn is stationary for the stretch clock. */
	static constexpr double StationarySpeedCm = 5.0;

	/** The regulator raises the long share while fewer than ceil(f_min N) + this NPCs are covered (0 when f_min is 0). */
	static constexpr int32 CoverMarginNpcs = 3;

	/** Updates Ordinal's stretch at Now; returns the stretch age (0 while moving). */
	double Update(uint32 Ordinal, double Speed2D, bool bConcealed, double Now);

	/** Forgets a pawn (unregistered, absorbed, killed). */
	void Remove(uint32 Ordinal) { Stretches.Remove(Ordinal); }

	/** The current stretch age of Ordinal (0 when moving or unknown). */
	double GetAge(uint32 Ordinal, double Now) const;

	/** True when Ordinal's stretch has reached MaxIdleSec (the watchdog's due condition). */
	bool IsWatchdogDue(uint32 Ordinal, double Now, double MaxIdleSec) const { return GetAge(Ordinal, Now) >= MaxIdleSec; }

	/** Pawns whose stretch is at least ThresholdSec old (the F4 metric). */
	int32 CountCovered(double Now, double ThresholdSec) const;

	/**
	 * The long-pause share (percent) the next pause draws with: CrowdLongPausePct, raised to min(2x, CrowdLongPauseMaxPct)
	 * while the covered count is below the floor for LiveCount pawns.
	 */
	/**
	 * F4's regulated long-pause share. It is raised while the covered count, or ProjectedCovered (the pawns whose current
	 * rest still runs at Now + P_th, so the count P_th from now: a long pause takes P_th to count), is below
	 * ceil(f_min N) + CoverMarginNpcs. ProjectedCovered < 0 skips the projection.
	 */
	double EffectiveLongPausePct(double Now, int32 LiveCount, const FCrowdTuning_Server& Tuning, double StallThresholdSec, int32 ProjectedCovered = -1) const;

	int32 Num() const { return Stretches.Num(); }

private:
	struct FStretch
	{
		/** Start of the current stationary stretch, or a negative value while moving. */
		double Start = -1.0;
		/** The last update time (to suspend the clock while concealed). */
		double LastUpdate = 0.0;
	};
	TMap<uint32, FStretch> Stretches;
};
