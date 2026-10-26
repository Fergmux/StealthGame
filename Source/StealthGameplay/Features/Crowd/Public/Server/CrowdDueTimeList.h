#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/** What a due-time entry does when it falls due (ADR-0016 section 3). */
enum class ECrowdDueKind : uint8
{
	/** A pause (C2), conversation (C3) or reaction pause ends. */
	PauseEnd,
	/** A glance or hold (G2-G6) ends. */
	GlanceEnd,
	/** The next G2 glance is due. */
	GlanceDue,
	/** A goods look (G3) ends. */
	GoodsEnd,
	/** A reaction or salient glance starts (StimulusTime + Delay). */
	ReactionStart,
	/** The yield jitter ends and the side-step starts (R3.3). */
	YieldJitterEnd,
	/** A red-herring tell clock fires. */
	TellDue,
	/** An unanswered conversation initiator turns away (C3). */
	ConverseTurnAway,
	/** The pause-context look ends (a long pause facing a stall, mural or view). */
	ContextLookEnd,
	/** Group drift (C4) pace matching ends. */
	DriftEnd,
};

/** One entry: due server time, kind and a small payload (an ordinal, a tell index). */
struct FCrowdDueEntry
{
	double Due = 0.0;
	ECrowdDueKind Kind = ECrowdDueKind::PauseEnd;
	uint32 Payload = 0;
	/** Increments per scheduling of a kind, so a superseded entry is recognised and skipped. */
	uint32 Generation = 0;
};

/**
 * FCrowdDueTimeList: the per-controller behaviour clock list (ADR-0016 section 3; story cnb-012). Every behaviour delay
 * (pause end, glance hold end, red-herring due, reaction start, yield jitter end) is an entry checked every controller
 * tick against the server time, so an entry acts in the controller tick in which it falls due (error at most one frame)
 * whatever the decision rate. Never a decision-tick count, never the engine timer manager. Small and allocation-free
 * after warm-up (an inline array).
 *
 * Usage:
 *   List.Schedule(ECrowdDueKind::PauseEnd, Now + Pause);
 *   while (const FCrowdDueEntry* Entry = List.PopDue(Now)) { ... }   // in the controller tick
 */
class STEALTHGAMEPLAY_API FCrowdDueTimeList
{
public:
	/**
	 * Schedules Kind at Due. bReplaceSameKind (default) cancels a pending entry of the same kind first (one pause end,
	 * one glance end at a time). Returns the entry's generation.
	 */
	uint32 Schedule(ECrowdDueKind Kind, double Due, uint32 Payload = 0, bool bReplaceSameKind = true);

	/** Cancels every pending entry of Kind. */
	void Cancel(ECrowdDueKind Kind);

	/** Cancels everything (Halt, release). */
	void Clear() { Entries.Reset(); }

	/** Pops the earliest entry due at or before Now (null when none is due); stable for equal due times. */
	bool PopDue(double Now, FCrowdDueEntry& Out);

	/** True when an entry of Kind is pending. */
	bool IsPending(ECrowdDueKind Kind) const;

	/** The due time of the pending entry of Kind, or a negative value. */
	double GetDue(ECrowdDueKind Kind) const;

	int32 Num() const { return Entries.Num(); }

private:
	TArray<FCrowdDueEntry, TInlineAllocator<12>> Entries;
	uint32 NextGeneration = 1;
};
