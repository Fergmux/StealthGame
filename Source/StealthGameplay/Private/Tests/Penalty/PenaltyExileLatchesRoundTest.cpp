// Story nkp-007 (production/epics/npc-kill-penalty-informants/story-007-exile-latches-round-and-double-exile.md), AC1 /
// AC-NKP-08 (PEN-R4 step 4, Edge Cases): GIVEN team 0's only Active player P at k = L - 1 (its teammate an informant) and
// team 1 with Active players, WHEN P's next ungraced NPC kill resolves, THEN Round latches EliminationWin(1) in the
// exile's slot (the exile's Roster event drives Round's elimination check inline); no RespawnScheduled row and no
// respawn call exist for P (Penalty never schedules from its own stack or a Roster subscription; Round, latched, makes no
// OnInformantAwaitingBody call); Round's StrikeLimit transition row is present; the NPC still falls and the plate still
// reads WrongTarget (with bExiled). Contrast: with the teammate Active the round does not latch.
// Real Round core, real queue and strike resolver; the Roster, Crowd, Character Pool and factory are the rig's fakes.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyEncounterRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyExileLatchesRoundTest, "StealthGame.Penalty.ExileLatchesRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyExileLatchesRoundTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyEncounterTest;
	const FParticipantId P(1);

	for (const bool bLastActive : { true, false })
	{
		const TCHAR* Case = bLastActive ? TEXT("last Active player") : TEXT("contrast: a teammate Active");

		// Arrange
		FPenaltyEncounterRig Rig(bLastActive ? TEXT("PenaltyExileLatches") : TEXT("PenaltyExileNoLatch"));
		if (bLastActive)
		{
			Rig.R.SetInformant(2);
		}
		APawn* BodyOfP = Rig.R.Spawn(FVector::ZeroVector, P.Value);
		Rig.ArrangeNpcKills(BodyOfP, Rig.Penalty->GetLatchedStrikeLimit() - 1);
		APawn* Npc = Rig.NpcInFrontOf(BodyOfP);
		TestFalse(FString::Printf(TEXT("%s: not latched before"), Case), Rig.R.W.Rig.Core->GetLatch().IsTerminated());

		// Act
		Rig.R.Drain({ Rig.R.AttackAt(BodyOfP, Npc) });
		const FQueueSlotRecord* Slot = Rig.R.CommandRecord(0);

		// Assert
		const FRoundTerminalLatch& Latch = Rig.R.W.Rig.Core->GetLatch();
		if (bLastActive)
		{
			TestTrue(FString::Printf(TEXT("%s: EliminationWin(1) latched"), Case), Latch.IsTerminated() && Latch.GetOutcome() == FRoundTerminalOutcome::Win(1, ERoundEndReason::Elimination));
			TestTrue(FString::Printf(TEXT("%s: in the exile's slot"), Case), Slot != nullptr && Latch.GetTerminalKey() == Slot->Key);
		}
		else
		{
			TestFalse(FString::Printf(TEXT("%s: no latch"), Case), Latch.IsTerminated());
		}
		TestEqual(FString::Printf(TEXT("%s: the exile ran once"), Case), Rig.Bodies.Count(TEXT("RequestTransition")), 1);
		TestEqual(FString::Printf(TEXT("%s: Round's StrikeLimit transition row"), Case), Rig.CountTransitionRows(P, ERosterReason::StrikeLimit), 1);
		TestEqual(FString::Printf(TEXT("%s: no RespawnScheduled row"), Case), Rig.CountRows(ERoundLogType::RespawnScheduled), 0);
		TestEqual(FString::Printf(TEXT("%s: no respawn scheduled from the exile's stack"), Case), Rig.Respawns.Calls.Num(), 0);
		TestTrue(FString::Printf(TEXT("%s: the NPC still falls (Dead, OnPawnKilled)"), Case), Rig.R.Core->IsDead(Npc)
			&& Rig.R.Kills.ContainsByPredicate([Npc](const FKill& K) { return K.Victim == Npc; }));
		const TArray<FAttackResolution>& Res = Rig.R.Core->GetResolutions();
		TestTrue(FString::Printf(TEXT("%s: the plate reads WrongTarget with bExiled"), Case), Res.Num() > 0 && Res.Last().Victim == Npc
			&& Res.Last().StrikeResult.Result == EStrikeResult::WrongTarget && Res.Last().StrikeResult.bExiled);
		TestTrue(FString::Printf(TEXT("%s: one StrikeCounted row in the exile's slot"), Case), Slot != nullptr && Rig.CountStrikeCounted(Slot->Key) == 1);
		TestEqual(FString::Printf(TEXT("%s: no Penalty fault"), Case), Rig.PenaltyFaults.GetRecords().Num(), 0);
	}
	return true;
}

#endif
