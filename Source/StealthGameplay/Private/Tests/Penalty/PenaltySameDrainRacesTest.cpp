// Story nkp-005 (production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md), AC3 /
// AC-NKP-34 (GDD Edge Cases): P's L-th NPC kill and a rival's assassination of P in ONE drain, in both key orders, on the
// real round queue, the real strike resolver and the real Round core with Penalty wired as the composition root wires it
// (PenaltyEncounterRig.h):
//   kill first:          the exile (the body released in the slot), then the rival's strike on that body resolves
//                        VictimKind = Npc: WrongTarget, the rival's tally + 1 and NO strike (inside the 2.0 s inclusive
//                        release grace: same drain, same tick);
//   assassination first: P becomes Informant (Assassinated); P's queued Attack is voided NotNow at its slot, the NPC lives,
//                        P's tally and strikes are unchanged and no exile runs.
// Penalty adds no ordering logic: the queue's key order decides (ADR-0002 section 4).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyEncounterRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltySameDrainRacesTest, "StealthGame.Penalty.SameDrainRaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltySameDrainRacesTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyEncounterTest;
	const FParticipantId P(1);
	const FParticipantId Rival(3);

	for (const bool bKillFirst : { true, false })
	{
		const TCHAR* Case = bKillFirst ? TEXT("kill first") : TEXT("assassination first");

		// Arrange: P (team 0, a teammate still Active) at k = L - 1; a rival (team 1) behind P facing it; an NPC ahead of P.
		FPenaltyEncounterRig Rig(bKillFirst ? TEXT("PenaltyRaceKillFirst") : TEXT("PenaltyRaceAssassinationFirst"));
		APawn* BodyOfP = Rig.R.Spawn(FVector::ZeroVector, P.Value);
		APawn* RivalPawn = Rig.R.Spawn(FVector(-150.0, 0.0, 0.0), Rival.Value);
		const int32 Limit = Rig.Penalty->GetLatchedStrikeLimit();
		Rig.ArrangeNpcKills(BodyOfP, Limit - 1);
		const int32 TallyBefore = Rig.Penalty->GetWrongTargetTally(P);
		TestEqual(FString::Printf(TEXT("%s: arranged k = L - 1"), Case), static_cast<int32>(Rig.Penalty->GetStrikeCount(P)), Limit - 1);
		APawn* Npc = Rig.NpcInFrontOf(BodyOfP);
		const int32 ResolutionsBefore = Rig.R.Core->GetResolutions().Num();
		Rig.Tallies.Reset();

		// Act: both Attacks in one drain, in the case's key order.
		const FQueueItem Kill = Rig.R.AttackAt(BodyOfP, Npc);
		const FQueueItem Assassination = Rig.R.AttackAt(RivalPawn, BodyOfP);
		Rig.R.Drain(bKillFirst ? TArray<FQueueItem>{ Kill, Assassination } : TArray<FQueueItem>{ Assassination, Kill });
		const FQueueSlotRecord* First = Rig.R.CommandRecord(0);
		const FQueueSlotRecord* Second = Rig.R.CommandRecord(1);
		TestTrue(FString::Printf(TEXT("%s: one drain, two keyed slots"), Case), First != nullptr && Second != nullptr && First->Key.ServerTickIndex == Second->Key.ServerTickIndex);

		if (bKillFirst)
		{
			TestEqual(FString::Printf(TEXT("%s: the exile ran in the kill's slot"), Case), Rig.Bodies.Count(TEXT("RequestTransition")), 1);
			TestTrue(FString::Printf(TEXT("%s: the transition is in the first slot"), Case), Rig.Bodies.Calls.Num() > 0 && First != nullptr && Rig.Bodies.Calls[0].SlotKey == First->Key);
			TestEqual(FString::Printf(TEXT("%s: P is Informant (StrikeLimit)"), Case), Rig.CountTransitionRows(P, ERosterReason::StrikeLimit), 1);
			const TArray<FAttackResolution>& Res = Rig.R.Core->GetResolutions();
			const FAttackResolution* RivalStrike = Res.FindByPredicate([RivalPawn](const FAttackResolution& X) { return X.Attacker == RivalPawn; });
			TestTrue(FString::Printf(TEXT("%s: the rival's strike kills the released body as an NPC: WrongTarget"), Case), RivalStrike != nullptr && RivalStrike->Victim == BodyOfP
				&& RivalStrike->FallenKind == EVictimKind::Npc && RivalStrike->StrikeResult.Result == EStrikeResult::WrongTarget);
			TestTrue(FString::Printf(TEXT("%s: the rival's kill is graced (no strike outcome)"), Case), Rig.Outcomes.Num() >= 2 && Rig.Outcomes.Last() == EPenaltyKillOutcome::Graced);
			TestEqual(FString::Printf(TEXT("%s: the rival gets no strike"), Case), static_cast<int32>(Rig.Penalty->GetStrikeCount(Rival)), 0);
			TestEqual(FString::Printf(TEXT("%s: the rival's tally + 1"), Case), static_cast<int32>(Rig.Penalty->GetWrongTargetTally(Rival)), 1);
			const TPair<const APawn*, FStrikeTally>* RivalTally = Rig.Tallies.FindByPredicate([RivalPawn](const TPair<const APawn*, FStrikeTally>& T) { return T.Key == RivalPawn; });
			TestTrue(FString::Printf(TEXT("%s: the rival's plate carries TallyAfter 1, not bExiled"), Case), RivalTally != nullptr && RivalTally->Value.TallyAfter == 1 && !RivalTally->Value.bExiled);
			const TPair<const APawn*, FStrikeTally>* PTally = Rig.Tallies.FindByPredicate([BodyOfP](const TPair<const APawn*, FStrikeTally>& T) { return T.Key == BodyOfP; });
			TestTrue(FString::Printf(TEXT("%s: P's own plate carries bExiled"), Case), PTally != nullptr && PTally->Value.bExiled);
			const TOptional<double> Released = Rig.Penalty->FindReleaseTime(KeyOf(BodyOfP));
			TestTrue(FString::Printf(TEXT("%s: the rival's slot is inside the inclusive grace"), Case), Released.IsSet()
				&& Rig.R.W.Rig.Clock.GetServerTimeSeconds() - Released.GetValue() <= static_cast<double>(Rig.Penalty->GetLatchedReleaseGraceSec()));
		}
		else
		{
			TestTrue(FString::Printf(TEXT("%s: the rival assassinates P (Player)"), Case), Rig.R.Kills.Num() > 0 && Rig.R.Kills.Last().Victim == BodyOfP
				&& Rig.R.Kills.Last().Kind == EVictimKind::Player);
			TestTrue(FString::Printf(TEXT("%s: P's Attack is voided NotNow"), Case), Second != nullptr && Second->Outcome == EQueueSlotOutcome::Voided
				&& Rig.R.Core->GetReplies().Num() > 0 && Rig.R.Core->GetReplies().Last().Code == ECommandRejectCode::NotNow);
			TestFalse(FString::Printf(TEXT("%s: the NPC lives"), Case), Rig.R.Core->IsDead(Npc));
			TestEqual(FString::Printf(TEXT("%s: only the assassination resolved"), Case), Rig.R.Core->GetResolutions().Num(), ResolutionsBefore + 1);
			TestEqual(FString::Printf(TEXT("%s: no exile"), Case), Rig.Bodies.Count(TEXT("RequestTransition")), 0);
			TestEqual(FString::Printf(TEXT("%s: P's strikes unchanged"), Case), static_cast<int32>(Rig.Penalty->GetStrikeCount(P)), Limit - 1);
			TestEqual(FString::Printf(TEXT("%s: P's tally unchanged"), Case), static_cast<int32>(Rig.Penalty->GetWrongTargetTally(P)), TallyBefore);
		}
		TestEqual(FString::Printf(TEXT("%s: no Penalty fault"), Case), Rig.PenaltyFaults.GetRecords().Num(), 0);
	}
	return true;
}

#endif
