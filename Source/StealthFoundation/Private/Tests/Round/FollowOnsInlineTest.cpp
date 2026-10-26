// Story rmf-009 (production/epics/round-match-framework/story-009-follow-ons-inline.md): GDD AC 44.
// An item whose resolver triggers a transition, OnPawnKilled and two subscribers runs them all depth-first inside
// its slot, and every Roster event is dispatched (flush barrier) before the next item is dequeued. A cascade of
// depth 9 logs CascadeOverflow and submits the rest in raised order with the slot's RootCauseKey; depth 8 does not
// overflow (QueueCascadeDepthMax = 8; the item's resolver is depth 0, each nested follow-on one deeper).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthQueueTestRig.h"

namespace FollowOnsInlineTestData
{
	/** FDebugPayload::Command values of the test's follow-on kinds. */
	constexpr uint8 CmdPawnKilled = 1;
	constexpr uint8 CmdSubscriber = 2;
	constexpr uint8 CmdChain = 3;
	constexpr uint8 CmdLeaf = 4;
	constexpr uint32 LeafX = 100;
	constexpr uint32 LeafY = 101;

	FQueueItem MakeDebug(const FStealthQueueTestRig& Rig, uint8 Command, uint32 Arg)
	{
		FDebugPayload Payload;
		Payload.Command = Command;
		Payload.Arg = Arg;
		return FQueueItem::Make(EQueueItemType::DebugItem, Rig.Epoch.Current, FMailboxId::ServerLocal(), Payload);
	}

	struct FTrace
	{
		FString What;
		FQueueKey Key;
		bool bInSlot = false;
		int32 Depth = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundFollowOnsInlineTest, "StealthGame.Round.FollowOnsInline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundFollowOnsInlineTest::RunTest(const FString& Parameters)
{
	using namespace FollowOnsInlineTestData;

	// 1. Transition, OnPawnKilled and two subscribers, depth-first in the slot; events flushed before the next item.
	{
		FStealthQueueTestRig Rig;
		FStealthSpyFlushBarrier Roster;
		Rig.Queue->SetEventFlushBarrier(&Roster);
		TArray<FTrace> Trace;
		const auto Note = [&Rig, &Trace](const FString& What)
		{
			Trace.Add({ What, Rig.Queue->CurrentKey(), Rig.Queue->IsInSlot(), Rig.Queue->GetFollowOnDepth() });
		};
		Roster.OnDispatch = [&Note](const FString& Event) { Note(FString::Printf(TEXT("Event:%s"), *Event)); };

		FStealthStubResolver Strike;
		Strike.OnResolve = [&](const FQueueItem& Item, FQueueKey)
		{
			const uint32 Tag = FStealthStubResolver::ArgOf(Item);
			Note(FString::Printf(TEXT("Resolve%u"), Tag));
			if (Tag == 1)
			{
				Roster.Pending.Add(TEXT("StateChanged"));  // the Roster transition queues its event
				Note(TEXT("Transition"));
				Rig.Queue->ExecuteFollowOn(MakeDebug(Rig, CmdPawnKilled, 0));
			}
			return EResolveResult::Executed;
		};
		FStealthStubResolver Debug;
		Debug.OnResolve = [&](const FQueueItem& Item, FQueueKey)
		{
			const FDebugPayload& Payload = Item.Payload.Get<FDebugPayload>();
			if (Payload.Command == CmdPawnKilled)
			{
				Note(TEXT("OnPawnKilled"));
				Rig.Queue->ExecuteFollowOn(MakeDebug(Rig, CmdSubscriber, 1));  // subscribers in registration order
				Rig.Queue->ExecuteFollowOn(MakeDebug(Rig, CmdSubscriber, 2));
			}
			else
			{
				Note(FString::Printf(TEXT("Subscriber%u"), Payload.Arg));
			}
			return EResolveResult::Executed;
		};
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Strike);
		Rig.Queue->RegisterResolver(EQueueItemType::DebugItem, &Debug);
		Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 1));
		Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 2));
		Rig.Tick();

		const TCHAR* Expected[] = { TEXT("Resolve1"), TEXT("Transition"), TEXT("OnPawnKilled"), TEXT("Subscriber1"), TEXT("Subscriber2"), TEXT("Event:StateChanged"), TEXT("Resolve2") };
		const int32 ExpectedDepth[] = { 0, 0, 1, 2, 2, 0, 0 };
		if (TestEqual(TEXT("trace length"), Trace.Num(), static_cast<int32>(UE_ARRAY_COUNT(Expected))))
		{
			for (int32 Index = 0; Index < Trace.Num(); ++Index)
			{
				TestEqual(FString::Printf(TEXT("step %d"), Index), Trace[Index].What, FString(Expected[Index]));
				TestTrue(FString::Printf(TEXT("step %d runs inside a slot"), Index), Trace[Index].bInSlot);
				TestEqual(FString::Printf(TEXT("step %d depth"), Index), Trace[Index].Depth, ExpectedDepth[Index]);
			}
			for (int32 Index = 1; Index <= 5; ++Index)
			{
				TestTrue(FString::Printf(TEXT("step %d carries the first item's key"), Index), Trace[Index].Key == Trace[0].Key);
			}
			TestTrue(TEXT("the second item has a later key"), Trace[0].Key < Trace[6].Key);
		}
		TestEqual(TEXT("one flush per slot"), Roster.FlushCount, 2);
		const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({});
		TestTrue(Teardown.Describe(), Teardown.IsClean());
	}

	// 2. Cascades: a chain item with Arg n raises a chain item with Arg n - 1; Arg 1 raises two leaves X and Y.
	const auto RunCascade = [](uint32 ChainStart, TArray<FStealthStubResolution>& OutLeaves, FQueueKey& OutStrikeKey, FQueueKey& OutLaterKey,
		TArray<FQueueSlotRecord>& OutRecords, int32& OutOverflowEntries, int32& OutOverflowFaults)
	{
		FStealthQueueTestRig Rig;
		FStealthStubResolver Strike;
		Strike.OnResolve = [&Rig, ChainStart](const FQueueItem& Item, FQueueKey)
		{
			if (FStealthStubResolver::ArgOf(Item) == 1)
			{
				Rig.Queue->ExecuteFollowOn(MakeDebug(Rig, CmdChain, ChainStart));
			}
			return EResolveResult::Executed;
		};
		FStealthStubResolver Debug;
		Debug.OnResolve = [&Rig, &OutLeaves](const FQueueItem& Item, FQueueKey Key)
		{
			const FDebugPayload& Payload = Item.Payload.Get<FDebugPayload>();
			if (Payload.Command == CmdChain && Payload.Arg > 1)
			{
				Rig.Queue->ExecuteFollowOn(MakeDebug(Rig, CmdChain, Payload.Arg - 1));
			}
			else if (Payload.Command == CmdChain)
			{
				Rig.Queue->ExecuteFollowOn(MakeDebug(Rig, CmdLeaf, LeafX));
				Rig.Queue->ExecuteFollowOn(MakeDebug(Rig, CmdLeaf, LeafY));
			}
			else
			{
				OutLeaves.Add({ Key, Item.Type, Item.Source, Payload.Arg, false });
			}
			return EResolveResult::Executed;
		};
		Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Strike);
		Rig.Queue->RegisterResolver(EQueueItemType::DebugItem, &Debug);
		Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 1));
		Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 2));
		Rig.Tick();
		OutStrikeKey = Strike.Resolved.Num() > 0 ? Strike.Resolved[0].Key : FQueueKey();
		OutLaterKey = Strike.Resolved.Num() > 1 ? Strike.Resolved[1].Key : FQueueKey();
		OutRecords = Rig.Queue->GetLastDrainRecords();
		OutOverflowEntries = Rig.Log.CountOfType(ERoundLogType::CascadeOverflow);
		OutOverflowFaults = Rig.Faults.GetRecords().FilterByPredicate([](const FStealthFaultRecord& F) { return F.Fault == EStealthFault::QueueCascadeOverflow; }).Num();
	};

	// Depth 8: chain Arg 7 runs at depths 1..7 and its leaves at depth 8: inline, no overflow.
	{
		TArray<FStealthStubResolution> Leaves;
		FQueueKey StrikeKey, LaterKey;
		TArray<FQueueSlotRecord> Records;
		int32 OverflowEntries = 0, OverflowFaults = 0;
		RunCascade(FResolutionQueueCore::QueueCascadeDepthMax - 1, Leaves, StrikeKey, LaterKey, Records, OverflowEntries, OverflowFaults);
		TestEqual(TEXT("depth 8: no CascadeOverflow entry"), OverflowEntries, 0);
		TestEqual(TEXT("depth 8: no overflow fault"), OverflowFaults, 0);
		TestTrue(TEXT("depth 8: both leaves ran inline in the slot"), Leaves.Num() == 2 && Leaves[0].Key == StrikeKey && Leaves[1].Key == StrikeKey);
		TestEqual(TEXT("depth 8: only the two submitted items got slots"), Records.Num(), 2);
	}

	// Depth 9: chain Arg 8 runs at depths 1..8; its leaves would be depth 9: CascadeOverflow, submitted in order.
	{
		TArray<FStealthStubResolution> Leaves;
		FQueueKey StrikeKey, LaterKey;
		TArray<FQueueSlotRecord> Records;
		int32 OverflowEntries = 0, OverflowFaults = 0;
		RunCascade(FResolutionQueueCore::QueueCascadeDepthMax, Leaves, StrikeKey, LaterKey, Records, OverflowEntries, OverflowFaults);
		TestEqual(TEXT("depth 9: one CascadeOverflow entry for the slot"), OverflowEntries, 1);
		TestEqual(TEXT("depth 9: the overflow is reported to the fault sink"), OverflowFaults, 1);
		if (TestEqual(TEXT("depth 9: both leaves still ran"), Leaves.Num(), 2))
		{
			TestEqual(TEXT("depth 9: leaves run in raised order (X first)"), Leaves[0].Arg, LeafX);
			TestEqual(TEXT("depth 9: then Y"), Leaves[1].Arg, LeafY);
			TestTrue(TEXT("depth 9: the leaves left the slot (new keys)"), StrikeKey < Leaves[0].Key && Leaves[0].Key < Leaves[1].Key);
			TestTrue(TEXT("depth 9: the leaves come after the already-stamped next item"), LaterKey < Leaves[0].Key);
		}
		if (TestEqual(TEXT("depth 9: four slots (two items, two moved leaves)"), Records.Num(), 4))
		{
			TestTrue(TEXT("depth 9: moved leaves carry the originating slot's RootCauseKey"), Records[2].RootCauseKey == StrikeKey && Records[3].RootCauseKey == StrikeKey);
			TestTrue(TEXT("depth 9: an ordinary item's root cause is its own key"), Records[1].RootCauseKey == Records[1].Key);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
