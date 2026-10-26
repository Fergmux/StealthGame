// Story iif-004 (production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md), ADR-0011
// section 8: one batched ClientCommandReplies per connection per drain, null sink discards.
//   AC: in one drain, sink A has 3 voided commands (slot keys 5, 2, 9), sink B 1 voided command and 1 FHoldStatus, the
//   null sink 2 voided commands; at the drain end the transport spy sees exactly one call for A (entries 2, 5, 9) and
//   one for B (by key), both after the last slot and none inside a slot; the null sink makes 0 calls and buffers 0
//   entries; a connection whose buffer is empty gets no call.
//   Edge cases (story notes): 1,000 voided commands of mixed codes through the real FResolutionQueueCore produce 0
//   sends inside the drain; two consecutive drains give two calls each with only their own entries; a sink released
//   mid-drain (connection closed) discards without a send or crash; ties inside one slot keep append order.
// No PIE: the stub queue and the transport spy of story iif-095 (StubDoubles.h, RpcSpy.h).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Input/RpcSpy.h"
#include "Input/StubDoubles.h"
#include "Server/Core/ReplySink.h"
#include "Server/Testing/StealthQueueTestRig.h"

namespace ReplySinkDrainFlushTestData
{
	FCommandReply Rejected(uint32 Seq, ECommandRejectCode Code = ECommandRejectCode::Busy)
	{
		FClientCommandSeq S;
		S.Seq = Seq;
		return FCommandReply::MakeRejected(S, Code);
	}

	TArray<uint32> SeqsOf(const TArray<FCommandReply>& Replies)
	{
		TArray<uint32> Out;
		for (const FCommandReply& Reply : Replies)
		{
			Out.Add(Reply.Seq.Seq);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputReplySinkDrainFlushTest, "StealthGame.Input.ReplySinkDrainFlush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputReplySinkDrainFlushTest::RunTest(const FString& Parameters)
{
	using namespace ReplySinkDrainFlushTestData;

	// ---- The AC drain (stub queue: slots in the order the queue resolves them) --------------------------------
	FReplySinkTable Table;
	const FReplySinkId SinkA = Table.Allocate();
	const FReplySinkId SinkB = Table.Allocate();
	const FReplySinkId SinkEmpty = Table.Allocate();
	const FReplySinkId Null = FReplySinkTable::NullSink();
	FStubResolutionQueue Queue;
	FReplyTransportSpy Spy([&Queue]() { return Queue.bInSlot; });

	// The producing slots' keys (A: 5, 2, 9; B: 3 voided, 7 hold status; null: 4, 6). Each slot appends its reply.
	struct FSlotPlan
	{
		FReplySinkId Sink;
		uint64 Key;
		bool bHold;
	};
	const FSlotPlan Plan[] = { { SinkA, 5, false }, { SinkA, 2, false }, { Null, 4, false }, { SinkB, 7, true }, { SinkA, 9, false }, { SinkB, 3, false }, { Null, 6, false } };
	for (const FSlotPlan& Slot : Plan)
	{
		FCommandPayload Payload;
		Payload.Arg = static_cast<uint32>(Slot.Key);
		Queue.Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::PawnCommand, FEpochToken(), FMailboxId::ServerLocal(), Payload));
	}
	int32 PlanIndex = 0;
	Queue.Drain([&](const FQueueItem& Item, FQueueKey)
	{
		const FSlotPlan& Slot = Plan[PlanIndex++];
		const FQueueKey Key(1, Slot.Key);
		if (Slot.bHold)
		{
			FHoldStatus Hold;
			Hold.Seq.Seq = static_cast<uint32>(Slot.Key);
			Hold.ActionType = EActionType::Interact;
			Table.Append(Slot.Sink, Key, FCommandReply::MakeHoldStatus(Hold));
		}
		else
		{
			Table.Append(Slot.Sink, Key, Rejected(static_cast<uint32>(Slot.Key)));
		}
	});
	TestEqual(TEXT("nothing is sent inside the drain's slots"), Spy.Calls.Num(), 0);
	TestEqual(TEXT("the null sink buffered 0 entries"), Table.GetBufferedCount(Null), 0);
	TestEqual(TEXT("5 entries buffered for A and B"), Table.GetBufferedCount(), 5);

	const int32 Calls = Table.FlushPass(Spy);
	TestEqual(TEXT("the drain-end flush makes exactly two calls (A and B; none for the null sink or the empty sink)"), Calls, 2);
	TestEqual(TEXT("the spy saw two calls"), Spy.Calls.Num(), 2);
	TestEqual(TEXT("no call was made inside a slot"), Spy.CallsInsideSlot, 0);
	const FReplyTransportSpy::FCall* CallA = Spy.Calls.FindByPredicate([&](const FReplyTransportSpy::FCall& C) { return C.Sink.Index == SinkA.Index; });
	const FReplyTransportSpy::FCall* CallB = Spy.Calls.FindByPredicate([&](const FReplyTransportSpy::FCall& C) { return C.Sink.Index == SinkB.Index; });
	if (TestNotNull(TEXT("one call for A"), CallA))
	{
		TestTrue(TEXT("A's entries are ordered by slot key: 2, 5, 9"), SeqsOf(CallA->Replies) == TArray<uint32>({ 2, 5, 9 }));
	}
	if (TestNotNull(TEXT("one call for B"), CallB))
	{
		TestTrue(TEXT("B's entries are ordered by key: 3 (rejected), 7 (hold status)"), SeqsOf(CallB->Replies) == TArray<uint32>({ 3, 7 })
			&& CallB->Replies.Num() == 2 && CallB->Replies[0].Kind == ECommandReplyKind::Rejected && CallB->Replies[1].Kind == ECommandReplyKind::HoldStatus);
	}
	TestFalse(TEXT("the empty sink got no call"), Spy.Calls.ContainsByPredicate([&](const FReplyTransportSpy::FCall& C) { return C.Sink.Index == SinkEmpty.Index; }));
	TestEqual(TEXT("the buffers are empty after the flush"), Table.GetBufferedCount(), 0);

	// Ties inside one slot keep append order (the IIF reply before the feature's owner result).
	Spy.Calls.Reset();
	Table.Append(SinkA, FQueueKey(2, 1), Rejected(100));
	Table.Append(SinkA, FQueueKey(2, 1), Rejected(101));
	Table.Append(SinkA, FQueueKey(1, 9), Rejected(99));
	Table.FlushPass(Spy);
	TestTrue(TEXT("equal keys keep append order; earlier ticks first"), Spy.Calls.Num() == 1 && SeqsOf(Spy.Calls[0].Replies) == TArray<uint32>({ 99, 100, 101 }));

	// ---- The real queue: 1,000 voided commands, mixed codes, two consecutive drains ----------------------------
	FStealthQueueTestRig Rig;
	FStealthStubResolver Resolver;
	FReplyTransportSpy RigSpy([&Rig]() { return Rig.Queue->CurrentKey().IsSet(); });
	const FReplySinkId Sinks[] = { SinkA, SinkB, Null };
	int32 Resolved = 0;
	Resolver.OnResolve = [&](const FQueueItem& Item, FQueueKey Key)
	{
		const uint32 Tag = FStealthStubResolver::ArgOf(Item);
		Table.Append(Sinks[Tag % 3], Key, Rejected(Tag, static_cast<ECommandRejectCode>(Tag % GCommandRejectCodeCount)));
		++Resolved;
		return EResolveResult::Voided;
	};
	Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Resolver);

	constexpr int32 MailboxCount = 40;
	constexpr int32 PerMailbox = 25;
	for (int32 Index = 0; Index < MailboxCount * PerMailbox; ++Index)
	{
		const FMailboxId Mailbox = FMailboxId::ForParticipant(FParticipantId(static_cast<uint32>(Index % MailboxCount)));
		Rig.Queue->Submit(Mailbox, Rig.MakeItem(EQueueItemType::PawnCommand, static_cast<uint32>(Index), Mailbox));
	}
	Rig.Tick();
	TestEqual(TEXT("the real drain resolved all 1,000 commands"), Resolved, MailboxCount * PerMailbox);
	TestEqual(TEXT("0 sends during the real drain"), RigSpy.Calls.Num(), 0);
	Table.FlushPass(RigSpy);
	TestEqual(TEXT("one call per live connection after the drain (A, B)"), RigSpy.Calls.Num(), 2);
	TestEqual(TEXT("no send inside a real slot"), RigSpy.CallsInsideSlot, 0);
	int32 Delivered = 0;
	bool bOrdered = true;
	for (const FReplyTransportSpy::FCall& Call : RigSpy.Calls)
	{
		Delivered += Call.Replies.Num();
	}
	TestEqual(TEXT("A and B received 2/3 of the replies; the null sink's third was discarded"), Delivered, 667);

	// Second drain: each call carries only this drain's entries.
	RigSpy.Calls.Reset();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FMailboxId Mailbox = FMailboxId::ForParticipant(FParticipantId(static_cast<uint32>(Index)));
		Rig.Queue->Submit(Mailbox, Rig.MakeItem(EQueueItemType::PawnCommand, static_cast<uint32>(2000 + Index), Mailbox));
	}
	Rig.Tick();
	Table.FlushPass(RigSpy);
	TestEqual(TEXT("the second drain: one call each for A and B"), RigSpy.Calls.Num(), 2);
	for (const FReplyTransportSpy::FCall& Call : RigSpy.Calls)
	{
		bOrdered &= Call.Replies.Num() == 2 && Call.Replies[0].Seq.Seq >= 2000 && Call.Replies[1].Seq.Seq >= 2000;
	}
	TestTrue(TEXT("each second-drain call carries only its own drain's two entries"), bOrdered);

	// A connection closed mid-drain: its sink is released inside a slot; nothing is sent to it, no crash.
	RigSpy.Calls.Reset();
	bool bReleased = false;
	Resolver.OnResolve = [&](const FQueueItem& Item, FQueueKey Key)
	{
		Table.Append(SinkB, Key, Rejected(FStealthStubResolver::ArgOf(Item)));
		if (!bReleased)
		{
			Table.Release(SinkB);
			bReleased = true;
		}
		return EResolveResult::Voided;
	};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FMailboxId Mailbox = FMailboxId::ForParticipant(FParticipantId(static_cast<uint32>(Index)));
		Rig.Queue->Submit(Mailbox, Rig.MakeItem(EQueueItemType::PawnCommand, static_cast<uint32>(3000 + Index), Mailbox));
	}
	Rig.Tick();
	TestEqual(TEXT("a released sink is flushed to nobody"), Table.FlushPass(RigSpy), 0);
	TestFalse(TEXT("the released sink no longer resolves"), Table.IsLive(SinkB));
	TestEqual(TEXT("nothing stays buffered for the released sink"), Table.GetBufferedCount(), 0);
	Rig.Queue->ClearRegistrations();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
