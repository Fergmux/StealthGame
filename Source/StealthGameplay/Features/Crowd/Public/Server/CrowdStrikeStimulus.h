#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

class FCrowdDraws;
struct FCrowdTuning_Server;

/**
 * Strike reactions (GDD CNB-R8; ADR-0016 section 5; story cnb-024). The stimulus is OnActionStarted(Pawn, Attack) only
 * (kill, whiff or reversal alike): the crowd never reads the outcome, the victim's kind or OnPawnKilled for a reaction.
 */

/** The R8 reaction a responder drew. */
enum class ECrowdStrikeReaction : uint8
{
	None,
	/** Look at the strike point 1-3 s (never tracking the attacker afterwards). */
	Glance,
	/** Stop and face the strike point 2-5 s. */
	Pause,
	/** A new leg away from the strike point, at walk. */
	Reroute,
	/** A short run (<= CrowdPanicRunMaxSec) away: only within CrowdStartleRadiusCm, counted in the T1 budget. */
	PanicRun,
	/** A startle brace: only within CrowdStartleRadiusCm, counted in the T4 budget. */
	StartleBrace,
	Count,
};

inline const TCHAR* LexToString(ECrowdStrikeReaction Reaction)
{
	switch (Reaction)
	{
	case ECrowdStrikeReaction::None: return TEXT("None");
	case ECrowdStrikeReaction::Glance: return TEXT("Glance");
	case ECrowdStrikeReaction::Pause: return TEXT("Pause");
	case ECrowdStrikeReaction::Reroute: return TEXT("Reroute");
	case ECrowdStrikeReaction::PanicRun: return TEXT("PanicRun");
	case ECrowdStrikeReaction::StartleBrace: return TEXT("StartleBrace");
	default: return TEXT("?");
	}
}

/** One qualifying NPC of a strike (unbound, started, outside its refractory window, within the radius). */
struct FCrowdStrikeCandidate
{
	uint32 Ordinal = 0;
	double DistanceCm = 0.0;
	/** The counter-based priority draw keyed by (ordinal, strike index) (CNB-R8, R14): lowest responds first. */
	double Priority = 0.0;
};

/** A strike being fanned out: its candidates in priority order and how many have responded. */
struct FCrowdPendingStrike
{
	uint64 StrikeIndex = 0;
	double StimulusTime = 0.0;
	FVector Point = FVector::ZeroVector;
	uint64 SourceKey = 0;
	TArray<FCrowdStrikeCandidate> Candidates;
	int32 Next = 0;
	int32 Responders = 0;
};

/**
 * FCrowdStrikeFanOut: picks a strike's responders (CNB-R8). Every qualifying NPC gets a priority from the counter-based
 * draw keyed by (ordinal, strike index), never by distance, appearance or kind; candidates are tried in priority order
 * and each one whose eye has line of sight to the strike point responds, until CrowdStrikeResponderLimit have. The line
 * of sight is the caller's (budgeted rays in the game; deferred rays keep the stimulus time, so the reaction delay still
 * runs from the stimulus). Plain C++, used by the Director and the headless sim alike.
 */
class STEALTHGAMEPLAY_API FCrowdStrikeFanOut
{
public:
	/** Queues a strike. Candidates are pre-filtered by the caller (radius, unbound, started, refractory). */
	void Begin(double StimulusTime, const FVector& Point, uint64 SourceKey, TArray<FCrowdStrikeCandidate> Candidates, FCrowdDraws& Draws);

	/**
	 * Tests candidates in priority order within RayBudget line-of-sight tests; Respond is called for each responder (the
	 * strike, the candidate). Returns the rays used. Strikes complete in order; a finished strike is dropped.
	 */
	int32 Process(int32 RayBudget, int32 ResponderLimit, TFunctionRef<bool(const FCrowdPendingStrike&, const FCrowdStrikeCandidate&)> Visible,
		TFunctionRef<void(const FCrowdPendingStrike&, const FCrowdStrikeCandidate&)> Respond);

	bool HasPending() const { return Pending.Num() > 0; }
	uint64 GetStrikeCount() const { return NextStrikeIndex; }
	void Reset() { Pending.Reset(); NextStrikeIndex = 0; }

	/** The keyed priority draw (exposed for the spec that pins it). */
	static double PriorityOf(const FCrowdDraws& Draws, uint32 Ordinal, uint64 StrikeIndex);

private:
	TArray<FCrowdPendingStrike> Pending;
	uint64 NextStrikeIndex = 0;
};
