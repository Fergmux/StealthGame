// Story asn-007, AC3 / AC-ASN-29 ReversalCanEndTheRound (ASN-R10, Round F2). Team 0 with one Active player A (2 is an
// informant) and team 1 with exactly one Active player (4 an informant). A strikes a braced defender (its Prepare executed
// in an earlier drain, the window still open by key order) which is (a) team-1 player 3 and (b) an NPC: in both runs the
// reversal eliminates A (ReversedAgainst) and Round latches EliminationWin(1) at the reversal's slot key. The trigger reads
// team counts, not who reversed; ResolveVictimKind(A) = Player.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterRoundRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationReversalCanEndTheRoundTest, "StealthGame.Assassination.ReversalCanEndTheRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationReversalCanEndTheRoundTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	for (const bool bNpcDefender : { false, true })
	{
		const TCHAR* Name = bNpcDefender ? TEXT("(b) NPC defender") : TEXT("(a) team-1 player defender");
		FRoundEncounterRig R(TEXT("ReversalEndsRoundWorld"));
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		R.SetInformant(2);
		R.SetInformant(4);
		APawn* A = R.Spawn(FVector::ZeroVector, 1);
		APawn* Defender = bNpcDefender ? R.Spawn(FVector(150.0, 0.0, 0.0)) : R.Spawn(FVector(150.0, 0.0, 0.0), 3);

		R.Drain({ R.Command(Defender, ECommandKind::PrepareBegin) });
		TestTrue(FString::Printf(TEXT("%s: the brace executed"), Name), R.Core->GetRole(Defender) == EStrikeRole::Bracing);
		R.Drain({ R.AttackAt(A, Defender) });

		const FQueueSlotRecord* Slot = R.CommandRecord(0);
		const FRoundTerminalLatch& Latch = R.W.Rig.Core->GetLatch();
		TestTrue(FString::Printf(TEXT("%s: a reversal"), Name), R.Core->GetResolutions().Num() == 1 && R.Core->GetResolutions()[0].Outcome == EAttackOutcome::Reversal);
		TestTrue(FString::Printf(TEXT("%s: A is the one eliminated, ReversedAgainst"), Name), R.Roster.Transitions.Num() == 1 && R.Roster.Transitions[0].Pawn == A
			&& R.Roster.Transitions[0].Reason == ERosterReason::ReversedAgainst && !R.W.IsActive(1));
		TestTrue(FString::Printf(TEXT("%s: OnPawnKilled(A, Defender, Player, Reversal)"), Name), R.Kills.Num() == 1 && R.Kills[0].Victim == A && R.Kills[0].Killer == Defender
			&& R.Kills[0].Kind == EVictimKind::Player && R.Kills[0].Cause == EKillCause::Reversal);
		TestTrue(FString::Printf(TEXT("%s: Round latched EliminationWin(1)"), Name), Latch.IsTerminated() && Latch.GetOutcome() == FRoundTerminalOutcome::Win(1, ERoundEndReason::Elimination));
		TestTrue(FString::Printf(TEXT("%s: at the reversal's slot key"), Name), Slot != nullptr && Latch.GetTerminalKey() == Slot->Key);
		TestTrue(FString::Printf(TEXT("%s: one Reversal row with the slot key"), Name), Slot != nullptr && R.CountLoggedReversal(Slot->Key) == 1);
		TestTrue(FString::Printf(TEXT("%s: the round end binds to A's death write (no defect)"), Name), R.W.Support.Faults.GetRecords().IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: no unexpected Round gateway call"), Name), R.W.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
