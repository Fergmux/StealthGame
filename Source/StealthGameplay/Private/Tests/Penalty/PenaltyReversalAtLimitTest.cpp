// Story nkp-002 (production/epics/npc-kill-penalty-informants/story-002-strike-rule-and-release-grace.md), AC3 / AC-NKP-32:
// L = 3 and P at k = 2; P's Attack is reversed (by a braced rival and, separately, by a bracing NPC with no participant):
// k stays 2, the tally is unchanged (Reversed is not WrongTarget), no StrikeLimit is due (bExiled false, no exile outcome).
// Edge: an assassination of P at k = 2 also leaves P's k unchanged and charges the rival nothing. The respawn half (one
// respawn scheduled after Round's OnInformantAwaitingBody(P, ReversedAgainst, t)) is story nkp-009's provider call.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyReversalAtLimitTest, "StealthGame.Penalty.ReversalAtLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyReversalAtLimitTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;

	const FPenaltyPawnKey PawnOfP = Pawn(10);
	for (const bool bNpcDefender : { false, true })
	{
		const TCHAR* Case = bNpcDefender ? TEXT("reversed by a bracing NPC") : TEXT("reversed by a braced rival");

		// Arrange: L latched, P one strike short of it.
		FRig Rig;
		const int32 Limit = Rig.Core->GetLatchedStrikeLimit();
		for (int32 Index = 0; Index < Limit - 1; ++Index)
		{
			Rig.NpcKill(P, 900 + Index);
		}
		const int32 StrikesBefore = Rig.Core->GetStrikeCount(P);
		const int32 TallyBefore = Rig.Core->GetWrongTargetTally(P);
		TestEqual(FString::Printf(TEXT("%s: arranged k = L - 1"), Case), StrikesBefore, Limit - 1);

		// Act: the reversal kills P (the attacker); the killer is the defender.
		const FParticipantId Defender = bNpcDefender ? FParticipantId() : Q;
		EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::Strike;
		const FStrikeTally Tally = Rig.Attack(P, FPenaltyKill{ PawnOfP, Defender, EVictimKind::Player, EKillCause::Reversal }, EStrikeResult::Reversed, &Outcome);

		// Assert
		TestEqual(FString::Printf(TEXT("%s: k unchanged"), Case), static_cast<int32>(Rig.Core->GetStrikeCount(P)), StrikesBefore);
		TestEqual(FString::Printf(TEXT("%s: tally unchanged"), Case), static_cast<int32>(Tally.TallyAfter), TallyBefore);
		TestFalse(FString::Printf(TEXT("%s: no StrikeLimit (bExiled false)"), Case), Tally.bExiled);
		TestTrue(FString::Printf(TEXT("%s: no strike for anyone"), Case), Outcome == EPenaltyKillOutcome::NoStrike && Rig.Core->GetStrikeCount(Q) == 0);
		TestEqual(FString::Printf(TEXT("%s: no fault"), Case), Rig.Faults.GetRecords().Num(), 0);
	}

	// Edge: an assassination of P at k = L - 1 leaves P's k unchanged and gives the rival Q no strike (RivalEliminated).
	{
		FRig Rig;
		const int32 Limit = Rig.Core->GetLatchedStrikeLimit();
		for (int32 Index = 0; Index < Limit - 1; ++Index)
		{
			Rig.NpcKill(P, 900 + Index);
		}
		const FStrikeTally Tally = Rig.Attack(Q, FPenaltyKill{ PawnOfP, Q, EVictimKind::Player, EKillCause::Strike }, EStrikeResult::RivalEliminated);
		TestEqual(TEXT("assassinated at k = L - 1: P's k unchanged"), static_cast<int32>(Rig.Core->GetStrikeCount(P)), Limit - 1);
		TestEqual(TEXT("the rival's kill of a player is no strike"), static_cast<int32>(Rig.Core->GetStrikeCount(Q)), 0);
		TestEqual(TEXT("RivalEliminated does not tick the rival's tally"), static_cast<int32>(Tally.TallyAfter), 0);
	}
	return true;
}

#endif
