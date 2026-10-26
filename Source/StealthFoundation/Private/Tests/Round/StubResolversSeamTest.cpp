// Story rmf-004 (production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md):
// GDD test seam 4. A stub resolver can be registered for every queue item type and every TimerFired kind, and
// records exactly the keys of its own type in resolve order. A type with no resolver is reported (fault and
// slot outcome), never silently dropped; a second registration for a type is rejected ("one resolver per type").

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthQueueTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundStubResolversSeamTest, "StealthGame.Round.StubResolversSeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundStubResolversSeamTest::RunTest(const FString& Parameters)
{
	// Arrange: one stub per non-timer item type and one per timer kind.
	FStealthQueueTestRig Rig;
	FStealthStubResolver TypeStubs[GQueueItemTypeCount];
	FStealthStubResolver TimerStubs[GTimerKindCount];
	for (int32 Type = 0; Type < GQueueItemTypeCount; ++Type)
	{
		if (static_cast<EQueueItemType>(Type) != EQueueItemType::TimerFired)
		{
			TestTrue(FString::Printf(TEXT("register item type %d"), Type), Rig.Queue->RegisterResolver(static_cast<EQueueItemType>(Type), &TypeStubs[Type]));
		}
	}
	for (int32 Kind = 0; Kind < GTimerKindCount; ++Kind)
	{
		TestTrue(FString::Printf(TEXT("register timer kind %d"), Kind), Rig.Queue->RegisterTimerResolver(static_cast<ETimerKind>(Kind), &TimerStubs[Kind]));
	}

	// Act: two drains; every type twice (multiple types in one drain), timers due now.
	for (int32 Round = 0; Round < 2; ++Round)
	{
		for (int32 Type = 0; Type < GQueueItemTypeCount; ++Type)
		{
			if (static_cast<EQueueItemType>(Type) != EQueueItemType::TimerFired)
			{
				Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(static_cast<EQueueItemType>(Type), static_cast<uint32>(Type)));
			}
		}
		for (int32 Kind = 0; Kind < GTimerKindCount; ++Kind)
		{
			Rig.Queue->ScheduleAt(Rig.Clock.GetServerTimeSeconds(), Rig.MakeTimer(static_cast<ETimerKind>(Kind), static_cast<uint32>(Kind)));
		}
		Rig.Tick();
	}

	// Assert: each stub saw exactly two items of its own type, with keys in increasing (resolve) order.
	for (int32 Type = 0; Type < GQueueItemTypeCount; ++Type)
	{
		if (static_cast<EQueueItemType>(Type) == EQueueItemType::TimerFired)
		{
			continue;
		}
		const FStealthStubResolver& Stub = TypeStubs[Type];
		if (TestEqual(FString::Printf(TEXT("type %d stub resolved two items"), Type), Stub.Resolved.Num(), 2))
		{
			TestTrue(FString::Printf(TEXT("type %d stub saw only its type"), Type),
				Stub.Resolved[0].Type == static_cast<EQueueItemType>(Type) && Stub.Resolved[1].Type == static_cast<EQueueItemType>(Type));
			TestTrue(FString::Printf(TEXT("type %d keys recorded in resolve order"), Type), Stub.Resolved[0].Key < Stub.Resolved[1].Key);
		}
	}
	for (int32 Kind = 0; Kind < GTimerKindCount; ++Kind)
	{
		const FStealthStubResolver& Stub = TimerStubs[Kind];
		if (TestEqual(FString::Printf(TEXT("timer kind %d stub resolved two items"), Kind), Stub.Resolved.Num(), 2))
		{
			TestTrue(FString::Printf(TEXT("timer kind %d stub saw only its kind"), Kind),
				Stub.Resolved[0].Arg == static_cast<uint32>(Kind) && Stub.Resolved[1].Arg == static_cast<uint32>(Kind));
			TestTrue(FString::Printf(TEXT("timer kind %d keys in resolve order"), Kind), Stub.Resolved[0].Key < Stub.Resolved[1].Key);
		}
	}
	// Every key in a drain is unique and the records come out in key order.
	const TArray<FQueueSlotRecord>& Records = Rig.Queue->GetLastDrainRecords();
	TestEqual(TEXT("one slot per item in the last drain"), Records.Num(), (GQueueItemTypeCount - 1) + GTimerKindCount);
	for (int32 Index = 1; Index < Records.Num(); ++Index)
	{
		TestTrue(TEXT("slot records strictly increasing"), Records[Index - 1].Key < Records[Index].Key);
	}

	// Edge: a second registration for a type is rejected and reported; the first stays.
	FStealthStubResolver Intruder;
	TestFalse(TEXT("second resolver for PawnCommand rejected"), Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Intruder));
	TestFalse(TEXT("second resolver for a timer kind rejected"), Rig.Queue->RegisterTimerResolver(ETimerKind::GraceExpiry, &Intruder));

	// Edge: an item type with no resolver is reported, not silently dropped.
	Rig.Queue->UnregisterResolver(EQueueItemType::PawnLost);
	Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnLost, 1));
	Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 1));
	Rig.Tick();
	const TArray<FQueueSlotRecord>& Last = Rig.Queue->GetLastDrainRecords();
	if (TestEqual(TEXT("both items got a slot"), Last.Num(), 2))
	{
		TestTrue(TEXT("the unregistered type's slot reports NoResolver"), Last[0].Outcome == EQueueSlotOutcome::NoResolver);
		TestTrue(TEXT("the registered item still resolved"), Last[1].Outcome == EQueueSlotOutcome::Executed);
	}
	TestEqual(TEXT("the intruder never resolved anything"), Intruder.Resolved.Num(), 0);
	TestEqual(TEXT("PawnLost stub kept its two earlier items only"), TypeStubs[static_cast<int32>(EQueueItemType::PawnLost)].Resolved.Num(), 2);

	// Edge (code review): a submit to an unset mailbox is refused and reported, never queued.
	TestFalse(TEXT("an unset mailbox is refused"), Rig.Queue->Submit(FMailboxId(), Rig.MakeItem(EQueueItemType::PawnCommand, 1)).IsSet());
	TestEqual(TEXT("nothing was queued"), Rig.Queue->GetPendingMailboxItemCount(), 0);

	// After unregistering, the type accepts a new registration (replacement goes through Unregister).
	TestTrue(TEXT("register again after unregister"), Rig.Queue->RegisterResolver(EQueueItemType::PawnLost, &Intruder));

	const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({ { EStealthFault::QueueDuplicateRegistration }, { EStealthFault::QueueNoResolver }, { EStealthFault::QueueMisuse } });
	TestTrue(Teardown.Describe(), Teardown.IsClean());
	TestEqual(TEXT("two duplicate registrations, one missing resolver and one unset mailbox reported"), Rig.Faults.GetRecords().Num(), 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
