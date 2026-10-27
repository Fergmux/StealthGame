#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/CrowdDraws.h"

struct FCrowdTuning_Server;

/** The Tell Matrix rows the crowd plays as red herrings (GDD CNB-R16 T1-T5, T12, T13; F2). Values index arrays. */
enum class ECrowdTell : uint8
{
	Run,
	Vault,
	Climb,
	Brace,
	DecoyTend,
	Bush,
	Reversal,
	HeadHold,
	Count,
};

/** The assassination strike's NPC rate: 0 by design, the intended confession (GDD CNB-R7, T6). A constant, not a key. */
inline constexpr double CrowdStrikeTellRatePerMin = 0.0;

/**
 * Calibration counters (story cnb-018; GDD AC-CNB-10 reads them through story cnb-023): plain integers per tell, held
 * server-side for the ADR-0023 telemetry section. Never logged (no UE_LOG / UE_LOGF), never replicated.
 */
struct FCrowdCalibrationCounters
{
	/** Clock firings (draws), discarded draws inside a chain gap, accepted tells, tells actually performed. */
	int64 Draws[static_cast<int32>(ECrowdTell::Count)] = {};
	int64 DiscardedInGap[static_cast<int32>(ECrowdTell::Count)] = {};
	int64 Accepted[static_cast<int32>(ECrowdTell::Count)] = {};
	int64 Performed[static_cast<int32>(ECrowdTell::Count)] = {};
	/** R8 reactions that would have been a tell inside the responder's gap and became a glance. */
	int64 ReactionsDowngraded = 0;
	/** Accepted tells not shown: a different tell was shown within the gap at the visible moment, or no chance came. */
	int64 Dropped[static_cast<int32>(ECrowdTell::Count)] = {};
};

/**
 * The server-only telemetry seam the counters reach (ADR-0023). The telemetry section of the Event Log store does not
 * exist yet; the Director keeps an in-memory FCrowdCalibrationCounters and forwards through this seam when one is
 * registered. Implementations must never log the values.
 */
class ICrowdTelemetrySink
{
public:
	virtual ~ICrowdTelemetrySink() = default;
	virtual void AddCrowdCounter(ECrowdTell Tell, int32 Field, int64 Delta) = 0;
};

/**
 * FCrowdTellSampler: compensated tell clocks and the chain-gap guard (GDD CNB-R7, F2; ADR-0016 section 9; story
 * cnb-018). Each enabled tell j runs an exponential clock per NPC at F2's compensated draw rate
 *   a_j = r_j / (1 - (Lambda - r_j) * T_gap)     r_j = 60 / Crowd<Tell>IntervalSec per minute, Lambda = sum r_j,
 *                                                  T_gap = CrowdTellChainGapSec / 60 min,
 * through UnitFraction53 and the exponential inverse CDF on the tell's own decision type. A draw that falls inside the
 * NPC's chain gap (another, different tell less than T_gap ago) is discarded and that clock restarts at the gap's end;
 * otherwise it is accepted. Rates come from the frozen snapshot and never change during a round (AC-CNB-37).
 * O(1) per NPC per tell; plain C++.
 */
class STEALTHGAMEPLAY_API FCrowdTellSampler
{
public:
	/** Per-minute accepted target rates r_j from the interval keys (F2 defaults: run 0.148, ..., head hold 0.10). */
	static void TargetRatesPerMin(const FCrowdTuning_Server& Tuning, double OutRates[static_cast<int32>(ECrowdTell::Count)]);

	/** F2's compensated draw rate a_j per minute (never a key). */
	static double CompensatedRatePerMin(const FCrowdTuning_Server& Tuning, ECrowdTell Tell);

	/**
	 * The draw rate this sampler's clocks run at: F2's a_j with Lambda summed over the ENABLED tells only. Only enabled
	 * clocks can discard one another, so compensating for a disabled tell's gap would overshoot r_j (while story cnb-019 to
	 * cnb-022 tells are not yet built). With every tell enabled it equals CompensatedRatePerMin.
	 */
	double EffectiveRatePerMin(const FCrowdTuning_Server& Tuning, ECrowdTell Tell) const;

	/** The decision type of a tell's clock. */
	static ECrowdDecision ClockDecision(ECrowdTell Tell);

	/** Enables the tells whose behaviours exist (the clocks of disabled tells never run). */
	void SetEnabled(ECrowdTell Tell, bool bEnabled);
	bool IsEnabled(ECrowdTell Tell) const { return bEnabledTells[static_cast<int32>(Tell)]; }

	/** Starts every enabled clock of an NPC at Now (round start, join). */
	void StartNpc(uint32 Ordinal, double Now, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws);
	void RemoveNpc(uint32 Ordinal) { Npcs.Remove(Ordinal); }

	/**
	 * Advances Ordinal's clocks to Now: every clock that fired is drawn again (discarded inside the gap: restarted at the
	 * gap end; accepted: restarted at Now). Accepted tells are appended to OutAccepted for the behaviour to perform.
	 */
	void Advance(uint32 Ordinal, double Now, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws, TArray<ECrowdTell, TInlineAllocator<2>>& OutAccepted);

	/**
	 * True when a different tell than Tell was accepted (or shown by an R8 reaction) less than the chain gap before Now:
	 * the anchor of the clocks' discard rule, F2's compensation assumes exactly this.
	 */
	bool IsInChainGap(uint32 Ordinal, ECrowdTell Tell, double Now, const FCrowdTuning_Server& Tuning) const;

	/**
	 * True when a different tell than Tell was SHOWN less than the chain gap before Now (the visible-moment rule of
	 * AC-CNB-31: a tell whose behaviour takes time to reach its visible moment is shown later than it was accepted, and
	 * another tell may not be shown inside the gap around it; the agent defers instead).
	 */
	bool IsInShownGap(uint32 Ordinal, ECrowdTell Tell, double Now, const FCrowdTuning_Server& Tuning) const;

	/**
	 * Records a tell the NPC showed. bFromClock: an accepted clock tell (its acceptance already anchored the gap, and the
	 * anchor stays there so F2's compensation holds); false for an R8 panic run or startle brace, which anchors the
	 * clocks' gap at Now.
	 */
	void NotePerformed(uint32 Ordinal, ECrowdTell Tell, double Now, bool bFromClock = false);

	/** Records an accepted tell that was not shown (telemetry field 5); it is no longer pending. */
	void NoteDropped(uint32 Ordinal, ECrowdTell Tell);

	/**
	 * Show-anchored mode (the crowd's worlds; GDD F2 as amended 2026-10-27): an accepted tell stays pending until it is
	 * shown or dropped, and while one is pending every other clock's draw is discarded (restarting after the gap), so the
	 * gap is anchored at the visible moment and no other tell piles up behind a tell on its way (AC-CNB-31). The
	 * compensated draw rate then counts each other tell's mean show delay in its gap (ShowDelaySec). Off (the default),
	 * the sampler is the acceptance-anchored clock set of story cnb-018.
	 */
	void SetShowAnchored(bool bInShowAnchored) { bShowAnchored = bInShowAnchored; }
	bool IsShowAnchored() const { return bShowAnchored; }
	/** True while Tell, accepted for Ordinal, waits for its visible moment. */
	bool IsPending(uint32 Ordinal, ECrowdTell Tell) const;

	/** Records an R8 reaction downgraded to a glance by the chain gap. */
	void NoteReactionDowngraded() { ++Counters.ReactionsDowngraded; if (Telemetry) { Telemetry->AddCrowdCounter(ECrowdTell::Count, 4, 1); } }

	const FCrowdCalibrationCounters& GetCounters() const { return Counters; }
	void SetTelemetrySink(ICrowdTelemetrySink* InSink) { Telemetry = InSink; }

	/** The next due time of Ordinal's Tell clock (negative when not running). */
	double GetNextDue(uint32 Ordinal, ECrowdTell Tell) const;

private:
	struct FNpcClocks
	{
		double NextDue[static_cast<int32>(ECrowdTell::Count)];
		double LastTell[static_cast<int32>(ECrowdTell::Count)];
		double LastShown[static_cast<int32>(ECrowdTell::Count)];
		bool bPending[static_cast<int32>(ECrowdTell::Count)] = {};
		FNpcClocks()
		{
			for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
			{
				NextDue[Index] = -1.0;
				LastTell[Index] = -1.0e9;
				LastShown[Index] = -1.0e9;
			}
		}
	};
	void Count(int32 Field, ECrowdTell Tell);

	TMap<uint32, FNpcClocks> Npcs;
	bool bEnabledTells[static_cast<int32>(ECrowdTell::Count)] = {};
	bool bShowAnchored = false;
	FCrowdCalibrationCounters Counters;
	ICrowdTelemetrySink* Telemetry = nullptr;
};
