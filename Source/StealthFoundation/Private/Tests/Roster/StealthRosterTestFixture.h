#pragma once

// Shared fixture of the StealthGame.Roster.* core tests (stories psr-001..014). Test-only: compiled only
// under WITH_STEALTH_TEST_SEAMS (it uses the core seams). Reuses the Foundation clock and fault-sink seams
// (stories tdc-007, tdc-008) and adds deterministic fakes for the Roster-specific dependencies.

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/Roster/StealthRosterCore.h"
#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"

/** Authority provider a test flips (true = server). */
class FRosterFakeAuthority : public IRosterAuthority
{
public:
	bool bAuthority = true;
	virtual bool HasAuthority() const override { return bAuthority; }
};

/** Thread check a test flips (true = game thread). Deterministic: no real worker thread is needed. */
class FRosterFakeThreadCheck : public IRosterThreadCheck
{
public:
	bool bGameThread = true;
	virtual bool IsGameThread() const override { return bGameThread; }
};

/** Pawn liveness: every pawn is Valid unless the test overrides it. */
class FRosterFakeLiveness : public IRosterPawnLiveness
{
public:
	TMap<FRosterPawnHandle, ERosterPawnLiveness> Overrides;

	virtual ERosterPawnLiveness GetLiveness(const FRosterPawnHandle& Pawn) const override
	{
		const ERosterPawnLiveness* Found = Overrides.Find(Pawn);
		return Found ? *Found : ERosterPawnLiveness::Valid;
	}
};

/** Deterministic random source: SplitMix64 over (seed, domain, counter). Same seed, same draws. */
class FRosterFakeRandom : public IRosterRandom
{
public:
	explicit FRosterFakeRandom(uint64 InSeed) : Seed(InSeed) {}

	virtual uint64 Draw(ERosterRandomDomain Domain, uint64 Counter) override
	{
		uint64 Z = Seed ^ (static_cast<uint64>(Domain) << 56) ^ (Counter * 0x9E3779B97F4A7C15ull);
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		return Z ^ (Z >> 31);
	}

	uint64 Seed;
};

/** Round phase a test flips. */
class FRosterFakeRoundPhase : public IRosterRoundPhase
{
public:
	bool bActive = false;
	virtual bool IsRoundPhaseActive() const override { return bActive; }
};

/** Records every dispatched event, in dispatch order. */
class FRosterEventRecorder : public IRosterEventListener
{
public:
	TArray<FRosterEvent> Events;

	virtual void OnRosterEvent(const FRosterEvent& Event) override { Events.Add(Event); }

	int32 CountOf(ERosterEventType Type) const
	{
		return Events.FilterByPredicate([Type](const FRosterEvent& E) { return E.Type == Type; }).Num();
	}
};

namespace RosterTestData
{
	/** Seed of the fixture's random source. Its first ten PublicHandle draws are not ascending (AC-PSR-05). */
	constexpr uint64 DefaultSeed = 0x5EED0000C0FFEEull;
	/** The default team size of the 5v5 fixtures. */
	constexpr int32 FullTeamSize = 5;

	/** The identity key of test participant Index (distinct per index). */
	inline FIdentityKey Key(int32 Index) { return FIdentityKey(0xA11CE00000000000ull, 1000ull + static_cast<uint64>(Index)); }

	/** A test pawn handle: object Index at Generation (defaults to 1). */
	inline FRosterPawnHandle Pawn(int32 Index, uint32 Generation = 1) { return FRosterPawnHandle(0xBA5E000000ull + static_cast<uint64>(Index), Generation); }
}

/** The core plus every injected fake. Members are declared before the core so they outlive it. */
struct FRosterTestFixture
{
	FFakeStealthClock Clock;
	FStealthRecordingFaultSink Sink;
	FRosterFakeAuthority Authority;
	FRosterFakeThreadCheck ThreadCheck;
	FRosterFakeLiveness Liveness;
	FRosterFakeRandom Random;
	FRosterFakeRoundPhase RoundPhase;
	FRosterCore Core;

	explicit FRosterTestFixture(uint64 Seed = RosterTestData::DefaultSeed)
		: Clock(100.0)
		, Random(Seed)
		, Core(MakeDeps())
	{
	}

	FRosterCoreDeps MakeDeps()
	{
		FRosterCoreDeps Deps;
		Deps.Liveness = &Liveness;
		Deps.Authority = &Authority;
		Deps.ThreadCheck = &ThreadCheck;
		Deps.Clock = &Clock;
		Deps.Random = &Random;
		Deps.FaultSink = &Sink;
		Deps.RoundPhase = &RoundPhase;
		return Deps;
	}

	/** Registers participant Index by its key; returns the id (invalid when rejected). */
	FParticipantId Register(int32 Index)
	{
		return Core.RegisterParticipant(RosterTestData::Key(Index), FString::Printf(TEXT("P%d"), Index)).Id;
	}

	/** BeginMatch(InTeamSize) then registers participants 0..Count-1; returns their ids in order. */
	TArray<FParticipantId> BeginAndRegister(int32 InTeamSize, int32 Count)
	{
		Core.BeginMatch(InTeamSize);
		TArray<FParticipantId> Ids;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Ids.Add(Register(Index));
		}
		return Ids;
	}

	/** Assigns Ids[0..Split-1] to team 0 and the rest to team 1 in one batch. */
	ERosterResult AssignSplit(const TArray<FParticipantId>& Ids, int32 Split)
	{
		TArray<FRosterTeamAssignment> Batch;
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			Batch.Add({ Ids[Index], Index < Split ? 0 : 1 });
		}
		return Core.AssignTeams(Batch);
	}

	/** The record of Id (the test fails loudly elsewhere if it is missing; this returns a default then). */
	FParticipantRecord Record(FParticipantId Id) const
	{
		const FParticipantRecord* Found = Core.FindRecord(Id);
		return Found ? *Found : FParticipantRecord();
	}
};

#endif // WITH_STEALTH_TEST_SEAMS
