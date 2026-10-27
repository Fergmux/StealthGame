#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthQueuePayloads.h"
#include "View/Random/StealthRandomTag.h"

class IEventFlushBarrier;
class IQueueResolver;
class IStagedWriteApplier;
class ITerminalDecision;
class IRoundLogWriter;
class IServerSubSeedSource;
class IStealthClock;
class IStealthFaultSink;
enum class ESeedDomain : uint32;
struct FRoundLogEntry;
enum class EStealthFault : uint8;

/**
 * FResolutionQueueCore: the round's single total-order resolution queue (ADR-0002 sections 1-5; Round GDD R11-R14,
 * F1). Plain C++, no UWorld, no UObject: every input arrives through FResolutionQueueDeps, so the whole contract
 * is testable headless with the fake clock, a fixed server secret and stub resolvers (StealthGame.Round.*). The
 * UResolutionQueue world-subsystem facade and its TG_PostPhysics drain tick function (a later Exit A story)
 * adapt the engine to it and call Drain() once per server frame.
 *
 * One Drain():
 *   1. reads ServerTickIndex and DrainStartTime from the injected clock (the production clock increments the
 *      tick once per frame at drain start);
 *   2. stamps due-heap items with DueTime <= DrainStartTime + DueEpsilonMs, ordered (DueTime, TieRank), equal
 *      HoldTimer DueTimes by their BeginKey;
 *   3. stamps intake: the non-empty mailboxes in canonical order (ascending ParticipantId, server-local last)
 *      permuted by Fisher-Yates from FSecretStream(GetServerSubSeed(SeqOrder, tick), "RND_SEQ_"), each FIFO, at
 *      most MailboxMaxItemsPerDrain per mailbox (the excess dropped, counted and logged MailboxOverflow);
 *   4. runs every stamped item in key order in its slot: epoch check (Rejected_StaleEpoch), connection-generation
 *      check (Dropped_StaleConnection), latch check (Rejected_Latched; HostCommand parked; the post-latch lane and
 *      the ConcealmentTransition exemption pass), then Resolve, then IEventFlushBarrier::FlushPending();
 *   5. stamps and runs items that became due during the drain (scheduled inline) after intake;
 *   6. runs the staging pass (ADR-0002 section 6): every staged write with ServerTime <= now (plus DueEpsilonMs), in
 *      order (ServerTime, CauseKey, TypeRank), through its applier, or ITerminalDecision for RoundEndStep.
 * A RoundEndStep staged inside a slot is bound at slot end (section 8.3): to the latest DeathWrite the same slot staged
 * (same CauseKey), else, for a strike-decided round, to the fallback (the deciding victim's latest pending DeathWrite,
 * else the slot time plus FallbackDeferSec, reported as a defect), else it is consumed inline at slot end.
 * Inline follow-ons run depth-first inside the causing slot through ExecuteFollowOn, bounded by
 * QueueCascadeDepthMax; a breach logs CascadeOverflow and submits the remaining work as items in raised order,
 * each carrying the slot's RootCauseKey.
 *
 * Nothing here logs through UE_LOG/UE_LOGF, ensure or trace text: Event Log entries go only to the injected
 * IRoundLogWriter; faults go to the injected fault sink. Mailbox order and sub-seeds are never exposed.
 */

/** The current round epoch the queue compares every item's Expected token with (Roster RoundEpoch after ResetAll). */
class IQueueEpochSource
{
public:
	virtual ~IQueueEpochSource() = default;

	virtual FEpochToken GetCurrentEpoch() const = 0;
};

/** Session's current ConnectionGeneration per participant (an item submitted under an older one is stale). */
class IQueueConnectionGenerationSource
{
public:
	virtual ~IQueueConnectionGenerationSource() = default;

	virtual uint32 GetConnectionGeneration(FParticipantId Participant) const = 0;
};

/** Injected dependencies. Clock, SubSeeds, Epoch, Log and FaultSink are required; Generations is optional. */
struct FResolutionQueueDeps
{
	const IStealthClock* Clock = nullptr;
	const IServerSubSeedSource* SubSeeds = nullptr;
	const IQueueEpochSource* Epoch = nullptr;
	const IQueueConnectionGenerationSource* Generations = nullptr;
	IRoundLogWriter* Log = nullptr;
	IStealthFaultSink* FaultSink = nullptr;
};

/** What happened to one stamped item (one record per slot, in key order). */
enum class EQueueSlotOutcome : uint8
{
	Executed,
	Voided,
	Rejected_Latched,
	Rejected_StaleEpoch,
	Dropped_StaleConnection,
	/** A HostCommand that arrived after the latch, parked unresolved for Round (ADR-0002 section 8.1). */
	Parked,
	/** No resolver registered for the item's type (reported to the fault sink, never silently dropped). */
	NoResolver,
	/** A DebugItem in a Shipping build: dropped with a fault (ADR-0002 section 2a). */
	DebugDroppedInShipping,
};

/** One slot's record, for observation (tests and the facade's diagnostics; never logged as text). */
struct FQueueSlotRecord
{
	FQueueKey Key;
	EQueueItemType Type = EQueueItemType::PawnCommand;
	FMailboxId Source;
	EQueueSlotOutcome Outcome = EQueueSlotOutcome::Executed;
	/** The originating slot's key (the item's own key unless a cascade overflow moved it out of its slot). */
	FQueueKey RootCauseKey;
	/** The slot that submitted this item from inside its own slot (a deferred outcome), else unset. */
	FQueueKey CauseKey;
};

/** Identifies a submitted or scheduled item; GetKey answers its key once it is stamped. */
struct FQueueItemHandle
{
	uint64 Id = 0;

	bool IsSet() const { return Id != 0; }
	bool operator==(const FQueueItemHandle& Other) const { return Id == Other.Id; }
};

/** Fault-sink Detail values for EStealthFault::QueueMisuse. */
enum class EStealthQueueMisuse : uint8
{
	FollowOnOutsideSlot = 1,
	ReentrantDrain = 2,
	ResetDuringDrain = 3,
	VoidCodeOutsideSlot = 4,
	NullRegistration = 5,
	/** Submit with an unset mailbox. */
	InvalidMailbox = 6,
	/** The sub-seed source holds no secret (the intake permutation would be public). */
	NoServerSecret = 7,
	/** Items scheduled inline kept becoming due past MaxInlineDuePasses in one drain. */
	InlineDueLoop = 8,
	/** StageWrite outside a slot, or the staging pass run inside one (ADR-0002 section 6). */
	StageWriteOutsideSlot = 9,
	/** An applier registration for RoundEndStep (it is routed to ITerminalDecision, never to an applier). */
	RoundEndApplierRegistration = 10,
	/** A strike-decided RoundEndStep found no DeathWrite of its slot: the section 8.3 fallback binding ran (a defect). */
	RoundEndFallbackBinding = 11,
	/** A staged write whose type has no live applier (dropped; the section 6 StagedWriteNoApplier case). */
	StagedWriteNoApplier = 12,
};

class STEALTHFOUNDATION_API FResolutionQueueCore
{
public:
	/** R12 / ruling D22: the one due tolerance, 1 ms, a design constant (not a tuning key). Every feature cites it. */
	static constexpr double DueEpsilonSec = 0.001;

	/** ADR-0002 section 3: discrete items stamped per mailbox per drain; the rest are dropped and counted. */
	static constexpr int32 MailboxMaxItemsPerDrain = 32;

	/** ADR-0002 section 5 / GDD constants: the inline follow-on depth bound (a safety bound, not a balance value). */
	static constexpr int32 QueueCascadeDepthMax = 8;

	/** Bound on the passes that stamp items scheduled inline and already due, per drain (a defect guard). */
	static constexpr int32 MaxInlineDuePasses = 16;

	/** GetKey answers for this many most recent handles (a fixed ring: no allocation per stamped item). */
	static constexpr int32 RecentKeyCount = 4096;

	/** ADR-0006 section 5 stream tags for the per-tick intake permutation and tie rank. */
	static constexpr uint64 SeqOrderStreamTag = StealthRandom::Tag("RND_SEQ_");
	static constexpr uint64 TieRankStreamTag = StealthRandom::Tag("RND_TIE_");

	/** Every required pointer in Deps must be non-null and outlive the core. */
	explicit FResolutionQueueCore(const FResolutionQueueDeps& InDeps);

	FResolutionQueueCore(const FResolutionQueueCore&) = delete;
	FResolutionQueueCore& operator=(const FResolutionQueueCore&) = delete;

	// --- Registration (ADR-0002 section 5: one resolver per type, from the composition root) ---------------

	/**
	 * Registers the resolver for an item type (TimerFired is registered per kind with RegisterTimerResolver).
	 * A second registration while one is held is rejected (false, QueueDuplicateRegistration fault): "one
	 * resolver per type" (RMF Exit A ruling, decision log). Unregister first to replace.
	 */
	bool RegisterResolver(EQueueItemType Type, IQueueResolver* Resolver);

	/** Registers the resolver for one TimerFired subkind; same rules as RegisterResolver. */
	bool RegisterTimerResolver(ETimerKind Kind, IQueueResolver* Resolver);

	void UnregisterResolver(EQueueItemType Type);
	void UnregisterTimerResolver(ETimerKind Kind);

	/** Drops every registration and the flush barrier (the facade calls it from Deinitialize). */
	void ClearRegistrations();

	/** The Roster's flush barrier, called after every Resolve (nullptr: no barrier). */
	void SetEventFlushBarrier(IEventFlushBarrier* Barrier) { FlushBarrier = Barrier; }

	/**
	 * Registers the applier of a staged-write type (DeathWrite, CounterPose; Assassination). One per type; RoundEndStep
	 * is not registrable (false and a QueueMisuse fault): it is routed to the terminal decision.
	 */
	bool RegisterStagedWriteApplier(EStagedWriteType Type, IStagedWriteApplier* Applier);
	void UnregisterStagedWriteApplier(EStagedWriteType Type);

	/** Round's round-end step (ADR-0002 section 8.4); the queue routes every RoundEndStep write here. */
	void SetTerminalDecision(ITerminalDecision* InDecision) { TerminalDecision = InDecision; }
	ITerminalDecision* GetTerminalDecision() const { return TerminalDecision; }

	// --- Intake ---------------------------------------------------------------------------------------------

	/** Pushes an item into Source's FIFO mailbox; it is stamped at the next drain start (never on arrival). */
	FQueueItemHandle Submit(FMailboxId Source, const FQueueItem& Item);

	/**
	 * As Submit, recording the sender's Session ConnectionGeneration captured at push: at its slot an item whose
	 * generation is older than the participant's current one is Dropped(StaleConnection) and logged.
	 */
	FQueueItemHandle Submit(FMailboxId Source, const FQueueItem& Item, uint32 ConnectionGeneration);

	/** Schedules a timed item into the due-time heap (every gameplay timer; ADR-0002 section 4). */
	FQueueItemHandle ScheduleAt(double DueServerTime, const FQueueItem& Item);

	// --- Drain ------------------------------------------------------------------------------------------------

	/** One drain (see the class comment). Called once per server frame by the facade's tick function. */
	void Drain();

	// --- Inline follow-ons (ADR-0002 section 5) ----------------------------------------------------------------

	/**
	 * Runs Work inline, inside the current slot, through the resolver registered for its type: it inherits the
	 * slot's key and consumes no Seq (R14.2). Past QueueCascadeDepthMax nested follow-ons the work is instead
	 * submitted as a new item (current tick, next Seq, processed later in this drain) carrying the slot's
	 * RootCauseKey, and the slot logs CascadeOverflow once. Returns true when Work ran inline.
	 */
	bool ExecuteFollowOn(const FQueueItem& Work);

	// --- Ordering primitives (the only ones any system may use; ADR-0002 section 1) -------------------------------

	/** Lexicographic compare of two keys: -1, 0 or 1. */
	static int32 Compare(FQueueKey A, FQueueKey B) { return FQueueKey::Compare(A, B); }

	/**
	 * The key a submitted or scheduled item was stamped with; unset until stamped, after ResetForNewRound, and
	 * once RecentKeyCount newer handles have been stamped.
	 */
	TOptional<FQueueKey> GetKey(FQueueItemHandle Handle) const;

	/** The key of the slot being resolved; an unset key outside a slot. */
	FQueueKey CurrentKey() const { return bInSlot ? Slot.Key : FQueueKey(); }

	/** The current slot's RootCauseKey (its own key, or the originating slot's after a cascade overflow). */
	FQueueKey CurrentRootCauseKey() const { return bInSlot ? Slot.RootCauseKey : FQueueKey(); }

	/** The slot that submitted the current item from inside its own slot (a deferred outcome, R14.3), else unset. */
	FQueueKey CurrentCauseKey() const { return bInSlot ? Slot.CauseKey : FQueueKey(); }

	/** True while a resolver runs inside a slot (including its inline follow-ons and the flush). */
	bool IsInSlot() const { return bInSlot; }

	/** Nested follow-on depth of the running code: 0 for the item's own resolver. */
	int32 GetFollowOnDepth() const { return FollowOnDepth; }

	/** Inside a slot: the owner-specific code the Voided entry carries if the resolver returns Voided. */
	void SetVoidCode(uint32 OwnerCode);

	// --- Typed staged writes (ADR-0002 sections 6 and 8.3; story rmf-053) -----------------------------------------

	/**
	 * Stages one already-decided write from inside a slot (false and a QueueMisuse fault outside one). The queue sets
	 * CauseKey to the slot's RootCauseKey and Expected to the current epoch (a Reset drops it by epoch mismatch).
	 * Example: a strike resolver stages {DeathWrite, ServerTime = ContactServerTime - StrikeNetLeadSec, Victim}.
	 */
	bool StageWrite(const FStagedWrite& Write);

	/**
	 * The staging pass (Drain runs it last, every frame): applies every staged write due now, in (ServerTime, CauseKey,
	 * TypeRank) order with TypeRank DeathWrite < CounterPose < RoundEndStep, each followed by the flush barrier. Staged
	 * writes run even after the latch. Refused (fault) inside a slot.
	 */
	void RunStagingPass();

	/** Drops every pending staged write (Round's Reset decided inside a slot: the writes belong to the torn-down round). */
	void DropStagedWrites() { StagedWrites.Reset(); Slot.RoundEndIndex = INDEX_NONE; }

	/** Staged writes not yet applied (observation). */
	const TArray<FStagedWrite>& GetPendingStagedWrites() const { return StagedWrites; }

	// --- Latch storage (ADR-0002 section 8.1; Round's record is authoritative) -------------------------------------

	/** Single assignment, called solely by Round's TryTerminate; closes intake. False if already latched. */
	bool Latch(FQueueKey TerminalKey);
	bool IsLatched() const { return bLatched; }
	FQueueKey GetTerminalKey() const { return TerminalKey; }

	/** HostCommand items parked after the latch, in key order, handed to Round at the round-end step. */
	TArray<FQueueItem> TakeParkedHostCommands();

	// --- Round lifecycle ------------------------------------------------------------------------------------------------

	/**
	 * Opens the queue for a new round: Seq restarts at 1 (ServerTickIndex continues: it is the clock's), and
	 * mailboxes, the due-time heap, the latch, parked commands and stamped-key records are cleared. Refused (fault)
	 * while a drain runs: the facade runs a Reset decided inside a slot after Drain() returns.
	 */
	void ResetForNewRound();

	// --- Observation (counts and records only; never mailbox order or sub-seeds) ---------------------------------------

	/** The Seq the next stamped item will receive. */
	uint64 PeekNextSeq() const { return NextSeq; }

	/** One record per slot of the last drain, in key order. */
	const TArray<FQueueSlotRecord>& GetLastDrainRecords() const { return DrainRecords; }

	/** Items dropped by the per-mailbox cap in the last drain, and in total since ResetForNewRound. */
	int32 GetLastDrainOverflowCount() const { return LastDrainOverflowCount; }
	uint64 GetTotalOverflowCount() const { return TotalOverflowCount; }

	/** Items dropped as stale-connection in total since ResetForNewRound. */
	uint64 GetTotalStaleConnectionDropCount() const { return TotalStaleConnectionDrops; }

	/** Items waiting in mailboxes, and timers waiting in the heap. */
	int32 GetPendingMailboxItemCount() const;
	int32 GetPendingTimerCount() const { return DueHeap.Num(); }

	/** The ETimerKind of every TimerFired item waiting in the due-time heap (observation; story rmf-051). */
	void GetPendingTimerKinds(TArray<ETimerKind>& OutKinds) const;

	/** Cancels every pending timer (Round R10 step 2); legal during a drain (items already stamped are unaffected). */
	void CancelAllTimers() { DueHeap.Reset(); }

	/**
	 * Cancels one pending timer by the handle ScheduleAt returned (ADR-0019 section 3.3 Cancel(TimerHandle); story nkp-009).
	 * False when it is no longer in the heap (already stamped, already cancelled, or never a timer). Legal during a drain:
	 * an item already stamped in this drain is unaffected (its resolver re-validates). O(n) in the heap size plus a
	 * re-heapify; called only on a cancellation, never per frame.
	 */
	bool CancelTimer(FQueueItemHandle Handle);

private:
	struct FPendingItem
	{
		FQueueItem Item;
		uint64 HandleId = 0;
		uint32 ConnectionGeneration = 0;
		bool bHasGeneration = false;
		FQueueKey CauseKey;
	};

	struct FMailbox
	{
		FMailboxId Id;
		TArray<FPendingItem> Items;
	};

	struct FDueEntry
	{
		double DueTime = 0.0;
		uint64 ScheduleOrder = 0;
		FPendingItem Pending;
	};

	struct FStampedItem
	{
		FQueueKey Key;
		FPendingItem Pending;
		FQueueKey RootCauseKey;
	};

	struct FRecentKey
	{
		uint64 HandleId = 0;
		FQueueKey Key;
	};

	struct FSlotContext
	{
		FQueueKey Key;
		FQueueKey RootCauseKey;
		FQueueKey CauseKey;
		FMailboxId Source;
		uint32 VoidCode = 0;
		int32 OverflowCount = 0;
		bool bOverflowLogged = false;
		/** Index in StagedWrites of the RoundEndStep this slot staged, else INDEX_NONE (bound at slot end). */
		int32 RoundEndIndex = INDEX_NONE;
	};

	FQueueItemHandle SubmitInternal(FMailboxId Source, const FQueueItem& Item, bool bHasGeneration, uint32 ConnectionGeneration);
	bool StampDueItems();
	uint64 SubSeedOrFault(ESeedDomain Domain) const;
	void StampIntake();
	FQueueKey StampNext(const FPendingItem& Pending, const FQueueKey* RootCause);
	void RunSlot(const FStampedItem& Stamped);
	void EnterSlot(const FStampedItem& Stamped);
	void EndSlot();
	IQueueResolver* FindResolver(const FQueueItem& Item) const;
	static bool IsPostLatchAllowed(const FQueueItem& Item);
	static uint32 GetTieParticipantValue(const FQueueItem& Item);
	static int64 RegistrationDetail(const FQueueItem& Item);
	void AppendEntry(FRoundLogEntry& Entry, const FQueueKey& Key, FMailboxId Source) const;
	void LogVoided(const FStampedItem& Stamped, EVoidReason Reason, uint32 OwnerCode) const;
	void ReportFault(EStealthFault Fault, int64 Detail) const;
	/** Section 8.3: binds or consumes the RoundEndStep the slot staged (called at slot end, after the flush). */
	void BindRoundEndStep(int32 Index);
	/** Applies one staged write (applier or terminal decision), then the flush barrier. */
	void ApplyStaged(const FStagedWrite& Write);

	FResolutionQueueDeps Deps;
	IEventFlushBarrier* FlushBarrier = nullptr;
	IQueueResolver* TypeResolvers[GQueueItemTypeCount] = {};
	IQueueResolver* TimerResolvers[GTimerKindCount] = {};

	/** Sorted by Id.Value (canonical base order: ascending ParticipantId, server-local last). */
	TArray<FMailbox> Mailboxes;
	/** Min-heap on (DueTime, ScheduleOrder). */
	TArray<FDueEntry> DueHeap;
	TArray<FStampedItem> Stamped;
	TArray<FQueueSlotRecord> DrainRecords;
	/** RecentKeyCount entries, allocated once at construction. */
	TArray<FRecentKey> RecentKeys;
	TArray<FQueueItem> ParkedHostCommands;
	TArray<FStagedWrite> StagedWrites;
	IStagedWriteApplier* Appliers[3] = {};
	ITerminalDecision* TerminalDecision = nullptr;

	uint64 NextSeq = 1;
	uint64 NextHandleId = 1;
	uint64 NextScheduleOrder = 1;
	uint64 DrainTick = 0;
	double DrainStartTime = 0.0;

	FSlotContext Slot;
	int32 FollowOnDepth = 0;
	bool bInSlot = false;
	bool bDraining = false;

	bool bLatched = false;
	FQueueKey TerminalKey;

	int32 LastDrainOverflowCount = 0;
	uint64 TotalOverflowCount = 0;
	uint64 TotalStaleConnectionDrops = 0;
};
