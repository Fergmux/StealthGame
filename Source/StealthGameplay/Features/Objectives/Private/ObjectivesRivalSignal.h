#pragma once

#include "CoreMinimal.h"
#include "Server/ObjectivesSeams.h"
#include "View/Random/StealthRandomTag.h"

/**
 * The OC-12 rival signal's draw and release grid (Objectives Chain story oc-014; ADR-0020 section 7 steps 1-2; ADR-0006;
 * GDD OC-12, F4, AC-OC-09, AC-OC-10). Pure integer functions: no clock, no queue, no float in the draw.
 *
 *   Delta_ms = UniformInRange(FSecretStream(GetServerSubSeed(StageSignal, k), Tag("OC_STAGE")), MinMs, MaxMs)
 *   t_rel_ms = t0_ms + QMs * ceil((t_c_ms + Delta_ms - t0_ms) / QMs)
 *
 * The draw is keyed by the round-global preliminary ordinal k only (both teams together, 1-based): no team, completer, step or
 * court parameter exists. Every release of both teams lies on the one grid t0 + n * Q anchored at Round's ActivePhaseStart, so
 * the tick a release lands on carries no team-specific timing. Bounds are whole milliseconds converted once (ToMs).
 */
struct STEALTHGAMEPLAY_API FObjectivesRivalSignal
{
	/** The secret stream tag of the StageSignal draw (ADR-0006 section 5). */
	static constexpr uint64 StreamTag = StealthRandom::Tag("OC_STAGE");

	/** Seconds to whole milliseconds (round half away from zero, as llround), the one conversion at tuning freeze or at a slot's ServerNow. */
	static int64 ToMs(double Seconds);

	/**
	 * Delta_ms for ordinal k from Draw's StageSignal sub-seed; -1 when no secret is held (sub-seed 0: nothing is drawn) or the
	 * bounds are refused (MinMs > MaxMs or MinMs < 0).
	 */
	static int32 DrawRivalDelayMs(uint32 Ordinal, const IObjectivesSecretDraw& Draw, int32 MinMs, int32 MaxMs);

	/** The same draw from an explicit sub-seed (the pinned golden vectors and the distribution test). */
	static int32 DrawRivalDelayMsFromSubSeed(uint64 SubSeed, int32 MinMs, int32 MaxMs);

	/**
	 * t_rel_ms on the grid: the first grid tick at or after t_c + Delta. The ceiling-division numerator (t_c + Delta - t0) must be
	 * positive and QMs > 0 (t_c >= t0 and Delta >= the minimum bound): -1 when it is not (a refused input, never a release).
	 */
	static int64 ComputeReleaseMs(int64 T0Ms, int64 TcMs, int64 DeltaMs, int64 QMs);
};
