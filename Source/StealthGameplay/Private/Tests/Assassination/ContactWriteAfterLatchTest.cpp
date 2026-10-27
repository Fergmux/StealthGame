// Story asn-013 AC2 (AC-ASN-43, ADR-0002 section 6, ADR-0018 section 3.2): StealthGame.Assassination.ContactWriteAfterLatch.
// On the real round queue (2v2, every player Active), one drain resolves a strike at key 3 (participant 1 kills an NPC)
// and a final completion at key 7 (participant 2, team 0: ObjectiveWin, the latch at key 7 and the round-end step inline).
// The strike's death write still runs at ContactServerTime - StrikeNetLeadSec (+- 1 tick) with DeathServerTime equal to
// the strike's ContactServerTime, although the round already latched and ended. Edge: a Reset between the slot and the
// write (the round epoch moves on) drops the pending write by epoch.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterRoundRig.h"

namespace ContactWriteAfterLatchTestData
{
	/** The completion item's tag (never 0 or 1: the pose-end hold timers' ActionEpoch values reach the same stub). */
	constexpr uint32 CompletionTag = 707;
	constexpr int32 MaxTicks = 40;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationContactWriteAfterLatchTest, "StealthGame.Assassination.ContactWriteAfterLatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationContactWriteAfterLatchTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace ContactWriteAfterLatchTestData;
	const double Lead = FEncounterParams::Defaults().Server.StrikeNetLeadSec;
	const double Tick = RoundTestFixtures::RigFrameSec;

	for (const bool bResetBeforeWrite : { false, true })
	{
		const TCHAR* Name = bResetBeforeWrite ? TEXT("reset before the write") : TEXT("latched before the write");
		// Arrange.
		FRoundEncounterRig R(TEXT("ContactWriteAfterLatchWorld"), /*bProductionWrites*/ true);
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		APawn* A = R.Spawn(FVector::ZeroVector, 1);
		APawn* Npc = R.Spawn(FVector(150.0, 0.0, 0.0));

		// Act: keys 1-2 fillers, 3 the strike, 4-6 fillers, 7 the final completion, all in one drain.
		R.Drain({ R.Filler(), R.Filler(), R.AttackAt(A, Npc), R.Filler(), R.Filler(), R.Filler(), R.W.Completion(CompletionTag, 2, 0) });
		const FQueueSlotRecord* Strike = R.CommandRecord(0);
		const FRoundTerminalLatch& Latch = R.W.Rig.Core->GetLatch();
		TestTrue(FString::Printf(TEXT("%s: the strike executed at Seq 3"), Name), Strike != nullptr && Strike->Key.Seq == 3 && Strike->Outcome == EQueueSlotOutcome::Executed);
		TestTrue(FString::Printf(TEXT("%s: the kill"), Name), R.Core->GetResolutions().Num() == 1 && R.Core->GetResolutions()[0].Outcome == EAttackOutcome::Kill);
		TestTrue(FString::Printf(TEXT("%s: the completion latched ObjectiveWin at Seq 7"), Name), Latch.IsTerminated() && Latch.GetTerminalKey().Seq == 7
			&& Latch.GetOutcome().EndReason == ERoundEndReason::ObjectiveWin);
		const double Contact = R.Core->GetResolutions().Num() == 1 ? R.Core->GetResolutions()[0].ContactServerTime : 0.0;
		if (bResetBeforeWrite)
		{
			++R.W.Support.Epoch.Current.RoundEpoch; // Round's Reset: the round epoch moves on before the write is due
		}
		TArray<FRecordingPawnState::FWrite> Deaths;
		for (int32 Index = 0; Index < MaxTicks && Deaths.IsEmpty(); ++Index)
		{
			R.Tick();
			Deaths = R.PawnState.Writes.FilterByPredicate([](const FRecordingPawnState::FWrite& W) { return W.bDeath; });
		}

		// Assert.
		if (bResetBeforeWrite)
		{
			TestEqual(TEXT("reset: the pending death write is dropped by epoch"), Deaths.Num(), 0);
			TestEqual(TEXT("reset: the applier never ran"), R.Core->GetAppliedWrites().Num(), 0);
			continue;
		}
		if (!TestEqual(FString::Printf(TEXT("%s: one death write"), Name), Deaths.Num(), 1))
		{
			continue;
		}
		AddInfo(FString::Printf(TEXT("AFTERLATCH contact %.4f, write applied %.4f (T_dw %.4f), phase %d"), Contact, Deaths[0].ServerTime, Contact - Lead,
			static_cast<int32>(R.W.Rig.Core->GetPhase())));
		TestTrue(TEXT("on the NPC victim"), Deaths[0].Pawn == Npc);
		TestTrue(TEXT("DeathServerTime = the strike's ContactServerTime"), Deaths[0].Death.DeathServerTime == Contact);
		TestTrue(TEXT("written at ContactServerTime - 0.15 s +- 1 tick"), FMath::Abs(Deaths[0].ServerTime - (Contact - Lead)) <= Tick + 1e-9);
		TestTrue(TEXT("the round had latched (and ended) before the write"), R.W.Rig.Core->GetPhase() == ERoundPhase::Ended);
		TestTrue(TEXT("no queue fault"), R.W.Support.Faults.GetRecords().IsEmpty());
	}
	return true;
}

#endif
