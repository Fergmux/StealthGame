// Story asn-007 (production/epics/assassination-defence-reversal/story-007-attack-outcomes-resolved-in-slot.md), AC1 /
// AC-ASN-10 OutcomeInSlot (ASN-R10; ADR-0002 sections 5, 8). The real round queue and Round core (FRoundEncounterRig), 2v2
// with participant 4 an informant, so 3 is team 1's last Active player. Participant 1 kills 3 by PawnRef: the call log
// shows RequestTransition before OnPawnKilled; Round latches EliminationWin(0) inside the Attack's own slot (terminal key =
// the Attack's key); Assassination enqueues no outcome-carrying item (no timer, no mailbox item; only the DeathWrite
// staging write of story asn-013, bound by Round's round-end step); a later Attack in the same drain is Rejected_Latched;
// the victim's strike role is Dead at the slot and GetScheduledDeathServerTime is valid inside the OnPawnKilled fan-out.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterRoundRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationOutcomeInSlotTest, "StealthGame.Assassination.OutcomeInSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationOutcomeInSlotTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	// Arrange: 2v2 (1, 2 team 0; 3, 4 team 1); 4 is an informant.
	FRoundEncounterRig R(TEXT("OutcomeInSlotWorld"));
	R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
	R.SetInformant(4);
	APawn* Attacker = R.Spawn(FVector::ZeroVector, 1);
	APawn* Second = R.Spawn(FVector(0.0, 50.0, 0.0), 2);
	APawn* Victim = R.Spawn(FVector(150.0, 0.0, 0.0), 3);

	// Act: the deciding strike, then a later strike in the same drain.
	R.Drain({ R.AttackAt(Attacker, Victim), R.AttackAt(Second, Victim) });

	// Assert: the call order and the latch inside the slot.
	const FQueueSlotRecord* Deciding = R.CommandRecord(0);
	const FQueueSlotRecord* Later = R.CommandRecord(1);
	TestTrue(TEXT("the Attack executed in its slot"), Deciding != nullptr && Deciding->Outcome == EQueueSlotOutcome::Executed);
	TestTrue(TEXT("RequestTransition, then OnPawnKilled"), R.Calls.Num() == 2 && R.Calls[0] == TEXT("RequestTransition") && R.Calls[1] == TEXT("OnPawnKilled"));
	const FRoundTerminalLatch& Latch = R.W.Rig.Core->GetLatch();
	TestTrue(TEXT("Round latched EliminationWin for the attacker's team"), Latch.IsTerminated() && Latch.GetOutcome() == FRoundTerminalOutcome::Win(0, ERoundEndReason::Elimination));
	TestTrue(TEXT("the latch key is the Attack's slot key (same slot)"), Deciding != nullptr && Latch.GetTerminalKey() == Deciding->Key);
	TestTrue(TEXT("the later Attack is Rejected_Latched"), Later != nullptr && Later->Outcome == EQueueSlotOutcome::Rejected_Latched);
	TestTrue(TEXT("the latched Attack gets only a held NotNow (no lockout, no result)"), R.Core->GetResolutions().Num() == 1 && R.Core->GetReplies().Last().bHeld
		&& R.Defence.Find(Second) == nullptr);
	TestEqual(TEXT("Assassination scheduled no timer"), R.W.Queue->GetPendingTimerCount(), 0);
	TestEqual(TEXT("and submitted no item"), R.W.Queue->GetPendingMailboxItemCount(), 0);
	int32 DeathWrites = 0, Others = 0;
	for (const FStagedWrite& Write : R.W.Queue->GetPendingStagedWrites())
	{
		DeathWrites += Write.Type == EStagedWriteType::DeathWrite ? 1 : 0;
		Others += Write.Type == EStagedWriteType::CounterPose ? 1 : 0;
	}
	TestTrue(TEXT("only the victim's DeathWrite is staged (plus Round's bound round-end step)"), DeathWrites == 1 && Others == 0);
	TestTrue(TEXT("the victim is Dead at the slot"), R.Core->IsDead(Victim) && R.Core->GetRole(Victim) == EStrikeRole::Dead);
	TestTrue(TEXT("GetScheduledDeathServerTime was valid inside the fan-out (= ContactServerTime)"), R.Kills.Num() == 1 && R.Kills[0].ScheduledDeath.IsSet()
		&& FMath::IsNearlyEqual(R.Kills[0].ScheduledDeath.GetValue(), R.Core->GetResolutions()[0].ContactServerTime));
	TestTrue(TEXT("the round-end step binds to that death write (no fallback defect)"), R.W.Support.Faults.GetRecords().IsEmpty());
	TestTrue(TEXT("one KillResolved row with the slot key"), R.CountLoggedKill(Deciding != nullptr ? Deciding->Key : FQueueKey()) == 1);
	TestTrue(TEXT("no unexpected Round gateway call"), R.W.Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
