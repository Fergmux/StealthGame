// Story rmf-010 (production/epics/round-match-framework/story-010-stale-epoch-item.md): GDD AC 13b.
// An item stamped with the previous RoundEpoch, already in a mailbox before ResetAll, is Rejected_StaleEpoch when
// drained: its resolver is not called, nothing changes, a Voided(Rejected_StaleEpoch) entry is logged. The epoch
// check runs before the connection-generation check; a current-epoch item in the same drain resolves normally.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthQueueTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundStaleEpochItemTest, "StealthGame.Round.StaleEpochItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundStaleEpochItemTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	// Arrange: items submitted (and a timer scheduled) under epoch N.
	FStealthQueueTestRig Rig;
	FStealthStubResolver Stub;
	FStealthStubResolver Grace;
	Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Stub);
	Rig.Queue->RegisterTimerResolver(ETimerKind::GraceExpiry, &Grace);
	const FMailboxId A = FMailboxId::ForParticipant(FParticipantId(FixtureParticipantIds[0]));
	Rig.Generations.Generations.Add(A.Value, 1);
	Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, 1, A));
	Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, 2, A), 1);  // stale epoch AND, below, stale connection
	Rig.Queue->ScheduleAt(Rig.Clock.GetServerTimeSeconds(), Rig.MakeTimer(ETimerKind::GraceExpiry, 3, A.GetParticipant()));

	// Act: ResetAll increments the epoch (and the participant reconnects: a newer generation); then items under
	// the new epoch arrive, and the queue drains.
	Rig.Epoch.Current.RoundEpoch += 1;
	Rig.Generations.Generations.Add(A.Value, 2);
	Rig.Queue->Submit(A, Rig.MakeItem(EQueueItemType::PawnCommand, 4, A), 2);
	Rig.Tick();

	// Assert.
	TestTrue(TEXT("only the current-epoch item reached its resolver"), Stub.Resolved.Num() == 1 && Stub.Resolved[0].Arg == 4);
	TestEqual(TEXT("the stale grace timer never reached its resolver"), Grace.Resolved.Num(), 0);
	const TArray<FQueueSlotRecord>& Records = Rig.Queue->GetLastDrainRecords();
	if (TestEqual(TEXT("four slots"), Records.Num(), 4))
	{
		// Group 1 (the due timer) first, then the mailbox FIFO.
		TestTrue(TEXT("the stale timer is Rejected_StaleEpoch"), Records[0].Type == EQueueItemType::TimerFired && Records[0].Outcome == EQueueSlotOutcome::Rejected_StaleEpoch);
		TestTrue(TEXT("the stale command is Rejected_StaleEpoch"), Records[1].Outcome == EQueueSlotOutcome::Rejected_StaleEpoch);
		TestTrue(TEXT("a stale-epoch item from a stale connection reports StaleEpoch (epoch checked first)"), Records[2].Outcome == EQueueSlotOutcome::Rejected_StaleEpoch);
		TestTrue(TEXT("the current-epoch item executed"), Records[3].Outcome == EQueueSlotOutcome::Executed);
	}
	const TArray<FRoundLogEntry> Voided = Rig.Log.Entries.FilterByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::Voided; });
	TestEqual(TEXT("three Voided entries"), Voided.Num(), 3);
	for (const FRoundLogEntry& Entry : Voided)
	{
		TestTrue(TEXT("each carries Rejected_StaleEpoch"), Entry.Code == static_cast<uint16>(EVoidReason::Rejected_StaleEpoch));
	}
	TestEqual(TEXT("no stale-connection drop was reported"), Rig.Log.CountOfType(ERoundLogType::Dropped_StaleConnection), 0);

	const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({});
	TestTrue(Teardown.Describe(), Teardown.IsClean());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
