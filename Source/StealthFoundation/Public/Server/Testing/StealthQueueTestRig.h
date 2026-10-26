#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/EventLog/StealthRoundLogWriter.h"
#include "Server/Queue/StealthEventFlushBarrier.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "Server/Testing/StealthRoundTestConstants.h"
#include "Templates/Function.h"

/**
 * Round & Match Framework queue test seams (GDD "Test seams (binding on implementation)" items 1, 2 and 4;
 * stories rmf-001, rmf-002, rmf-004). Compiled only under WITH_STEALTH_TEST_SEAMS.
 *
 *   FStealthFakeEntropySource   IEntropySource that returns a RoundTestSeeds fixture secret and nonce (rmf-002)
 *   FStealthStubResolver        IQueueResolver that records every key it resolves and returns a scripted result
 *   FStealthRecordingLogWriter  IRoundLogWriter that keeps every entry (stands in for FRoundEventLog)
 *   FStealthFakeQueueEpoch      IQueueEpochSource the test advances (rmf-010)
 *   FStealthFakeGenerations     IQueueConnectionGenerationSource the test sets per participant (rmf-007)
 *   FStealthQueueTestRig        all of the above plus FFakeStealthClock (rmf-001) and a recording fault sink,
 *                               wired into one FResolutionQueueCore by constructor injection (no singleton)
 */

/** Fills the secret then the nonce from a fixture (ADR-0006 Guideline 8: tests inject entropy). */
class FStealthFakeEntropySource : public IEntropySource
{
public:
	explicit FStealthFakeEntropySource(const RoundTestFixtures::FRoundTestSecret& InSecret)
	{
		AppendWord(InSecret.Word0);
		AppendWord(InSecret.Word1);
		AppendWord(InSecret.MatchNonce);
	}

	/** Makes the next Fill fail (the fatal-entropy path). */
	bool bFail = false;

	virtual bool Fill(TArrayView<uint8> Out) override
	{
		if (bFail || Cursor + Out.Num() > Bytes.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			Out[Index] = Bytes[Cursor++];
		}
		return true;
	}

private:
	void AppendWord(uint64 Word)
	{
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Bytes.Add(static_cast<uint8>((Word >> (8 * Index)) & 0xFF));
		}
	}

	TArray<uint8> Bytes;
	int32 Cursor = 0;
};

/** One key a stub resolved and what it resolved. */
struct FStealthStubResolution
{
	FQueueKey Key;
	EQueueItemType Type = EQueueItemType::PawnCommand;
	FMailboxId Source;
	uint32 Arg = 0;
	bool bRejectedLatched = false;
};

/**
 * Recording stub resolver. Returns Result (Executed by default) unless OnResolve is set, in which case OnResolve
 * decides. Never changes game state itself. Arg is the payload's Arg/Seq/Command field, so tests can tag items.
 */
class FStealthStubResolver : public IQueueResolver
{
public:
	EResolveResult Result = EResolveResult::Executed;
	TFunction<EResolveResult(const FQueueItem&, FQueueKey)> OnResolve;
	TFunction<void(const FQueueItem&, FQueueKey)> OnLatched;
	TArray<FStealthStubResolution> Resolved;

	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override
	{
		Resolved.Add({ Key, Item.Type, Item.Source, ArgOf(Item), false });
		return OnResolve ? OnResolve(Item, Key) : Result;
	}

	virtual void OnRejectedLatched(const FQueueItem& Item, FQueueKey Key) override
	{
		Resolved.Add({ Key, Item.Type, Item.Source, ArgOf(Item), true });
		if (OnLatched)
		{
			OnLatched(Item, Key);
		}
	}

	/** The keys resolved (not latched-rejected), in resolve order. */
	TArray<FQueueKey> GetResolvedKeys() const
	{
		TArray<FQueueKey> Keys;
		for (const FStealthStubResolution& R : Resolved)
		{
			if (!R.bRejectedLatched)
			{
				Keys.Add(R.Key);
			}
		}
		return Keys;
	}

	/** The tag of an item: FCommandPayload::Arg, FTimerPayload::Seq, FHoldTimerPayload::ActionEpoch, FDebugPayload::Arg, else Command/Transition. */
	static uint32 ArgOf(const FQueueItem& Item)
	{
		if (const FCommandPayload* P = Item.Payload.TryGet<FCommandPayload>()) { return P->Arg; }
		if (const FTimerPayload* P = Item.Payload.TryGet<FTimerPayload>()) { return P->Seq; }
		if (const FHoldTimerPayload* P = Item.Payload.TryGet<FHoldTimerPayload>()) { return P->ActionEpoch; }
		if (const FDebugPayload* P = Item.Payload.TryGet<FDebugPayload>()) { return P->Arg; }
		if (const FHostCommandPayload* P = Item.Payload.TryGet<FHostCommandPayload>()) { return P->Command; }
		if (const FConnectionTransitionPayload* P = Item.Payload.TryGet<FConnectionTransitionPayload>()) { return P->Transition; }
		return 0;
	}
};

/** Keeps every appended entry, in append order. */
class FStealthRecordingLogWriter : public IRoundLogWriter
{
public:
	TArray<FRoundLogEntry> Entries;

	virtual void Append(const FRoundLogEntry& Entry) override { Entries.Add(Entry); }

	int32 CountOfType(ERoundLogType Type) const
	{
		return Entries.FilterByPredicate([Type](const FRoundLogEntry& E) { return E.Type == Type; }).Num();
	}
};

class FStealthFakeQueueEpoch : public IQueueEpochSource
{
public:
	FEpochToken Current{ RoundTestFixtures::FixtureMatchId, RoundTestFixtures::FixtureRoundEpoch };

	virtual FEpochToken GetCurrentEpoch() const override { return Current; }
};

class FStealthFakeGenerations : public IQueueConnectionGenerationSource
{
public:
	TMap<uint32, uint32> Generations;

	virtual uint32 GetConnectionGeneration(FParticipantId Participant) const override
	{
		const uint32* Found = Generations.Find(Participant.Value);
		return Found != nullptr ? *Found : 0;
	}
};

/** Recording flush barrier: records each flush; a test may queue "events" it dispatches on flush. */
class FStealthSpyFlushBarrier : public IEventFlushBarrier
{
public:
	/** Events queued during the slot; FlushPending dispatches them FIFO through OnDispatch. */
	TArray<FString> Pending;
	TFunction<void(const FString&)> OnDispatch;
	int32 FlushCount = 0;

	virtual void FlushPending() override
	{
		++FlushCount;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)  // events raised during dispatch also flush (FIFO)
		{
			if (OnDispatch)
			{
				const FString Event = Pending[Index];  // copy: dispatch may queue more events
				OnDispatch(Event);
			}
		}
		Pending.Reset();
	}

	virtual bool HasPending() const override { return Pending.Num() > 0; }
};

/** Everything a queue test needs, wired by constructor injection. */
class FStealthQueueTestRig
{
public:
	explicit FStealthQueueTestRig(const RoundTestFixtures::FRoundTestSecret& Secret = RoundTestFixtures::SecretS1)
		: Entropy(Secret)
	{
		Seeds.Open(Entropy, &Faults);
		FResolutionQueueDeps Deps;
		Deps.Clock = &Clock;
		Deps.SubSeeds = &Seeds;
		Deps.Epoch = &Epoch;
		Deps.Generations = &Generations;
		Deps.Log = &Log;
		Deps.FaultSink = &Faults;
		Queue = MakeUnique<FResolutionQueueCore>(Deps);
	}

	/** Advances one server frame (tick + DeltaSeconds) and drains once. */
	void Tick(double DeltaSeconds = 1.0 / 60.0)
	{
		Clock.Advance(DeltaSeconds, 1);
		Queue->Drain();
	}

	/** An item of Type with a payload tagged Tag, under the current epoch. */
	FQueueItem MakeItem(EQueueItemType Type, uint32 Tag, FMailboxId Source = FMailboxId::ServerLocal()) const
	{
		switch (Type)
		{
		case EQueueItemType::PawnCommand: { FCommandPayload P; P.Arg = Tag; return FQueueItem::Make(Type, Epoch.Current, Source, P); }
		case EQueueItemType::HoldTimer: { FHoldTimerPayload P; P.ActionEpoch = Tag; return FQueueItem::Make(Type, Epoch.Current, Source, P); }
		case EQueueItemType::TimerFired: { FTimerPayload P; P.Seq = Tag; return FQueueItem::Make(Type, Epoch.Current, Source, P); }
		case EQueueItemType::PawnLost: { FPawnLostPayload P; P.Participant = FParticipantId(Tag); return FQueueItem::Make(Type, Epoch.Current, Source, P); }
		case EQueueItemType::ConnectionTransition: { FConnectionTransitionPayload P; P.Transition = static_cast<uint8>(Tag); return FQueueItem::Make(Type, Epoch.Current, Source, P); }
		case EQueueItemType::HostCommand: { FHostCommandPayload P; P.Command = static_cast<uint8>(Tag); return FQueueItem::Make(Type, Epoch.Current, Source, P); }
		case EQueueItemType::DebugItem:
		default: { FDebugPayload P; P.Arg = Tag; return FQueueItem::Make(EQueueItemType::DebugItem, Epoch.Current, Source, P); }
		}
	}

	/** A TimerFired item of Kind tagged Tag (Seq), with an optional participant subject. */
	FQueueItem MakeTimer(ETimerKind Kind, uint32 Tag, FParticipantId Subject = FParticipantId()) const
	{
		FTimerPayload P;
		P.Kind = Kind;
		P.Seq = Tag;
		if (Subject.IsValid())
		{
			P.Subject = FSubjectRef::ForParticipant(Subject);
		}
		return FQueueItem::Make(EQueueItemType::TimerFired, Epoch.Current, FMailboxId::ServerLocal(), P);
	}

	FFakeStealthClock Clock;
	FStealthFakeEntropySource Entropy;
	FStealthServerSeeds Seeds;
	FStealthFakeQueueEpoch Epoch;
	FStealthFakeGenerations Generations;
	FStealthRecordingLogWriter Log;
	FStealthRecordingFaultSink Faults;
	TUniquePtr<FResolutionQueueCore> Queue;
};

#endif // WITH_STEALTH_TEST_SEAMS
