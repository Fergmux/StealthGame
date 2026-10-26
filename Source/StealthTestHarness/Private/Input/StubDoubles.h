#pragma once

// Named stub doubles of the cross-epic seams the pawn command interface consumes (story iif-095, ADR-0011 Validation
// Criteria; the predicate stub first landed with story iif-003). Shared by every IIF harness test so parity tests use
// the same fixtures. Each double records the calls made on it and carries no caller identity. The harness module is
// excluded from Shipping and Test (.uproject TargetConfigurationDenyList), so none of this ships.
//
//   FStubResolutionQueue       IPawnCommandQueue: records Submit/ScheduleAt (items, mailboxes, due times); Drain()
//                              hands each recorded item to a callback inside a "slot" in submit order.
//   FStubRosterLegality        IRosterLegalityQuery: a configured class (Unbound maps to Active, by the caller).
//   FStubSharedPawnStateView   ISharedPawnStateView: configured IsRunning / IsInTraversal / displacement.
//   FStubCamera                the Camera aim ray (ADR-0015 GetAimRay; the interface lands with the camera epic).
//   FStubInteractTargetOwner   Objectives' interact-target owner (GetInteractHoldSec, OnInteractExecuted/Ended).
//   UStubCommandPredicate      ICommandPredicate (Assassination, Concealment, Objectives): a configured verdict.

#include "CoreMinimal.h"
#include "Server/Core/CommandPredicate.h"
#include "Server/Core/PawnCommandCore.h"
#include "UObject/Object.h"

#include "StubDoubles.generated.h"

/**
 * ICommandPredicate stub: answers the configured verdict and records each call. A UObject so the hub's weak
 * registration can go stale when it is destroyed (MarkAsGarbage + GC, or simply a dropped strong reference).
 */
UCLASS(NotBlueprintType, NotBlueprintable, Transient)
class UStubCommandPredicate : public UObject, public ICommandPredicate
{
	GENERATED_BODY()

public:
	/** The verdict every call answers. */
	FPredicateResult Verdict = FPredicateResult::Allow();

	/** Calls made (the stub's own bookkeeping; it changes no game state). */
	mutable int32 Calls = 0;

	virtual FPredicateResult CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const override
	{
		++Calls;
		return Verdict;
	}
};

/** IPawnCommandQueue stub mirroring ADR-0002 Submit / ScheduleAt, with a controllable drain. */
class FStubResolutionQueue : public IPawnCommandQueue
{
public:
	struct FSubmitted
	{
		FMailboxId Mailbox;
		FQueueItem Item;
	};

	struct FScheduled
	{
		double DueServerTime = 0.0;
		FQueueItem Item;
	};

	virtual void Submit(FMailboxId Mailbox, const FQueueItem& Item) override { Submitted.Add({ Mailbox, Item }); }
	virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) override { Scheduled.Add({ DueServerTime, Item }); }

	/**
	 * Runs every pending submitted item through Resolve in submit order, one "slot" each, with a stamped key
	 * (FirstTick, 1..N). bInSlot is true only inside Resolve. Drained items are moved to Drained.
	 */
	void Drain(TFunctionRef<void(const FQueueItem&, FQueueKey)> Resolve, uint64 TickIndex = 1)
	{
		uint64 Seq = 0;
		TArray<FSubmitted> Pending = MoveTemp(Submitted);
		Submitted.Reset();
		for (const FSubmitted& Entry : Pending)
		{
			bInSlot = true;
			Resolve(Entry.Item, FQueueKey(TickIndex, ++Seq));
			bInSlot = false;
			Drained.Add(Entry);
		}
	}

	TArray<FSubmitted> Submitted;
	TArray<FSubmitted> Drained;
	TArray<FScheduled> Scheduled;
	bool bInSlot = false;
};

/** IRosterLegalityQuery stub: a configured legality class; records the calls. */
class FStubRosterLegality : public IRosterLegalityQuery
{
public:
	virtual ECommandLegalityClass GetLegalityClass(const APawn& Pawn) const override
	{
		++Calls;
		return Class;
	}

	ECommandLegalityClass Class = ECommandLegalityClass::Active;
	mutable int32 Calls = 0;
};

/** ISharedPawnStateView stub; records the calls. */
class FStubSharedPawnStateView : public ISharedPawnStateView
{
public:
	virtual bool IsRunning() const override { ++Calls; return bRunning; }
	virtual bool IsInTraversal() const override { ++Calls; return bInTraversal; }
	virtual float CommandedDisplacementCm(double SinceServerTime) const override { ++Calls; return DisplacementCm; }

	bool bRunning = false;
	bool bInTraversal = false;
	float DisplacementCm = 0.f;
	mutable int32 Calls = 0;
};

/** Camera aim-ray stub (ADR-0015 GetAimRay); records the calls. */
class FStubCamera
{
public:
	/** The configured aim ray: origin and unit direction. */
	FRay GetAimRay() const
	{
		++Calls;
		return FRay(Origin, Direction, true);
	}

	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	mutable int32 Calls = 0;
};

/** Objectives' interact-target owner stub (ADR-0011 section 6 amendment, ADR-0020 section 5.1); records the calls. */
class FStubInteractTargetOwner
{
public:
	float GetInteractHoldSec() const
	{
		++HoldSecCalls;
		return HoldSec;
	}

	void OnInteractExecuted(const APawn* Pawn, uint16 CourtFixture, FQueueKey Key) { Executed.Add({ CourtFixture, Key }); }
	void OnInteractEnded(const APawn* Pawn, uint16 CourtFixture, bool bCompleted, FQueueKey Key) { Ended.Add({ CourtFixture, Key }); }

	float HoldSec = 3.f;
	mutable int32 HoldSecCalls = 0;
	TArray<TPair<uint16, FQueueKey>> Executed;
	TArray<TPair<uint16, FQueueKey>> Ended;
};
