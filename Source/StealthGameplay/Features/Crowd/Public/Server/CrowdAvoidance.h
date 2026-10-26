#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/CrowdSpatialHash.h"

class FCrowdDraws;
struct FCrowdTuning_Server;

/** What the avoidance step asks of the follower this tick. */
struct FCrowdAvoidanceOutput
{
	/** Lateral carrot offset, cm (positive = to the walker's right). 0 when not avoiding. */
	double LateralOffsetCm = 0.0;
	/** True while a yield side-step is applied. */
	bool bYielding = false;
	/** The yield draw happened this tick (a newly detected approach). */
	bool bNewApproach = false;
	/** The decision of the newly detected approach (valid with bNewApproach). */
	bool bNewApproachYields = false;
};

/**
 * FCrowdAvoidance: one walker's avoidance as Move offsets (ADR-0016 section 4.2; GDD CNB-R3.3, AC-CNB-32; story
 * cnb-011). On a predicted closest approach below CrowdAvoidTriggerCm with a moving steering obstacle, the yield decision
 * is drawn ONCE at first detection (ECrowdDecision::Yield, CrowdYieldPct; never re-rolled for that approach, and the
 * partner's kind is not an input) and, when it yields, a lateral offset of CrowdAvoidOffsetCm applies after a jitter of
 * U(CrowdYieldJitterMin, Max) s (a due time, never a decision tick): side from the relative position, keep-right on a
 * tie, a per-pawn coin only on an exact head-on. Planted and tending pawns are static obstacles of
 * CrowdStaticObstacleCm plus margin, always stepped around. Nothing pushes another pawn; there is no RVO.
 * Plain C++; the follower applies the offset to its carrot.
 */
class STEALTHGAMEPLAY_API FCrowdAvoidance
{
public:
	/** Seconds ahead an approach is predicted (about 3-5 m at walking speeds). */
	static constexpr double HorizonSec = 3.0;
	/** Range of the neighbour query. */
	static constexpr double QueryRangeCm = 600.0;
	/** Relative position cross product below this (cm) counts as an exact head-on. */
	static constexpr double HeadOnToleranceCm = 1.0;

	/**
	 * One avoidance step. Self: the walker's public facts; Neighbours: the Steering query around it (nearest first);
	 * bWantsToMove: the follower is on a leg; bBlockedWide: the blocked ladder asks for the widened offset.
	 */
	FCrowdAvoidanceOutput Step(uint32 Ordinal, const FCrowdPerceivedPawn& Self, TConstArrayView<const FCrowdPerceivedPawn*> Neighbours,
		double Now, bool bWantsToMove, bool bBlockedWide, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws);

	/** Forgets every approach (a new leg or a stop). */
	void Reset();

	/** Decisions taken so far (tests count the yield share per partner). */
	int32 GetApproachCount() const { return ApproachCount; }
	int32 GetYieldCount() const { return YieldCount; }

	/** The predicted closest approach distance (2D) of two moving points, and when (t >= 0 seconds). */
	static double PredictClosestApproach(const FVector& SelfPos, const FVector& SelfVel, const FVector& OtherPos, const FVector& OtherVel, double& OutTime);

private:
	struct FApproach
	{
		uint64 Key = 0;
		bool bYield = false;
		double ActiveFrom = 0.0;
		double Side = 1.0;
		/** Last time the approach was still predicted (it ends when not predicted for a moment). */
		double LastSeen = 0.0;
	};
	TArray<FApproach, TInlineAllocator<4>> Approaches;
	int32 ApproachCount = 0;
	int32 YieldCount = 0;
	/** The per-pawn head-on coin (+1 right, -1 left), drawn once. */
	double HeadOnSide = 0.0;
};
