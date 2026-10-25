#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthClock.h"
#include "Server/StealthFaultSink.h"

/**
 * Injected dependencies of FRosterCore (GDD participant-state-roster "Tags" list of seven injected
 * dependencies; ADR-0007 section 5: a plain core takes its inputs through constructor arguments or small
 * interfaces; story psr-001).
 *
 * The server clock and the fault sink reuse the Foundation seams IStealthClock (story tdc-007) and
 * IStealthFaultSink (story tdc-008) rather than Roster-specific copies. The Round-phase input
 * (IRosterRoundPhase) is the extra injected dependency ruled by decision D43 (2026-10-25).
 *
 * All interfaces are plain C++ (no UObject); the URosterStore facade adapts engine objects to them.
 */

/**
 * The opaque pawn handle the core sees: an object key plus the generation the pawn had when the handle
 * was made (R9 tombstones are keyed by object identity plus generation, so a recycled pawn never inherits
 * one, I5). The facade derives it from the engine object (FObjectKey and a per-pawn spawn generation).
 * ObjectKey 0 is the null pawn.
 */
struct FRosterPawnHandle
{
	uint64 ObjectKey = 0;
	uint32 Generation = 0;

	FRosterPawnHandle() = default;
	constexpr FRosterPawnHandle(uint64 InObjectKey, uint32 InGeneration) : ObjectKey(InObjectKey), Generation(InGeneration) {}

	/** True for any non-null handle (the pawn may still be destroyed; ask IRosterPawnLiveness). */
	bool IsSet() const { return ObjectKey != 0; }

	bool operator==(const FRosterPawnHandle& Other) const { return ObjectKey == Other.ObjectKey && Generation == Other.Generation; }
	bool operator!=(const FRosterPawnHandle& Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FRosterPawnHandle& Handle) { return HashCombine(::GetTypeHash(Handle.ObjectKey), ::GetTypeHash(Handle.Generation)); }
};

/** Liveness of a pawn object, as the engine reports it. */
enum class ERosterPawnLiveness : uint8
{
	Valid,
	Destroyed,
	PendingKill,
};

/** Answers whether a pawn object is valid, destroyed or pending kill (R10 Stale rule). */
class IRosterPawnLiveness
{
public:
	virtual ~IRosterPawnLiveness() = default;

	/** Liveness of the object behind Pawn (never called for a null handle). */
	virtual ERosterPawnLiveness GetLiveness(const FRosterPawnHandle& Pawn) const = 0;
};

/** Answers "is this process the authority" (R2). False on a client process. */
class IRosterAuthority
{
public:
	virtual ~IRosterAuthority() = default;

	virtual bool HasAuthority() const = 0;
};

/** Answers "is the caller on the game thread" (R2, I7). */
class IRosterThreadCheck
{
public:
	virtual ~IRosterThreadCheck() = default;

	virtual bool IsGameThread() const = 0;
};

/**
 * The server-only random draws the Roster makes (R3 `MatchId`, `PublicHandle`). Values must match the
 * ADR-0006 seed domains so the facade can map them to ESeedDomain without a lookup table.
 */
enum class ERosterRandomDomain : uint32
{
	/** ADR-0006 domain 15: the random 32-bit PublicHandle, counter-as-Attempt per registration draw. */
	PublicHandle = 15,
	/** ADR-0006 domain 17 (decision D46): the random 64-bit MatchId, counter-as-Attempt per BeginMatch draw. */
	MatchId = 17,
};

/**
 * Server-only random source (R3: "a server-only random source"). The production adapter returns
 * GetServerSubSeed(Domain, Counter) (ADR-0006 F5); a test injects a deterministic source. The core
 * discards unusable draws (0, a repeat) and advances the counter itself.
 */
class IRosterRandom
{
public:
	virtual ~IRosterRandom() = default;

	/** One 64-bit draw for Domain at attempt Counter. Deterministic for a given (Domain, Counter) within a match. */
	virtual uint64 Draw(ERosterRandomDomain Domain, uint64 Counter) = 0;
};

/**
 * Round & Match Framework's phase, as far as the Roster needs it (decision D43: the Round-phase input is an
 * injected dependency). R9 faults a live binding cleared by Destroyed only in phase Active; F4
 * IsTeamEliminated answers only in phase Active.
 */
class IRosterRoundPhase
{
public:
	virtual ~IRosterRoundPhase() = default;

	/** True while Round's phase is Active. */
	virtual bool IsRoundPhaseActive() const = 0;
};

/** The seven injected dependencies plus the Round-phase input. Every pointer must be non-null and outlive the core. */
struct FRosterCoreDeps
{
	IRosterPawnLiveness* Liveness = nullptr;
	IRosterAuthority* Authority = nullptr;
	IRosterThreadCheck* ThreadCheck = nullptr;
	IStealthClock* Clock = nullptr;
	IRosterRandom* Random = nullptr;
	IStealthFaultSink* FaultSink = nullptr;
	IRosterRoundPhase* RoundPhase = nullptr;

	/**
	 * Pawn-binding table capacity reserved at BeginMatch (ADR-0005 2026-10-25b: reserved for the maximum pawn
	 * count, NpcCount hard maximum plus participants, so a bind never grows the table). The facade passes the
	 * value derived from the tuning hard maximum; the default only serves tests.
	 */
	int32 PawnTableCapacity = 512;
};
