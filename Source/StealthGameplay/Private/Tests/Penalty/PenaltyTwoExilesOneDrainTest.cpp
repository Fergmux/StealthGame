// Story nkp-007 (production/epics/npc-kill-penalty-informants/story-007-exile-latches-round-and-double-exile.md), AC2 /
// AC-NKP-41 (Edge Cases): GIVEN two exiling Attacks in one drain, one per team, each by its team's last Active player at
// k = L - 1, WHEN the drain resolves (once per key order), THEN only the first in key order exiles and latches
// EliminationWin for the other team; the second Attack is Rejected_Latched with no strike, no tally query, no exile and no
// OnPawnKilled; StrikeCounted rows exist for the first Attack only. Real Round core, queue and strike resolver.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyEncounterRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyTwoExilesOneDrainTest, "StealthGame.Penalty.TwoExilesOneDrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyTwoExilesOneDrainTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyEncounterTest;
	const FParticipantId P0(1); // team 0's last Active player
	const FParticipantId P1(3); // team 1's last Active player

	for (const bool bTeam0First : { true, false })
	{
		const TCHAR* Case = bTeam0First ? TEXT("team 0 first") : TEXT("team 1 first");

		// Arrange: each team's teammate an informant; both last Active players at k = L - 1, each with an NPC ahead.
		FPenaltyEncounterRig Rig(bTeam0First ? TEXT("PenaltyTwoExilesA") : TEXT("PenaltyTwoExilesB"));
		Rig.R.SetInformant(2);
		Rig.R.SetInformant(4);
		APawn* A0 = Rig.R.Spawn(FVector::ZeroVector, P0.Value);
		APawn* A1 = Rig.R.Spawn(FVector(0.0, 2000.0, 0.0), P1.Value);
		const int32 Limit = Rig.Penalty->GetLatchedStrikeLimit();
		Rig.ArrangeNpcKills(A0, Limit - 1);
		Rig.ArrangeNpcKills(A1, Limit - 1);
		APawn* Npc0 = Rig.NpcInFrontOf(A0);
		APawn* Npc1 = Rig.NpcInFrontOf(A1);
		const int32 TalliesBefore = Rig.Tallies.Num();
		const int32 KillsBefore = Rig.R.Kills.Num();
		const int32 Tally0 = Rig.Penalty->GetWrongTargetTally(P0);
		const int32 Tally1 = Rig.Penalty->GetWrongTargetTally(P1);

		// Act: both exiling Attacks in one drain.
		const FQueueItem First = bTeam0First ? Rig.R.AttackAt(A0, Npc0) : Rig.R.AttackAt(A1, Npc1);
		const FQueueItem Second = bTeam0First ? Rig.R.AttackAt(A1, Npc1) : Rig.R.AttackAt(A0, Npc0);
		Rig.R.Drain({ First, Second });
		const FQueueSlotRecord* FirstSlot = Rig.R.CommandRecord(0);
		const FQueueSlotRecord* SecondSlot = Rig.R.CommandRecord(1);

		// Assert
		const FParticipantId Winner = bTeam0First ? P0 : P1;
		const FParticipantId Loser = bTeam0First ? P1 : P0;
		const uint8 WinningTeam = bTeam0First ? 1 : 0; // the first exile empties its own team: the other team wins
		const FRoundTerminalLatch& Latch = Rig.R.W.Rig.Core->GetLatch();
		TestTrue(FString::Printf(TEXT("%s: EliminationWin for the other team"), Case), Latch.IsTerminated()
			&& Latch.GetOutcome() == FRoundTerminalOutcome::Win(WinningTeam, ERoundEndReason::Elimination));
		TestTrue(FString::Printf(TEXT("%s: latched in the first Attack's slot"), Case), FirstSlot != nullptr && Latch.GetTerminalKey() == FirstSlot->Key);
		TestTrue(FString::Printf(TEXT("%s: the second Attack is Rejected_Latched"), Case), SecondSlot != nullptr && SecondSlot->Outcome == EQueueSlotOutcome::Rejected_Latched);
		TestTrue(FString::Printf(TEXT("%s: one exile, the first attacker's"), Case), Rig.Bodies.Count(TEXT("RequestTransition")) == 1 && Rig.Bodies.Calls[0].Participant == Winner);
		TestEqual(FString::Printf(TEXT("%s: one OnPawnKilled in the drain"), Case), Rig.R.Kills.Num(), KillsBefore + 1);
		TestEqual(FString::Printf(TEXT("%s: one tally query in the drain (none for the rejected Attack)"), Case), Rig.Tallies.Num(), TalliesBefore + 1);
		TestEqual(FString::Printf(TEXT("%s: the second attacker keeps k = L - 1"), Case), static_cast<int32>(Rig.Penalty->GetStrikeCount(Loser)), Limit - 1);
		TestEqual(FString::Printf(TEXT("%s: the second attacker's tally is unchanged"), Case), static_cast<int32>(Rig.Penalty->GetWrongTargetTally(Loser)),
			bTeam0First ? Tally1 : Tally0);
		TestEqual(FString::Printf(TEXT("%s: the first attacker reached L"), Case), static_cast<int32>(Rig.Penalty->GetStrikeCount(Winner)), Limit);
		TestTrue(FString::Printf(TEXT("%s: StrikeCounted for the first Attack only"), Case), FirstSlot != nullptr && SecondSlot != nullptr
			&& Rig.CountStrikeCounted(FirstSlot->Key) == 1 && Rig.CountStrikeCounted(SecondSlot->Key) == 0);
		TestFalse(FString::Printf(TEXT("%s: the second NPC lives"), Case), Rig.R.Core->IsDead(bTeam0First ? Npc1 : Npc0));
		TestEqual(FString::Printf(TEXT("%s: no Penalty fault"), Case), Rig.PenaltyFaults.GetRecords().Num(), 0);
	}
	return true;
}

#endif
