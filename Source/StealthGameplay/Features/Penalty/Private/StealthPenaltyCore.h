#pragma once

#include "CoreMinimal.h"
#include "Server/EncounterTypes.h"
#include "Server/StealthFaultSink.h"
#include "Server/StealthPenaltyEventRows.h"
#include "Server/StealthPenaltySeams.h"
#include "Server/StrikeTallySource.h"

class IStealthClock;
class IRoundLogWriter;

/**
 * One participant's Penalty record (ADR-0019 section 1 table), keyed by ParticipantId in FPenaltyCore. Plain C++, never a
 * USTRUCT and never replicated: StrikeCount is ServerOnly (it never reaches any client, its owner included, before Ended);
 * the tally reaches its owner only through FStrikeResult.TallyAfter and the owner re-push (PEN-R3). The respawn fields
 * (LostServerTime .. bClearExclusionsAtFire) are the lifecycle of stories nkp-009 to nkp-012; T_due reaches its owner only
 * through OnDueTimeChanged and the carrier adapter (story nkp-011).
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
	/** The starvation widening level LSC reported for this request (0..2, PEN-R10; never decreases within a request, story nkp-013). */
	uint8 LastWideningLevel = 0;
	/** The pending InformantRespawn item's handle value (0 = none; story nkp-009). */
	uint64 TimerHandle = 0;
	/** The participant's own released body: never re-adopted (PEN-R5, CR-5). */
	FPenaltyPawnKey ExcludePawn;
	/** F2 D of the current loss: InformantRespawnDelaySec, or 0 for a body lost without a kill (kept across Absent). */
	double RespawnDelaySec = 0.0;
	/** The cause of the current request (its RespawnScheduled row). */
	ERespawnCause RespawnCause = ERespawnCause::Assassinated;
	/** The request's short-lived exclusion set (ADR-0019 section 4.1 step 3): pawns that failed the pre-check, the release or the takeover. */
	TArray<FPenaltyPawnKey> RequestExclusions;
	/** True when the pending item is a regular EmergenceRetrySec retry: the exclusion set is cleared when it fires. */
	bool bClearExclusionsAtFire = false;
};

/** One OnPawnKilled, as the core sees it (the service resolves the killer's participant inside the slot). */
struct FPenaltyKill
{
	FPenaltyPawnKey Victim;
	/** GetParticipant(KillerPawn); invalid for a killer with no participant (a bracing NPC's reversal). */
	FParticipantId Killer;
	EVictimKind VictimKind = EVictimKind::Npc;
	EKillCause Cause = EKillCause::Strike;
	/** The killer's body (the exile releases it, story nkp-005); unset in tests that exercise no exile. */
	FPenaltyPawnKey KillerPawn;
	/**
	 * The killer's strike lockout end (contact slot time + AttackAttemptLockoutSec), read from Assassination's action
	 * state, never recomputed here: the released body's RestrictedUntilServerTime and its AI possession time.
	 */
	double KillerLockoutEndServerTime = -1.0;
	/** GetParticipant(Victim) before the kill (an informant victim's participant, story nkp-008); invalid for an NPC. */
	FParticipantId VictimParticipant;
	/** The victim's death write time (its ContactServerTime): the informant retire's RetireAtServerTime (psr-021). */
	double VictimDeathServerTime = 0.0;
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
	/**
	 * The exile's and the informant kill's calls (stories nkp-005, nkp-008). Null: the core decides only (the exile flag
	 * bExiled of story nkp-003 is still set, nothing is called; the Logic tests of stories nkp-001..004 run so).
	 */
	IPenaltyBodyGateway* Bodies = nullptr;
	/** The due-time heap (story nkp-009; null: nothing is scheduled and each scheduling reports one DeadRegistration fault). */
	IPenaltyRespawnTimers* Timers = nullptr;
	/** The respawn commit's gateways (story nkp-012; null: a fired item retries after EmergenceRetrySec). */
	IPenaltyRespawnGateway* Respawn = nullptr;
	/** OnDueTimeChanged (story nkp-011's carrier adapter listens; null: not raised). */
	IPenaltyDueTimeSink* DueTimes = nullptr;
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

	/**
	 * The Roster projected Disconnected (or Connected again without a reclaim). Counters untouched (PEN-R6). Disconnected
	 * while Awaiting / Picking: the item is cancelled (Cancel(TimerHandle)), t_lost and t_floor kept, state Absent,
	 * RespawnCancelled{Disconnect} logged, T_due unset; while Embodied (Round R17.5 releases the pawn in this slot): Absent
	 * with t_lost = t_floor = now and no item (stories nkp-009, nkp-010).
	 */
	void OnConnectionChanged(FParticipantId Participant, bool bConnected);
	/** The participant reclaimed its record: the owner re-push {tally, LastResultSeq} (PEN-R3, ADR-0019 section 2.2a). */
	void OnParticipantReclaimed(FParticipantId Participant);
	/** The participant's C-TeamView was rebound after a rejoin (Roster OnViewRebound): the owner re-push. */
	void OnViewRebound(FParticipantId Participant);
	/**
	 * A returning informant was readmitted (Round R18, after its Session acknowledgement; story nkp-010). Counters untouched.
	 * From Absent: T_due = max(t_lost + D, t_floor, now) from the KEPT t_lost and t_floor (never restarted, never skipped):
	 * Awaiting with a new item when now < T_due, else Picking(0) and the commit runs in this slot.
	 */
	void OnInformantReadmitted(FParticipantId Participant);

	// --- Respawn lifecycle (stories nkp-009, nkp-010, nkp-012; ADR-0019 sections 3, 4) -------------------------------

	/**
	 * Round's IInformantBodyProvider::OnInformantAwaitingBody (after its elimination check, never when latched): the only
	 * scheduler of the Active -> Informant causes Assassinated, ReversedAgainst and StrikeLimit. A dying body's floor reads
	 * the death time recorded by this slot's OnPawnKilled (ADR-0018 section 4.1); the exile's body does not fall.
	 */
	void OnInformantAwaitingBody(FParticipantId Participant, ERespawnCause Cause, double LostServerTime);
	/**
	 * Round R17.9's informant case (IInformantBodyProvider::OnInformantPawnLost): an Embodied informant's body lost without
	 * a kill. T_due = now (D = 0), PawnLost{Participant, Cause} logged, Picking(0) and the commit in this slot; strike and
	 * tally unchanged. Disconnected: Absent instead. Any other state: an idempotent no-op (a second report).
	 */
	void OnInformantPawnLost(FParticipantId Participant, EPenaltyPawnLostCause Cause);
	/**
	 * Round's grace expiry turned an Active participant into Informant(Disconnect) and released its pawn (Round R17.4; the
	 * Roster state event): Absent with t_lost = t_floor = now and no item (story nkp-010). Never schedules anything.
	 */
	void OnGraceExpired(FParticipantId Participant);
	/**
	 * The InformantRespawn item fired (the queue adapter's resolver, inside its slot): re-validated against the request
	 * (sequence, state, epoch) and then the commit (story nkp-012). False (Voided) for a stale or cancelled item.
	 */
	bool ResolveRespawnItem(FParticipantId Participant, uint32 RequestSeq, uint32 ItemRoundEpoch);

	/** F2: T_due = max(t_lost + D, t_floor, t_now). */
	static double ComputeDueTime(double LostServerTime, double DelaySec, double FloorServerTime, double ServerTimeNow);
	/** F2: t_floor = DeathServerTime + DeathFallSec + NoPawnFadeSec for a dying body (DeathServerTime set), else t_lost. */
	static double ComputeFloor(TOptional<double> DeathServerTime, double DeathFallSec, double NoPawnFadeSec, double LostServerTime);
	/**
	 * The next-frame retry offset (ADR-0019 section 4.3, "ServerNow + 1 ms", meaning strictly after this drain). The queue
	 * stamps every item due at or before DrainStart + DueEpsilonSec (1 ms, inclusive) in the same drain, so the smallest
	 * offset that really lands in the next frame is twice the due tolerance: 2 ms.
	 */
	static double NextFrameRetryOffsetSec();
	/** The latched F2 and retry values. */
	const FPenaltyParams& GetLatchedParams() const { return Latched; }

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
	 * AttackerPawn: after an exile the attacker's body is Unbound, so the Roster no longer names its participant; the
	 * core then answers for the participant it released that body for in this slot (story nkp-005).
	 */
	FStrikeTally GetTallyForResult(FParticipantId Attacker, EStrikeResult Result, FClientCommandSeq Seq, FPenaltyPawnKey AttackerPawn = FPenaltyPawnKey());

	/**
	 * A body released to the crowd at ReleaseServerTime (the exile's PEN-R4.2 release, story nkp-005; tests seed it).
	 * Killed within the latched grace (inclusive) it adds no strike (PEN-R5).
	 */
	void RecordReleasedBody(FPenaltyPawnKey Body, double ReleaseServerTime);
	/** Graced(Victim) at SlotServerTime: a released body and SlotServerTime - release <= grace (inclusive). */
	bool IsGraced(FPenaltyPawnKey Victim, double SlotServerTime) const;

	// --- Server-only queries (tests, the service; never copied to a client-visible type) --------------------------

	const FPenaltyRecord* FindRecord(FParticipantId Participant) const { return Records.Find(Participant); }
	/** The participant's Penalty sub-state (Active for an unknown participant). */
	EInformantLifeState GetLifeState(FParticipantId Participant) const;
	/** T_due while one is pending (Awaiting or Picking), else unset. */
	TOptional<double> GetDueServerTime(FParticipantId Participant) const;
	uint8 GetStrikeCount(FParticipantId Participant) const;
	uint8 GetWrongTargetTally(FParticipantId Participant) const;
	int32 GetReleasedBodyCount() const { return ReleasedBodies.Num(); }
	/** The release server time of a released body (unset when Body was never released this round). */
	TOptional<double> FindReleaseTime(FPenaltyPawnKey Body) const;

private:
	FPenaltyRecord& FindOrAddRecord(FParticipantId Participant);
	/**
	 * The exile (PEN-R4; ADR-0019 section 2.3 steps 1-5; story nkp-005), one call stack inside the killing Attack's slot:
	 * RequestTransition(StrikeLimit) (the Roster releases the body), ReleaseCard, AdoptReleasedPawn(lockout end, exit at
	 * the next scheduled slot), RequestHandOver(Ai), then ReleasedBodies, ExcludePawn and the slot-local bExiled. A
	 * rejected step 1 reports one fault and runs nothing else (no bExiled). The respawn is NOT scheduled here: Round's
	 * OnInformantAwaitingBody does that after its elimination check (story nkp-009). Returns true when it ran.
	 */
	bool RunExile(const FPenaltyKill& Kill, FQueueKey SlotKey, double SlotServerTime);
	/** The informant kill (PEN-R7; ADR-0019 section 2.1; story nkp-008): RetireBinding inline, then ScheduleRespawn. */
	void RetireInformant(const FPenaltyKill& Kill, double SlotServerTime);
	void RepushOwnerTally(FParticipantId Participant, const FPenaltyRecord& Record);
	/**
	 * ADR-0019 section 3.2: F2, then Absent (Disconnected: t_lost and t_floor kept, no item) or Awaiting with one item, the
	 * RespawnScheduled row and OnDueTimeChanged. DeathServerTime: the dying body's death write time (unset: no fall).
	 */
	void ScheduleRespawn(FParticipantId Participant, ERespawnCause Cause, double LostServerTime, TOptional<double> DeathServerTime);
	/** Arms a new request (a fresh RespawnRequestSeq) due at DueServerTime: Awaiting, item, RespawnScheduled, OnDueTimeChanged. */
	void ArmRequest(FParticipantId Participant, ERespawnCause Cause, double DueServerTime);
	/** Starts a new request that is due now: Picking(0) and the commit in this call stack (readmission after T_due, PawnLost). */
	void StartPickingNow(FParticipantId Participant, double DueServerTime);
	/** ADR-0019 section 4.1: re-validate, pick, pre-check, TryReleaseNpc, TakeOverCrowdPawn, (Applied) card, bookkeeping. */
	void RunCommit(FParticipantId Participant);
	/** A retry of the same request (same RespawnRequestSeq) at DueServerTime; bRegular: the EmergenceRetrySec cadence. */
	void ScheduleRetry(FParticipantId Participant, double DueServerTime, bool bRegular);
	/**
	 * Stores the widening level LSC reported for the request (story nkp-013; ADR-0019 section 4.3): a None pick's level and
	 * the committed pick's level, never a Deferred or excluded one, and never lower than a level the request already reached.
	 */
	void RecordWideningLevel(FParticipantId Participant, uint8 WideningLevel);
	/** Cancels the pending item (if any) and forgets the request; t_lost and t_floor are kept. */
	void CancelRequest(FPenaltyRecord& Record);
	/** Ends the request without a body (re-validation failed, round over, stale bind): the row, T_due unset. */
	void VoidRequest(FParticipantId Participant, ERespawnCancelReason Reason, EInformantLifeState NewState);
	uint64 ScheduleItem(FParticipantId Participant, const FPenaltyRecord& Record, double DueServerTime);
	void RaiseDueTime(FParticipantId Participant, TOptional<double> DueServerTime);
	bool IsDisconnected(FParticipantId Participant) const;
	double NowServerTime() const;
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
		/** The released body (the tally query's attacker pawn after the release). */
		FPenaltyPawnKey Pawn;
	};

	FPenaltyCoreDeps Deps;
	FPenaltyEventLog EventLog;
	FPenaltyParams Latched;
	TMap<FParticipantId, FPenaltyRecord> Records;
	/** Round-scoped: released body -> release server time (PEN-R5). */
	TMap<FPenaltyPawnKey, double> ReleasedBodies;
	TOptional<FPendingStrikeRow> PendingStrikeRow;
	TOptional<FSlotExile> SlotExile;
	/** Round-scoped: a dying participant body's death write time, from its OnPawnKilled (the F2 floor of its respawn). */
	TMap<FParticipantId, double> DyingBodyDeathTimes;
	/** The next RespawnRequestSeq (round-scoped, never 0). */
	uint32 NextRespawnRequestSeq = 1;
	uint32 RoundEpoch = 0;
	bool bFrozen = false;
};
