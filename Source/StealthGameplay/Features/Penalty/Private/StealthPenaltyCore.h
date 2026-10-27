#pragma once

#include "CoreMinimal.h"
#include "Server/EncounterTypes.h"
#include "Server/StealthFaultSink.h"
#include "Server/StealthPenaltyEventRows.h"
#include "Server/StealthPenaltySeams.h"
#include "Server/StrikeTallySource.h"

class IStealthClock;
class IRoundLogWriter;

/** Penalty's per-participant sub-state (GDD States and Transitions; ADR-0019 section 1). The Roster state is authoritative. */
enum class EInformantLifeState : uint8
{
	Active,
	Awaiting,
	Picking,
	Embodied,
	Absent,
	RoundEnded,
};

/**
 * One participant's Penalty record (ADR-0019 section 1 table), keyed by ParticipantId in FPenaltyCore. Plain C++, never a
 * USTRUCT and never replicated: StrikeCount is ServerOnly (it never reaches any client, its owner included, before Ended);
 * the tally reaches its owner only through FStrikeResult.TallyAfter and the owner re-push (PEN-R3). The respawn fields
 * (LostServerTime .. ExcludePawn) are declared now so later stories (009-013) do not reshape the record.
 */
struct FPenaltyRecord
{
	/** k: ungraced NPC kills this round (F1), 0..L. */
	uint8 StrikeCount = 0;
	/** t: every WrongTarget result this round, saturating at 255 (F1). */
	uint8 WrongTargetTally = 0;
	/** The ClientCommandSeq of the last FStrikeResult that changed the tally (the re-push's LastResultSeq). */
	FClientCommandSeq LastResultSeq;
	EInformantLifeState LifeState = EInformantLifeState::Active;
	/** t_lost (F2), server seconds; 0 = none. */
	double LostServerTime = 0.0;
	/** The fall-plus-fade floor of T_due (F2), server seconds; 0 = none. */
	double FloorServerTime = 0.0;
	/** T_due (F2), server seconds; 0 = none. */
	double DueServerTime = 0.0;
	/** The respawn request's sequence (the pick stream continues with it, PEN-R9). */
	uint32 RespawnRequestSeq = 0;
	double PickStartServerTime = 0.0;
	/** The starvation widening level reached (0..2, PEN-R10). */
	uint8 LastWideningLevel = 0;
	/** The pending InformantRespawn item's handle value (0 = none; story nkp-009). */
	uint64 TimerHandle = 0;
	/** The participant's own released body: never re-adopted (PEN-R5, CR-5). */
	FPenaltyPawnKey ExcludePawn;
};

/** One OnPawnKilled, as the core sees it (the service resolves the killer's participant inside the slot). */
struct FPenaltyKill
{
	FPenaltyPawnKey Victim;
	/** GetParticipant(KillerPawn); invalid for a killer with no participant (a bracing NPC's reversal). */
	FParticipantId Killer;
	EVictimKind VictimKind = EVictimKind::Npc;
	EKillCause Cause = EKillCause::Strike;
};

/** What OnPawnKilled did (an observation for tests and the service; never sent). */
enum class EPenaltyKillOutcome : uint8
{
	/** Not a strike (wrong cause, victim kind, killer, graced, or frozen). */
	NoStrike,
	/** A graced released body: no strike, the tally still rises at the result (PEN-R5). */
	Graced,
	/** One strike added. */
	Strike,
	/** One strike added and it reached L: the exile is due in this slot (sequence: story nkp-005). */
	StrikeAndExile,
	/** An Informant participant as the killer: a defect, one fault, nothing counted. */
	InformantKillerFault,
};

/** Everything the core is injected with (Clock, Roster and Tuning required; the rest optional). */
struct FPenaltyCoreDeps
{
	const IStealthClock* Clock = nullptr;
	const IPenaltyRosterQuery* Roster = nullptr;
	const IPenaltyTuningSource* Tuning = nullptr;
	const IPenaltySlotContext* Slot = nullptr;
	/** Round's Event Log writer (null: rows are not written, one DeadRegistration fault each). */
	IRoundLogWriter* Log = nullptr;
	/** The Roster owner-view feed (null until roster story psr-038: nothing is re-pushed). */
	IPenaltyOwnerViewFeed* OwnerView = nullptr;
	IStealthFaultSink* Faults = nullptr;
};

/**
 * FPenaltyCore: the NPC-Kill Penalty & Informants plain-C++ core (GDD npc-kill-penalty-informants.md PEN-R2..R6, F1;
 * ADR-0019 sections 1, 2.1, 2.2, 2.2a, 7; stories nkp-001 to nkp-004). No UWorld, no UObject, no engine singleton: every
 * input arrives through FPenaltyCoreDeps, so StealthGame.Penalty.* runs it headless. Every mutator is O(1) and runs inside
 * the queue slot that produced the fact (ADR-0019 guardrail 0.5 ms); nothing here uses a timer, a tick or a latent action.
 *
 * Lifetime of the values (PEN-R6): round-scoped and keyed by ParticipantId; disconnect, reclaim, readmission and travel
 * touch neither counter; OnRosterReset returns every record to Active(0) with tally 0 and clears the released bodies;
 * OnRosterRoundEnded freezes the store (every later mutator is a no-op) until the reset.
 *
 * Usage (the service, in the Attack's slot):
 *   Core.OnPawnKilled({ VictimKey, KillerParticipant, Kind, Cause });                 // OnPawnKilled fan-out
 *   const FStrikeTally Tally = Core.GetTallyForResult(KillerParticipant, Result, Seq);  // IStrikeTallySource, after it
 */
class STEALTHGAMEPLAY_API FPenaltyCore
{
public:
	/** Builds the store and latches the round values once (the first round may start before a Preparing is seen). */
	explicit FPenaltyCore(const FPenaltyCoreDeps& InDeps);

	// --- Round lifetime -----------------------------------------------------------------------------------------

	/** Round entered Preparing: latch L and the release grace for the round (TDC rule CR-11 Latching; PEN-R4). */
	void OnRoundPreparing();
	/** The latched L (never the live knob; no literal). */
	int32 GetLatchedStrikeLimit() const { return Latched.NpcKillStrikeLimit; }
	/** The latched StrikeOutReleaseGraceSec. */
	double GetLatchedReleaseGraceSec() const { return Latched.StrikeOutReleaseGraceSec; }

	/** OnRosterRoundEnded: every record RoundEnded, the store frozen (idempotent). */
	void OnRosterRoundEnded();
	/** OnRosterReset: every record Active(0), tally 0, the new epoch, released bodies cleared; then the owner re-push. */
	void OnRosterReset(uint32 NewRoundEpoch);
	bool IsFrozen() const { return bFrozen; }
	uint32 GetRoundEpoch() const { return RoundEpoch; }

	// --- Connection (PEN-R6 carry-over: no counter changes) -------------------------------------------------------

	/** The Roster projected Disconnected (or Connected again without a reclaim). Counters untouched. */
	void OnConnectionChanged(FParticipantId Participant, bool bConnected);
	/** The participant reclaimed its record: the owner re-push {tally, LastResultSeq} (PEN-R3, ADR-0019 section 2.2a). */
	void OnParticipantReclaimed(FParticipantId Participant);
	/** The participant's C-TeamView was rebound after a rejoin (Roster OnViewRebound): the owner re-push. */
	void OnViewRebound(FParticipantId Participant);
	/** A returning informant was readmitted (Round R18). Counters untouched; its respawn is story nkp-010. */
	void OnInformantReadmitted(FParticipantId Participant);

	// --- Strikes, tally (stories nkp-002, nkp-003) -----------------------------------------------------------------

	/**
	 * OnPawnKilled inline in the Attack's slot (PEN-R2, F1): k + 1 only for Cause = Strike, VictimKind = Npc, a killer
	 * whose Roster state is Active and a victim that is not a graced released body. An Informant killer faults once
	 * (ASN-R2 makes it a defect) and counts nothing; a killer with no participant counts nothing and reports nothing.
	 * When the strike reaches L the slot-local exile flag is set (bExiled of this Attack's result; story nkp-005 runs the
	 * exile itself).
	 */
	EPenaltyKillOutcome OnPawnKilled(const FPenaltyKill& Kill);

	/**
	 * IStrikeTallySource (ADR-0018 section 2.1 step 8): asked once per executed Attack after the fan-out. WrongTarget:
	 * t := min(t + 1, 255) and LastResultSeq := Seq; any other result: t unchanged. Returns {t, bExiled} where bExiled is
	 * true only for the Attack whose slot exiled the attacker (consumed here). Never reads VictimKind. Asserts k <= t.
	 */
	FStrikeTally GetTallyForResult(FParticipantId Attacker, EStrikeResult Result, FClientCommandSeq Seq);

	/**
	 * A body released to the crowd at ReleaseServerTime (the exile's PEN-R4.2 release, story nkp-005; tests seed it).
	 * Killed within the latched grace (inclusive) it adds no strike (PEN-R5).
	 */
	void RecordReleasedBody(FPenaltyPawnKey Body, double ReleaseServerTime);
	/** Graced(Victim) at SlotServerTime: a released body and SlotServerTime - release <= grace (inclusive). */
	bool IsGraced(FPenaltyPawnKey Victim, double SlotServerTime) const;

	// --- Server-only queries (tests, the service; never copied to a client-visible type) --------------------------

	const FPenaltyRecord* FindRecord(FParticipantId Participant) const { return Records.Find(Participant); }
	uint8 GetStrikeCount(FParticipantId Participant) const;
	uint8 GetWrongTargetTally(FParticipantId Participant) const;
	int32 GetReleasedBodyCount() const { return ReleasedBodies.Num(); }

private:
	FPenaltyRecord& FindOrAddRecord(FParticipantId Participant);
	void RepushOwnerTally(FParticipantId Participant, const FPenaltyRecord& Record);
	FQueueKey CurrentSlotKey() const;
	void ReportFault(EStealthFault Fault, int64 Detail) const;

	/** The StrikeCounted row of a kill, written when the same slot's tally result lands (it carries TallyAfter). */
	struct FPendingStrikeRow
	{
		FQueueKey SlotKey;
		FParticipantId Killer;
		uint8 StrikeAfter = 0;
		bool bGraced = false;
	};

	/** The slot-local exile flag (never a record field, so no later result can carry a stale true). */
	struct FSlotExile
	{
		FQueueKey SlotKey;
		FParticipantId Participant;
	};

	FPenaltyCoreDeps Deps;
	FPenaltyEventLog EventLog;
	FPenaltyParams Latched;
	TMap<FParticipantId, FPenaltyRecord> Records;
	/** Round-scoped: released body -> release server time (PEN-R5). */
	TMap<FPenaltyPawnKey, double> ReleasedBodies;
	TOptional<FPendingStrikeRow> PendingStrikeRow;
	TOptional<FSlotExile> SlotExile;
	uint32 RoundEpoch = 0;
	bool bFrozen = false;
};
