#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthQueuePayloads.h"
#include "View/NetPawnCommand.h"

class IQueueEpochSource;
class IStealthClock;
class IStealthFaultSink;
class APawn;

/**
 * The plain-C++ logic of the pawn command interface (ADR-0011 section 1; story iif-002) and the cross-epic seam
 * interfaces it consumes. No UWorld, AActor, UActorComponent, timer manager or engine-time dependency: time comes
 * from an injected IStealthClock, work goes to an injected IPawnCommandQueue, so ADR-0008 headless specs cover it
 * (StealthGame.Input.PawnCommandCoreHeadless includes an include-scan lint of this file and its .cpp).
 *
 * Story iif-002 builds the skeleton only: construction seams, the slot-state enum and the submit path
 * (InteractBegin then InteractEnd submit exactly two items, in order). Slot arbitration, legality, buckets, debounce
 * and hold timers arrive with stories iif-020 and iif-027 to iif-050. The named stub doubles of these seams are in
 * the test harness (story iif-095).
 *
 * Usage:
 *   FPawnCommandCoreDeps Deps; Deps.Clock = &Clock; Deps.Queue = &Queue; Deps.FaultSink = &Faults;
 *   FPawnCommandCore Core(Deps);
 *   Core.Submit(FCommandOrigin::ServerLocal(), ECommandKind::InteractBegin, Payload);
 */

/**
 * The queue seam the core submits to (mirrors ADR-0002 UResolutionQueue::Submit and ScheduleAt). Production: the
 * Round queue facade; tests: FStubResolutionQueue (story iif-095). A plain C++ interface held by pointer.
 */
class IPawnCommandQueue
{
public:
	virtual ~IPawnCommandQueue() = default;

	/** Pushes Item into Mailbox's FIFO; it is stamped at the next drain (never on arrival). */
	virtual void Submit(FMailboxId Mailbox, const FQueueItem& Item) = 0;

	/** Schedules a time-driven item (hold timers) at DueServerTime. */
	virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) = 0;
};

/**
 * The legality class the command check reads (ADR-0011 section 3.2 step 1). Unbound maps to Active inside the Roster
 * adapter, so the command interface never learns whether a pawn is bound (ADR-0001).
 */
enum class ECommandLegalityClass : uint8
{
	Active,
	Informant,
	RoundEnded,
};

/** The Roster's legality query (consumer-owned seam; implemented by a Roster adapter, stubbed by FStubRosterLegality). */
class IRosterLegalityQuery
{
public:
	virtual ~IRosterLegalityQuery() = default;

	virtual ECommandLegalityClass GetLegalityClass(const APawn& Pawn) const = 0;
};

/**
 * Read-only pawn state the resolver uses (ADR-0010 section 8, ADR-0011 Key Interfaces). Implemented by a server-only
 * adapter over the movement component; stubbed by FStubSharedPawnStateView.
 */
class ISharedPawnStateView
{
public:
	virtual ~ISharedPawnStateView() = default;

	virtual bool IsRunning() const = 0;
	virtual bool IsInTraversal() const = 0;
	/** Commanded (input-driven) displacement since SinceServerTime, in cm. */
	virtual float CommandedDisplacementCm(double SinceServerTime) const = 0;
};

/**
 * Where a command came from, as far as the command interface may know (ADR-0011 Key Interfaces): an intake mailbox
 * and an opaque reply sink. Null sink for AI and server-local sources. Never a controller, PlayerState or team.
 */
struct FCommandOrigin
{
	FMailboxId Mailbox;
	FReplySinkId Sink;

	/** The server-local origin (AI, server sources): server-local mailbox, null sink. */
	static FCommandOrigin ServerLocal()
	{
		FCommandOrigin Origin;
		Origin.Mailbox = FMailboxId::ServerLocal();
		return Origin;
	}
};

/** The pawn's single action slot (ADR-0011 section 4); transitions are filled in by later stories. */
enum class EPawnSlotState : uint8
{
	Idle,
	Interacting,
	Bracing,
	Striking,
	Countering,
	HideTransition,
};

/** Fault-sink Detail values for EStealthFault::CommandMisuse. */
enum class EPawnCommandMisuse : uint8
{
	/** The core was built without a clock or a queue (a construction error; every submit is refused). */
	MissingSeam = 1,
	/** The injected clock went backwards; the core clamps to the last time it read. */
	ClockRegressed = 2,
	/** A command kind outside ECommandKind reached the core. */
	KindOutOfRange = 3,
};

/** The core's injected dependencies. Clock and Queue are required; the rest are optional until their stories land. */
struct FPawnCommandCoreDeps
{
	const IStealthClock* Clock = nullptr;
	IPawnCommandQueue* Queue = nullptr;
	/** The round epoch stamped on submitted items (null: a default token). */
	const IQueueEpochSource* Epoch = nullptr;
	const IRosterLegalityQuery* Legality = nullptr;
	const ISharedPawnStateView* PawnState = nullptr;
	IStealthFaultSink* FaultSink = nullptr;
};

/**
 * One pawn's command logic (ADR-0011 section 1). Per-pawn state is "tens of bytes": the slot, the last time read and
 * counters. Holds no caller identity and never asks whether a player or an AI drives the pawn.
 */
class STEALTHGAMEPLAY_API FPawnCommandCore
{
public:
	/** A core with no seams: invalid until Initialize (the component is constructed before the composition root wires it). */
	FPawnCommandCore() = default;

	/** Builds the core. A null Clock or Queue is a construction error: reported as CommandMisuse(MissingSeam), core invalid. */
	explicit FPawnCommandCore(const FPawnCommandCoreDeps& InDeps) { Initialize(InDeps); }

	/** (Re)binds the seams; same rules as the constructor. Returns IsValid(). */
	bool Initialize(const FPawnCommandCoreDeps& InDeps);

	/** True when the clock and the queue are bound. */
	bool IsValid() const { return Deps.Clock != nullptr && Deps.Queue != nullptr; }

	/**
	 * Submits one discrete command as a PawnCommand queue item to Origin's mailbox (ADR-0011 section 3: entry points
	 * build a payload and submit; nothing executes here). Kind, Sink and ClientSeq are stamped into the payload; the
	 * submit time is read from the injected clock. Returns false (and submits nothing) on an invalid core.
	 */
	bool Submit(const FCommandOrigin& Origin, ECommandKind Kind, const FCommandPayload& Payload);

	/** The current slot state (Idle until the slot stories land). */
	EPawnSlotState GetSlotState() const { return SlotState; }

	/** The server time of the last submit, read through the injected clock (0 before any). */
	double GetLastSubmitServerTime() const { return LastSubmitServerTime; }

	/** Commands submitted since construction. */
	int32 GetSubmittedCount() const { return SubmittedCount; }

	/** The injected seams (read-only). */
	const FPawnCommandCoreDeps& GetDeps() const { return Deps; }

private:
	/** Reads the injected clock, clamped so time never runs backwards inside the core. */
	double ReadClock();

	void ReportMisuse(EPawnCommandMisuse Misuse) const;

	FPawnCommandCoreDeps Deps;
	EPawnSlotState SlotState = EPawnSlotState::Idle;
	double LastSubmitServerTime = 0.0;
	double LastReadServerTime = 0.0;
	int32 SubmittedCount = 0;
};
