#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Random/StealthSecretStream.h"
#include "View/Random/StealthRandomTag.h"

/**
 * Crowd behavioural randomness (ADR-0016 section 9, GDD CNB-R14; story cnb-002). Every behavioural draw of the crowd is
 * counter-based: Draw(Ordinal, Decision) reads the secret stream's counter tuple {Ordinal, Decision, DecisionIndex,
 * Retry} (ADR-0006 section 5, mode 0x02), where DecisionIndex is the pawn's own count of draws of that decision type.
 * A pawn's k-th draw of a decision type is therefore fixed whatever else happened in the world (kills, other pawns,
 * other decision types), and a rejection retry extends the Retry field, never the index.
 *
 * Plain C++, server only. The counters are keyed by the factory's spawn ordinal and live with the Director (story
 * cnb-004), never on a controller (which is destroyed on possession), so streams travel with a pawn through takeover
 * and release. Never log the sub-seed or the stream (ADR-0006 section 7).
 *
 * Usage:
 *   FCrowdDraws Draws(Session->GetServerSubSeed(ESeedDomain::CrowdBehaviour, LayoutAttempt));
 *   const double Pause = Draws.DrawUniform(Ordinal, ECrowdDecision::PauseLength, 1.0, 3.0);
 */

/**
 * The decision types (ADR-0016 section 9). Append-only with fixed values: never renumber or reuse a value, because the
 * value is part of every draw's counter tuple.
 */
enum class ECrowdDecision : uint16
{
	TickPhase = 1,
	DeviceProfile = 2,
	LaneOffset = 3,
	Destination = 4,
	PauseKind = 5,
	PauseLength = 6,
	Pace = 7,
	Yield = 8,
	YieldJitter = 9,
	GlanceTarget = 10,
	GlanceHold = 11,
	GazeScan = 12,
	ConverseStart = 13,
	ConverseReply = 14,
	ConverseLength = 15,
	ReactionKind = 16,
	ReactionDelay = 17,
	RunClock = 18,
	VaultClock = 19,
	ClimbClock = 20,
	BraceClock = 21,
	DecoyClock = 22,
	DecoyCourt = 23,
	DecoyRelease = 24,
	BushClock = 25,
	BushStay = 26,
	ReversalClock = 27,
	HeadHoldClock = 28,
	ExitPick = 29,
	// Appended by the crowd build (2026-10-26, stories cnb-009 to cnb-018); fixed values from here on.
	FirstActivity = 30,
	LegLength = 31,
	GlanceInterval = 32,
	GoodsHold = 33,
	ConverseTurnAway = 34,
	HeadOnCoin = 35,
	CameraPush = 36,
	TurnRate = 37,
	GoodsPitch = 38,
	StrikePriority = 39,
	SalientGlance = 40,
	PartnerLook = 41,
	GroupDrift = 42,
	StartCell = 43,
	DestinationJitter = 44,
	HeadHoldLength = 45,
	CameraLead = 46,
	ConverseFaceGap = 47,
	/** One past the last value (array sizing only; never drawn). */
	Count = 48,
};

/** The stream tag of the crowd behaviour stream (ADR-0016 section 9). */
inline constexpr uint64 CrowdBehaviourStreamTag = StealthRandom::Tag("CNB_BHVR");

/**
 * Per-ordinal decision counters: one uint32 per decision type, in a flat array keyed by spawn ordinal (O(1),
 * allocation-free once an ordinal exists).
 */
class STEALTHGAMEPLAY_API FCrowdDecisionCounters
{
public:
	/** The next DecisionIndex for (Ordinal, Decision), advancing the counter. */
	uint32 Take(uint32 Ordinal, ECrowdDecision Decision);

	/** The number of draws of Decision taken so far by Ordinal. */
	uint32 Peek(uint32 Ordinal, ECrowdDecision Decision) const;

	/** Pre-sizes the store for ordinals below Count (optional; Take grows it). */
	void Reserve(int32 Count);

	/** Forgets every counter (a new round). */
	void Reset() { Rows.Reset(); }

private:
	struct FRow
	{
		uint32 Index[static_cast<int32>(ECrowdDecision::Count)] = {};
	};
	TArray<FRow> Rows;
};

/**
 * The crowd's counter-based draws over FSecretStream(SubSeed, CNB_BHVR). Integer draws are Draw / DrawBelow /
 * DrawWeighted; continuous samples map one 64-bit draw to a unit fraction (UnitFraction53, never 0 or 1) and a
 * closed-form inverse CDF (exponential, uniform, log-normal through InvNormalAS241). Every function consumes exactly one
 * DecisionIndex of its (Ordinal, Decision).
 */
class STEALTHGAMEPLAY_API FCrowdDraws
{
public:
	/** SubSeed: GetServerSubSeed(CrowdBehaviour, LayoutAttempt) of the round. */
	explicit FCrowdDraws(uint64 SubSeed);

	/** Raw 64-bit draw: tuple {Ordinal, Decision, DecisionIndex++, Retry = 0}. */
	uint64 Draw(uint32 Ordinal, ECrowdDecision Decision);

	/**
	 * Uniform integer in [0, N) by Lemire's multiply-shift with rejection; each retry reads {Ordinal, Decision, Index,
	 * Retry + 1} with the same DecisionIndex. N == 0 returns 0 (still consuming one index).
	 */
	uint32 DrawBelow(uint32 Ordinal, ECrowdDecision Decision, uint32 N);

	/**
	 * Weighted pick: CumWeights are the running sums of the integer weights (last = total, > 0). Returns the first index
	 * whose cumulative weight exceeds a DrawBelow(total) value; -1 for an empty or zero-total table.
	 */
	int32 DrawWeighted(uint32 Ordinal, ECrowdDecision Decision, TConstArrayView<uint32> CumWeights);

	/** u = UnitFraction53(Draw(...)) in (0, 1). */
	double DrawUnit(uint32 Ordinal, ECrowdDecision Decision);

	/** True with probability P (u < P). */
	bool DrawChance(uint32 Ordinal, ECrowdDecision Decision, double P) { return DrawUnit(Ordinal, Decision) < P; }

	/** Uniform in [A, B): A + u (B - A). */
	double DrawUniform(uint32 Ordinal, ECrowdDecision Decision, double A, double B);

	/** Exponential with rate Lambda (per second): -ln(1 - u) / Lambda. Lambda <= 0 returns +infinity. */
	double DrawExponential(uint32 Ordinal, ECrowdDecision Decision, double Lambda);

	/** Log-normal: exp(Mu + Sigma * InvNormalAS241(u)). */
	double DrawLogNormal(uint32 Ordinal, ECrowdDecision Decision, double Mu, double Sigma);

	/** The counters (read-only; the Director owns this object). */
	const FCrowdDecisionCounters& GetCounters() const { return Counters; }

	/** Retries taken by the last DrawBelow (tests prove retries extend Retry only). */
	uint32 GetLastRetryCount() const { return LastRetryCount; }

	/** The raw tuple value for an explicit (Ordinal, Decision, Index, Retry), without touching a counter (tests). */
	uint64 PeekTuple(uint32 Ordinal, ECrowdDecision Decision, uint32 Index, uint32 Retry) const;

	/** ((Bits >> 11) + 0.5) * 2^-53: a double in (0, 1), never 0 or 1. */
	static double UnitFraction53(uint64 Bits);

	/** The standard normal quantile Phi^-1(U) by Wichura's AS241 (PPND16), U in (0, 1). */
	static double InvNormalAS241(double U);

private:
	FSecretStream Stream;
	FCrowdDecisionCounters Counters;
	uint32 LastRetryCount = 0;
};
