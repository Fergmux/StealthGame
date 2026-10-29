#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/CommandPredicate.h"
#include "Server/Core/PawnCommandCore.h"
#include "Server/StealthQueuePayloads.h"
#include "Server/StealthTuningSubstructs.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "View/CommandReplyTypes.h"
#include "View/ConcealmentTypes.h"
#include "View/StealthEncounterWire.h"
#include "View/StealthTuningPublicSubstructs.h"

class APawn;
class IConcealmentCullSource;
class IRoundLogWriter;
class IStealthClock;
class IStealthFaultSink;
struct FCommandReply;

/**
 * FConcealmentCore: the plain-C++ server core of Concealment (ADR-0017 sections 1-3, 7, 10; GDD CON-R2..R12, States and
 * Transitions; stories con-003..con-009, con-014..con-018, con-024). Headless-testable: every engine fact arrives through
 * the seams below, time through IStealthClock, timers through the round queue's ScheduleAt.
 *
 * It owns, per pawn, the server record {State, BushId, TransitionGeneration, TransitionStartServerTime, CommitSeq,
 * LastExitCompleteServerTime} and, per bush, the occupancy record: the pawns in Concealed, or in Exiting/Emerging before
 * their leaf crossing, each with a strictly increasing CommitSeq. There is no capacity limit. The legal moves are
 *   Outside->Entering (HideEnter execution)      Entering->Concealed (Commit)      Entering->Outside (killed before commit)
 *   Concealed->Exiting (HideExit, guard exit)     Concealed->Emerging (Attack)       Concealed->removed (killed)
 *   Exiting->Outside (End)                        Emerging->Outside (End)
 * (a guard failure is Entering->Exiting: the ordinary exit on the entry side, AC-CON-29). Every other move is refused and
 * reported (ConcealTransition). Every accepted move bumps TransitionGeneration. Transition timers are
 * TimerFired(ConcealmentTransition) items {Subject = pawn, Generation, Phase = EConcealPhase}; an item whose generation is
 * stale or whose pawn is dead resolves Voided with no effect.
 *
 * Nothing here reads controller kind, team or occupancy for legality, timing or replies (CON-R3, CON-R14); the relation
 * seam is read only by the spot query and IsConcealedFrom. No timer of any kind other than the queue items exists, and no
 * dwell, capacity, reveal or timeout rule (P4).
 *
 * Usage (server, from the Hide resolver):
 *   FConcealmentCore Core(Deps, Params);
 *   Core.SetBushes(Runtime->GetBushes());
 *   const EResolveResult R = Core.ResolveCommand(Item, Key);   // PawnCommand HideEnter / HideExit
 *   Core.ResolveTimer(Item, Key);                             // TimerFired(ConcealmentTransition)
 */

/** The values the core reads once (the schema defaults until the TDC store feeds features the frozen set). */
struct FConcealmentParams
{
	FConcealmentTuning_Public Public;
	FConcealmentTuning_Server Server;
	/** Assassination AttackAttemptLockoutSec: an emerging striker's recovery ends this long after execution. */
	double AttackLockoutSec = 1.75;
	/** Assassination StrikeContactSec (Cross of an emerge = ContactServerTime - StrikeContactSec). */
	double StrikeContactSec = 0.4;
};

/** Read and write access to the engine pawns (server). Implemented by the service adapter; faked in tests. */
class IConcealmentPawnWorld
{
public:
	virtual ~IConcealmentPawnWorld() = default;

	/** Alive and not pending teardown (no death write staged or applied, not destroyed). */
	virtual bool IsLive(const APawn& Pawn) const = 0;
	/** Server capsule centre. */
	virtual FVector GetLocation(const APawn& Pawn) const = 0;
	virtual double GetCapsuleRadiusCm(const APawn& Pawn) const = 0;
	virtual double GetCapsuleHalfHeightCm(const APawn& Pawn) const = 0;
	/** The pawn's look yaw (control rotation), degrees. */
	virtual double GetLookYawDeg(const APawn& Pawn) const = 0;
	/** IIF legality class (unbound pawns are Active). */
	virtual ECommandLegalityClass GetLegalityClass(const APawn& Pawn) const = 0;
	virtual bool IsRunning(const APawn& Pawn) const = 0;
	virtual bool IsTraversing(const APawn& Pawn) const = 0;
	/** The one FActionState write (UPawnCommandComponent::WriteActionState): {Pose, StartServerTime}. */
	virtual void WritePose(APawn& Pawn, EActionPose Pose, double StartServerTime) = 0;
	/** The pawn's current FActionState. */
	virtual FActionState GetActionState(const APawn& Pawn) const = 0;
	/** Starts the server root-motion move to Target over DurationSec (FRootMotionSource_MoveToForce; story con-011). */
	virtual void StartWarp(APawn& Pawn, const FVector& Target, double DurationSec, bool bEnter) = 0;
	/** The server mirror of the record (the pawn's UConcealmentComponent; the host presentation refresh). */
	virtual void OnStateChanged(APawn& Pawn, EConcealState State, uint16 BushId, double TransitionStartServerTime) = 0;
	/** Live pawns whose capsule centre is within RadiusCm (horizontal) of Point. */
	virtual void GetPawnsNear(const FVector& Point, double RadiusCm, TArray<APawn*>& OutPawns) const = 0;
	/** Caps the pawn's depenetration from other pawns when its collision returns (CON-R8, never a launch). */
	virtual void SetExitDepenetrationCap(APawn& Pawn, double CapCm) = 0;
};

/** Roster relations (server). The only reads of identity, for the spot query and IsConcealedFrom; never legality. */
class IConcealmentRelations
{
public:
	virtual ~IConcealmentRelations() = default;

	/** The participant bound to Pawn (invalid for an unbound pawn: an NPC). */
	virtual FParticipantId GetParticipant(const APawn& Pawn) const = 0;
	/** True when Viewer is Pawn's own participant or on the same team (allies see a concealed teammate). NPCs: false. */
	virtual bool IsAllyViewer(FParticipantId Viewer, const APawn& Pawn) const = 0;
	/** The pawn's kind for the log only (true: a bound player pawn). */
	virtual bool IsPlayerPawn(const APawn& Pawn) const = 0;
};

/** The owner and team concealed flags' only writers (ADR-0017 section 1; story con-023). */
class IConcealmentFlagSink
{
public:
	virtual ~IConcealmentFlagSink() = default;

	/** The owner's UConcealmentOwnerViewComponent::bIsConcealed (owner audience). */
	virtual void SetOwnerConcealed(FParticipantId Participant, bool bConcealed) = 0;
	/** Roster SetTeammateConcealed (the C-TeamView teammate entry, team audience). */
	virtual void SetTeammateConcealed(FParticipantId Participant, bool bConcealed) = 0;
};

/** The round queue as Concealment uses it (inside slots), and the IIF reply step and hub events. */
class IConcealmentQueue
{
public:
	virtual ~IConcealmentQueue() = default;

	/** Schedules a TimerFired(ConcealmentTransition) item (ADR-0002 section 4). */
	virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) = 0;
	/** The owner-specific code a Voided slot logs. */
	virtual void SetVoidCode(uint32 OwnerCode) = 0;
	/** The IIF reply of the slot (the null sink discards). */
	virtual void AppendReply(FReplySinkId Sink, FQueueKey Key, const FCommandReply& Reply) = 0;
	/** The hub's OnActionStarted (fired inside the slot, every pawn kind alike). */
	virtual void OnActionStarted(APawn& Pawn, EActionType Type) = 0;
};

/** Concealment's Event Log events (ADR-0017 section 10; EConcealmentLogEvent is FRoundLogEntry::Code). */
enum class EConcealmentLogEvent : uint8
{
	Commit,
	ExitCross,
	Emerge,
	Killed,
	BushStrike,
};

/** The core's seams. Clock, Pawns and Queue are required. */
struct FConcealmentCoreDeps
{
	const IStealthClock* Clock = nullptr;
	IConcealmentPawnWorld* Pawns = nullptr;
	IConcealmentQueue* Queue = nullptr;
	const IConcealmentRelations* Relations = nullptr;
	IConcealmentFlagSink* Flags = nullptr;
	IConcealmentCullSource* Cull = nullptr;
	IRoundLogWriter* Log = nullptr;
	IStealthFaultSink* Faults = nullptr;
};

/** One pawn's server record (ADR-0017 section 1). */
struct FConcealRecord
{
	EConcealState State = EConcealState::Outside;
	uint16 BushId = 0;
	uint32 TransitionGeneration = 0;
	double TransitionStartServerTime = 0.0;
	/** The commit sequence of the occupancy entry (0 = never committed this transition). */
	uint32 CommitSeq = 0;
	/** Completion of the last exit or emergence (the re-entry cooldown's start); very negative = none. */
	double LastExitCompleteServerTime = -1.0e9;
	/** In its bush's occupancy record (Concealed, or Exiting/Emerging before the crossing). */
	bool bInOccupancy = false;
	/** The leaf crossing of the current exit or emergence has passed. */
	bool bCrossed = false;
	/** The transition's End has not fired yet (the enter's slot is held until BushEnterDurSec: an Exit is Busy). */
	bool bTransitionOpen = false;
	/** The cull was pushed (committed and not yet un-culled). */
	bool bCulled = false;
	/** Killed while in Concealed (or before its crossing): the record left, flags held until removal or rebind. */
	bool bKilled = false;
	/** The death write reached the pawn (the un-cull with bDiedConcealed happened). */
	bool bDeathWritten = false;
	/** The participant whose owner/team flags this pawn set (cleared at the crossing, removal or rebind). */
	FParticipantId FlagParticipant;
	/** The side the transition uses (entry side for Enter, exit side for Exit and Emerge). */
	uint8 Side = 0;
};

class STEALTHGAMEPLAY_API FConcealmentCore
{
public:
	FConcealmentCore(const FConcealmentCoreDeps& InDeps, const FConcealmentParams& InParams);

	/** The layout's bushes (copied; every later query reads them). */
	void SetBushes(TConstArrayView<FBushRuntime> InBushes);
	const FBushRuntime* FindBush(uint16 BushId) const;
	const TArray<FBushRuntime>& GetBushes() const { return Bushes; }

	// ---- Resolvers (inside queue slots) --------------------------------------------------------------------------

	/** PawnCommand HideEnter / HideExit (any other kind: Voided). */
	EResolveResult ResolveCommand(const FQueueItem& Item, FQueueKey Key);
	/** TimerFired(ConcealmentTransition): Commit, Cross or End of the pawn's current transition. */
	EResolveResult ResolveTimer(const FQueueItem& Item, FQueueKey Key);

	// ---- Legality (the IIF CanBegin predicate; const, side-effect-free) ------------------------------------------

	/** The verdict for Pawn beginning Cmd (HideEnter / HideExit) now. Never reads occupancy, controller kind or team. */
	FPredicateResult CanBeginHide(const APawn& Pawn, ECommandKind Kind, uint32 Arg) const;

	// ---- Events from other systems (inside the slot that decides them) --------------------------------------------

	/**
	 * Assassination's Attack execution (IIF OnActionStarted(Attack); story con-014): a Concealed attacker breaks
	 * concealment in the same slot: un-culled, Emerging, Cross at ContactServerTime - StrikeContactSec, End when the
	 * strike's recovery ends. Identical for a kill, a reversal and a whiff. Not concealed: nothing.
	 */
	void OnAttackExecuted(APawn& Attacker, double ContactServerTime, FQueueKey Key, const FQueueItem& Item);

	/**
	 * A kill in Assassination's outcome slot (story con-017/018). Entering: Outside with no record (the Commit drops by
	 * generation; an ordinary visible kill). Concealed, or Exiting/Emerging before the crossing: the record is left in the
	 * slot, no cull change, flags held. Idempotent.
	 */
	void OnPawnKilled(APawn& Victim, FQueueKey Key);

	/** The victim's death write (staging pass): the un-cull for a killed-concealed victim. True when it was concealed. */
	bool OnDeathWrite(APawn& Victim);

	/** True when the victim's FDeathState must carry bDiedConcealed (Concealed or CullPending at the death write). */
	bool ShouldMarkDiedConcealed(const APawn& Victim) const;

	/** The server body removal of Body (ADR-0018 IBodyRemovalObserver): clears a killed-concealed pawn's flags. */
	void OnBodyRemoved(APawn& Body);

	/** Participant was bound to a new pawn (BindPawn / TakeOverCrowdPawn): flags carried for its old body clear. */
	void OnParticipantRebound(FParticipantId Participant, const APawn* NewPawn);

	/** OnRosterReset: every record and occupancy cleared, every flag cleared (items drop by epoch). */
	void ResetAll();

	// ---- Server queries ---------------------------------------------------------------------------------------------

	EConcealState GetState(const APawn* Pawn) const;
	const FConcealRecord* FindRecord(const APawn* Pawn) const;
	/** True for every state except Outside (Crowd eligibility, Penalty/takeover, Pings; D33: Emerging counts). */
	bool IsInBushOrTransition(const APawn* Pawn) const;
	/** ICrowdPerceptionFilter: false only while Concealed. */
	bool IsPerceivable(const APawn* Pawn) const;
	/** ICrowdPerceptionFilter: true only while Outside. */
	bool IsSteeringObstacle(const APawn* Pawn) const;
	/** Concealed now and not killed (a candidate only through the bush spot rule). */
	bool IsConcealed(const APawn* Pawn) const;
	/** F4: the bush whose spot zone holds WorldPoint (zones never overlap at BushSpacingCm). */
	TOptional<uint16> TargetsBush(const FVector& WorldPoint) const;
	/** The bush's occupancy record minus Attacker and its allies, by CommitSeq (Exiting/Emerging before crossing included). */
	void GetSpotOccupants(uint16 BushId, const APawn* Attacker, TArray<APawn*>& OutByCommitSeq) const;
	/** Every live occupant of BushId by CommitSeq (tests, dumps). */
	void GetOccupancy(uint16 BushId, TArray<APawn*>& OutByCommitSeq) const;
	/**
	 * IVP R7.8 (story con-024): Pawn is in Concealed, or Exiting/Emerging before the crossing (or killed concealed until the
	 * death write), and Viewer is not an ally (own participant or teammate). Every viewer of an NPC is a non-ally. Server-only.
	 */
	bool IsConcealedFrom(FParticipantId Viewer, const APawn* Pawn) const;

	/** The parameters read once at construction. */
	const FConcealmentParams& GetParams() const { return Params; }
#if WITH_STEALTH_TEST_SEAMS
	/** Test seam (the S-CON-1 spike lifts the re-entry cooldown for back-to-back cycles). */
	void SetParamsForTest(const FConcealmentParams& InParams) { Params = InParams; }
#endif

	/** Records every transition item the core scheduled (observation; tests). */
	int32 GetScheduledItemCount() const { return ScheduledItems; }

private:
	FConcealRecord& RecordOf(APawn& Pawn);
	double Now() const;
	bool Transition(APawn& Pawn, FConcealRecord& Record, EConcealState To);
	void Schedule(const FQueueItem& Cause, APawn& Pawn, const FConcealRecord& Record, EConcealPhase Phase, double Due);
	EResolveResult Void(const FCommandPayload& Cmd, FQueueKey Key, ECommandRejectCode Code);
	EResolveResult ExecuteEnter(const FQueueItem& Item, APawn& Pawn, uint16 BushId, FQueueKey Key);
	EResolveResult ExecuteExitCommand(const FQueueItem& Item, APawn& Pawn, uint8 Side, FQueueKey Key);
	/** Exit execution (command, or the guard-failure internal exit): Exiting, un-cull in the slot, Cross and End items. */
	void StartExit(const FQueueItem& Cause, APawn& Pawn, FConcealRecord& Record, uint8 Side, FQueueKey Key);
	void Commit(const FQueueItem& Item, APawn& Pawn, FConcealRecord& Record, FQueueKey Key);
	void Cross(APawn& Pawn, FConcealRecord& Record, FQueueKey Key);
	void End(APawn& Pawn, FConcealRecord& Record);
	/** The open side for an exit request: validated, then the first unblocked one in angular order (CON-R8). */
	uint8 ChooseExitSide(const APawn& Pawn, const FBushRuntime& Bush, uint8 Requested) const;
	bool IsBlockingPawn(const APawn& Pawn) const;
	void InsertOccupant(APawn& Pawn, FConcealRecord& Record);
	void RemoveOccupant(APawn& Pawn, FConcealRecord& Record);
	void SetFlags(APawn& Pawn, FConcealRecord& Record, bool bConcealed);
	void LogRow(FQueueKey Key, const APawn& Pawn, uint16 BushId, EConcealmentLogEvent Event, uint32 D2 = 0, uint32 D3 = 0, uint32 D4 = 0);
	void Report(int64 FaultValue, int64 Detail) const;

	FConcealmentCoreDeps Deps;
	FConcealmentParams Params;
	TArray<FBushRuntime> Bushes;
	TMap<TWeakObjectPtr<APawn>, FConcealRecord> Records;
	/** Occupancy per bush, in CommitSeq order (append at commit; CommitSeq is strictly increasing). */
	TMap<uint16, TArray<TWeakObjectPtr<APawn>>> Occupancy;
	uint32 NextCommitSeq = 1;
	int32 ScheduledItems = 0;
};
