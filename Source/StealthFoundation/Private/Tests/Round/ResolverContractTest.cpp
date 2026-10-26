// Story rmf-008 (production/epics/round-match-framework/story-008-resolver-contract.md): GDD AC 46.
// A Voided resolver changes nothing, emits nothing, and one Voided entry is logged; an in-slot outcome consumes no
// Seq and carries the resolving key; a deferred outcome (MVP: the generic Submit from inside a slot) has a later
// key and its CauseKey; GetKey, Compare and CurrentKey return the expected keys for a fixture sequence.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthQueueTestRig.h"

namespace ResolverContractTestData
{
	constexpr uint32 VoidOwnerCode = 77;
	constexpr uint32 OutcomeTag = 5;
	constexpr uint32 DeferredTag = 6;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundResolverContractTest, "StealthGame.Round.ResolverContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundResolverContractTest::RunTest(const FString& Parameters)
{
	using namespace ResolverContractTestData;
	using namespace RoundTestFixtures;

	// 1. Voided: no state change, nothing emitted, exactly one Voided entry.
	{
		FStealthQueueTestRig Rig;
		FStealthSpyFlushBarrier Barrier;
		Rig.Queue->SetEventFlushBarrier(&Barrier);
		FStealthStubResolver Voider;
		FStealthStubResolver FollowOns;
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Voider);
		Rig.Queue->RegisterResolver(EQueueItemType::DebugItem, &FollowOns);
		Voider.OnResolve = [&Rig](const FQueueItem&, FQueueKey)
		{
			Rig.Queue->SetVoidCode(VoidOwnerCode);  // the owner's code travels in the entry
			return EResolveResult::Voided;
		};
		Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 1));
		Rig.Tick();
		TestEqual(TEXT("the voiding resolver ran once"), Voider.Resolved.Num(), 1);
		if (TestEqual(TEXT("exactly one log entry"), Rig.Log.Entries.Num(), 1))
		{
			const FRoundLogEntry& Entry = Rig.Log.Entries[0];
			TestTrue(TEXT("it is a Voided entry"), Entry.Type == ERoundLogType::Voided);
			TestTrue(TEXT("reason Revalidation"), Entry.Code == static_cast<uint16>(EVoidReason::Revalidation));
			TestTrue(TEXT("names the item type"), Entry.Data[0] == static_cast<uint32>(EQueueItemType::PawnCommand));
			TestTrue(TEXT("carries the owner's code"), Entry.Data[1] == VoidOwnerCode);
			TestTrue(TEXT("carries the item's key"), Entry.Key == Voider.Resolved[0].Key);
		}
		TestEqual(TEXT("nothing emitted: no follow-on ran"), FollowOns.Resolved.Num(), 0);
		TestEqual(TEXT("nothing emitted: no item submitted"), Rig.Queue->GetPendingMailboxItemCount(), 0);
		TestEqual(TEXT("nothing emitted: no timer scheduled"), Rig.Queue->GetPendingTimerCount(), 0);
		TestEqual(TEXT("only the item's own stamping consumed a Seq"), Rig.Queue->PeekNextSeq(), static_cast<uint64>(2));
		TestTrue(TEXT("slot outcome Voided"), Rig.Queue->GetLastDrainRecords().Num() == 1 && Rig.Queue->GetLastDrainRecords()[0].Outcome == EQueueSlotOutcome::Voided);
		TestEqual(TEXT("the slot still ended with a flush"), Barrier.FlushCount, 1);
	}

	// 2. In-slot outcome: inherits the resolving key, consumes no Seq. 3. Deferred outcome: later key, CauseKey.
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Command;
		FStealthStubResolver Outcome;
		FStealthStubResolver Deferred;
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Command);
		Rig.Queue->RegisterResolver(EQueueItemType::DebugItem, &Outcome);
		Rig.Queue->RegisterResolver(EQueueItemType::PawnLost, &Deferred);
		uint64 SeqBeforeOutcome = 0;
		uint64 SeqAfterOutcome = 0;
		FQueueKey CauseSeenByDeferred;
		Command.OnResolve = [&](const FQueueItem&, FQueueKey Key)
		{
			SeqBeforeOutcome = Rig.Queue->PeekNextSeq();
			Rig.Queue->ExecuteFollowOn(Rig.MakeItem(EQueueItemType::DebugItem, OutcomeTag));
			SeqAfterOutcome = Rig.Queue->PeekNextSeq();
			Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnLost, DeferredTag));
			return EResolveResult::Executed;
		};
		Deferred.OnResolve = [&](const FQueueItem&, FQueueKey)
		{
			CauseSeenByDeferred = Rig.Queue->CurrentCauseKey();
			return EResolveResult::Executed;
		};
		Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 1));
		Rig.Tick();
		if (TestEqual(TEXT("the command resolved"), Command.Resolved.Num(), 1) && TestEqual(TEXT("the in-slot outcome ran"), Outcome.Resolved.Num(), 1))
		{
			TestTrue(TEXT("the in-slot outcome carries the resolving key"), Outcome.Resolved[0].Key == Command.Resolved[0].Key);
			TestEqual(TEXT("the in-slot outcome consumed no Seq"), SeqAfterOutcome, SeqBeforeOutcome);
		}
		TestEqual(TEXT("the deferred outcome is not resolved in its cause's drain"), Deferred.Resolved.Num(), 0);
		Rig.Tick();
		if (TestEqual(TEXT("the deferred outcome resolved in a later drain"), Deferred.Resolved.Num(), 1) && Command.Resolved.Num() == 1)
		{
			TestTrue(TEXT("the deferred outcome has a later key"), FResolutionQueueCore::Compare(Command.Resolved[0].Key, Deferred.Resolved[0].Key) < 0);
			TestTrue(TEXT("the deferred outcome records its CauseKey"), CauseSeenByDeferred == Command.Resolved[0].Key);
			const TArray<FQueueSlotRecord>& Records = Rig.Queue->GetLastDrainRecords();
			TestTrue(TEXT("the slot record carries the CauseKey"), Records.Num() == 1 && Records[0].CauseKey == Command.Resolved[0].Key);
		}
	}

	// 4. Key primitives over the fixture sequence (tick 100: two items; tick 101: one item).
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Stub;
		TArray<FQueueKey> CurrentKeys;
		Stub.OnResolve = [&](const FQueueItem&, FQueueKey Key)
		{
			CurrentKeys.Add(Rig.Queue->CurrentKey());
			return EResolveResult::Executed;
		};
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Stub);
		Rig.Clock.AdvanceTicks(ResolverContractFirstTick);
		TArray<FQueueItemHandle> Handles;
		Handles.Add(Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 1)));
		Handles.Add(Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 2)));
		TestFalse(TEXT("GetKey is unset before stamping"), Rig.Queue->GetKey(Handles[0]).IsSet());
		Rig.Queue->Drain();
		Handles.Add(Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 3)));
		Rig.Clock.AdvanceTicks(1);
		Rig.Queue->Drain();

		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(ResolverContractExpectedKeys)); ++Index)
		{
			const FQueueKey Expected(ResolverContractExpectedKeys[Index][0], ResolverContractExpectedKeys[Index][1]);
			const TOptional<FQueueKey> Got = Rig.Queue->GetKey(Handles[Index]);
			TestTrue(FString::Printf(TEXT("GetKey(item %d) is the fixture key"), Index), Got.IsSet() && Got.GetValue() == Expected);
			TestTrue(FString::Printf(TEXT("CurrentKey inside slot %d is the fixture key"), Index), CurrentKeys.IsValidIndex(Index) && CurrentKeys[Index] == Expected);
		}
		const FQueueKey K1(ResolverContractExpectedKeys[0][0], ResolverContractExpectedKeys[0][1]);
		const FQueueKey K2(ResolverContractExpectedKeys[1][0], ResolverContractExpectedKeys[1][1]);
		const FQueueKey K3(ResolverContractExpectedKeys[2][0], ResolverContractExpectedKeys[2][1]);
		TestEqual(TEXT("Compare(K1, K2) = -1 (Seq decides within a tick)"), FResolutionQueueCore::Compare(K1, K2), -1);
		TestEqual(TEXT("Compare(K3, K2) = 1 (tick decides first)"), FResolutionQueueCore::Compare(K3, K2), 1);
		TestEqual(TEXT("Compare(K2, K2) = 0"), FResolutionQueueCore::Compare(K2, K2), 0);
		TestEqual(TEXT("Compare is lexicographic: a later tick with a lower Seq still sorts after"), FResolutionQueueCore::Compare(FQueueKey(K1.ServerTickIndex + 1, 0), K2), 1);
		TestFalse(TEXT("CurrentKey is unset outside a slot"), Rig.Queue->CurrentKey().IsSet());
		TestFalse(TEXT("not in a slot after the drain"), Rig.Queue->IsInSlot());

		const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({});
		TestTrue(Teardown.Describe(), Teardown.IsClean());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
