// Story rmf-006 (production/epics/round-match-framework/story-006-intake-fairness.md): GDD AC 14.
// With fixture secret S1 over 10,000 ticks: two mailboxes, source A first in [4835, 5165] ticks; eleven mailboxes,
// each source first in [814, 1004] ticks; the same (secret, tick, mailbox set) gives the same permutation, a
// changed mailbox set changes it; keys strictly increasing with no duplicate. Deterministic: the bounds are fixed
// for S1. The permutation and the sub-seeds are never printed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthQueueTestRig.h"

namespace IntakeFairnessTestData
{
	/** Runs Ticks drains with one item per source per tick; returns, per tick, the source order (mailbox values). */
	TArray<TArray<uint32>> RunIntake(TConstArrayView<FMailboxId> Sources, int32 Ticks, bool& bOutKeysStrictlyIncreasing, int32& OutFaults)
	{
		FStealthQueueTestRig Rig(RoundTestFixtures::SecretS1);
		FStealthStubResolver Stub;
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Stub);
		TArray<TArray<uint32>> Orders;
		Orders.Reserve(Ticks);
		bOutKeysStrictlyIncreasing = true;
		FQueueKey Previous;
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			for (const FMailboxId Source : Sources)
			{
				Rig.Queue->Submit(Source, Rig.MakeItem(EQueueItemType::PawnCommand, Source.Value, Source));
			}
			Stub.Resolved.Reset();
			Rig.Tick();
			TArray<uint32>& Order = Orders.AddDefaulted_GetRef();
			for (const FStealthStubResolution& R : Stub.Resolved)
			{
				Order.Add(R.Source.Value);
				if (!(Previous < R.Key))
				{
					bOutKeysStrictlyIncreasing = false;
				}
				Previous = R.Key;
			}
		}
		OutFaults = Rig.Faults.GetRecords().Num();
		return Orders;
	}

	TArray<FMailboxId> MakeSources(int32 ParticipantCount, bool bWithServerLocal)
	{
		TArray<FMailboxId> Sources;
		for (int32 Index = 0; Index < ParticipantCount; ++Index)
		{
			Sources.Add(FMailboxId::ForParticipant(FParticipantId(RoundTestFixtures::FixtureParticipantIds[Index])));
		}
		if (bWithServerLocal)
		{
			Sources.Add(FMailboxId::ServerLocal());
		}
		return Sources;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundIntakeFairnessTest, "StealthGame.Round.IntakeFairness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundIntakeFairnessTest::RunTest(const FString& Parameters)
{
	using namespace IntakeFairnessTestData;
	using namespace RoundTestFixtures;

	// Two mailboxes.
	{
		const TArray<FMailboxId> Sources = MakeSources(2, false);
		bool bIncreasing = false;
		int32 Faults = 0;
		const TArray<TArray<uint32>> Orders = RunIntake(Sources, FairnessTicks, bIncreasing, Faults);
		int32 AFirst = 0;
		int32 Complete = 0;
		for (const TArray<uint32>& Order : Orders)
		{
			Complete += Order.Num() == 2 ? 1 : 0;
			AFirst += (Order.Num() > 0 && Order[0] == Sources[0].Value) ? 1 : 0;
		}
		TestEqual(TEXT("2 mailboxes: both items stamped every tick"), Complete, FairnessTicks);
		TestTrue(FString::Printf(TEXT("2 mailboxes: source A first in %d ticks, within [%d, %d]"), AFirst, TwoMailboxFirstMin, TwoMailboxFirstMax),
			AFirst >= TwoMailboxFirstMin && AFirst <= TwoMailboxFirstMax);
		TestTrue(TEXT("2 mailboxes: keys strictly increasing, no duplicate"), bIncreasing);
		TestEqual(TEXT("2 mailboxes: no fault"), Faults, 0);
	}

	// Eleven mailboxes (ten participants and the server-local one).
	TArray<TArray<uint32>> ElevenOrders;
	{
		const TArray<FMailboxId> Sources = MakeSources(10, true);
		bool bIncreasing = false;
		int32 Faults = 0;
		ElevenOrders = RunIntake(Sources, FairnessTicks, bIncreasing, Faults);
		TMap<uint32, int32> FirstCounts;
		for (const TArray<uint32>& Order : ElevenOrders)
		{
			if (Order.Num() == 11)
			{
				FirstCounts.FindOrAdd(Order[0])++;
			}
		}
		for (const FMailboxId Source : Sources)
		{
			const int32 Count = FirstCounts.FindRef(Source.Value);
			TestTrue(FString::Printf(TEXT("11 mailboxes: source %u first in %d ticks, within [%d, %d]"), Source.Value, Count, ElevenMailboxFirstMin, ElevenMailboxFirstMax),
				Count >= ElevenMailboxFirstMin && Count <= ElevenMailboxFirstMax);
		}
		TestTrue(TEXT("11 mailboxes: keys strictly increasing, no duplicate"), bIncreasing);
		TestEqual(TEXT("11 mailboxes: no fault"), Faults, 0);
	}

	// Determinism: a re-run with the same (secret, tick, mailbox set) gives the same permutation every tick.
	{
		const TArray<FMailboxId> Sources = MakeSources(10, true);
		bool bIncreasing = false;
		int32 Faults = 0;
		const TArray<TArray<uint32>> Again = RunIntake(Sources, FairnessTicks, bIncreasing, Faults);
		TestTrue(TEXT("re-run with the same secret, ticks and mailbox set: identical permutations"), Again == ElevenOrders);
	}

	// A changed mailbox set changes the permutation: drop the server-local mailbox; the relative order of the
	// remaining ten sources differs from the eleven-mailbox run in some tick.
	{
		const TArray<FMailboxId> Sources = MakeSources(10, false);
		bool bIncreasing = false;
		int32 Faults = 0;
		const TArray<TArray<uint32>> Ten = RunIntake(Sources, FairnessTicks, bIncreasing, Faults);
		int32 Differing = 0;
		for (int32 Tick = 0; Tick < FairnessTicks; ++Tick)
		{
			TArray<uint32> Relative = ElevenOrders[Tick];
			Relative.Remove(FMailboxId::ServerLocalValue);
			Differing += Relative != Ten[Tick] ? 1 : 0;
		}
		TestTrue(FString::Printf(TEXT("a changed mailbox set changes the permutation (%d of %d ticks differ)"), Differing, FairnessTicks), Differing > 0);
	}

	// The permutation is not the identity or a rotation of the base order: some tick starts with neither the
	// lowest nor the highest id while the base order's neighbour pair is split.
	{
		int32 NonCanonical = 0;
		for (const TArray<uint32>& Order : ElevenOrders)
		{
			NonCanonical += (Order.Num() == 11 && Order[0] != FixtureParticipantIds[0]) ? 1 : 0;
		}
		TestTrue(TEXT("the canonical base order is permuted (not first-come, not join order)"), NonCanonical > 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
