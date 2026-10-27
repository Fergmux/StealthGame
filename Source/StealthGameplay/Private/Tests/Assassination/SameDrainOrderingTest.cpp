// Story asn-009, AC2 / AC-ASN-19 SameDrainOrdering (F4, ASN-R13; ADR-0002 sections 3, 8). The real round queue and Round
// core, 2v2 (1, 2 team 0; 3, 4 team 1). Two items in one drain at the drain's 3rd and 7th Seq of one tick (fillers keep the
// offsets; equal-tick order is Round's stamping, never an Assassination rule):
//   (a) A -> B and B -> A: A kills B, B's Attack is Voided(NotNow): no lockout, no result;
//   (b) A1 -> B and A2 -> B by PawnRef: A1 kills B, A2 is NoContact (B is Dead from A1's slot: a terminal whiff);
//   (c) a final completion, then a strike: the strike is Rejected_Latched with no lockout and no FStrikeResult, and its
//       owner reply is a held NotNow;
//   (d) a braced A strikes a braced B: A's brace is superseded and A is reversed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterRoundRig.h"

namespace SameDrainOrderingTestData
{
	using namespace StealthEncounterTest;

	/** [filler, filler, First, filler, filler, filler, Second]: First at the drain's Seq + 2, Second at + 6. */
	inline TArray<FQueueItem> Interleave(FRoundEncounterRig& R, const FQueueItem& First, const FQueueItem& Second)
	{
		return { R.Filler(), R.Filler(), First, R.Filler(), R.Filler(), R.Filler(), Second };
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationSameDrainOrderingTest, "StealthGame.Assassination.SameDrainOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationSameDrainOrderingTest::RunTest(const FString& Parameters)
{
	using namespace SameDrainOrderingTestData;

	// (a) Mutual strikes.
	{
		FRoundEncounterRig R(TEXT("SameDrainMutual"));
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		APawn* A = R.Spawn(FVector::ZeroVector, 1);
		APawn* B = R.Spawn(FVector(150.0, 0.0, 0.0), 3, 180.0);
		const uint64 Base = R.W.Queue->PeekNextSeq();
		R.Drain(Interleave(R, R.AttackAt(A, B), R.AttackAt(B, A)));
		const FQueueSlotRecord* First = R.CommandRecord(0);
		const FQueueSlotRecord* Second = R.CommandRecord(1);
		TestTrue(TEXT("(a) keys are (T, 3rd) and (T, 7th) of the drain"), First != nullptr && Second != nullptr && First->Key.Seq == Base + 2 && Second->Key.Seq == Base + 6
			&& First->Key.ServerTickIndex == Second->Key.ServerTickIndex);
		TestTrue(TEXT("(a) A kills B"), R.Core->GetResolutions().Num() == 1 && R.Core->GetResolutions()[0].Victim == B && R.Core->IsDead(B));
		TestTrue(TEXT("(a) B's Attack is Voided(NotNow)"), Second != nullptr && Second->Outcome == EQueueSlotOutcome::Voided && R.Core->GetReplies().Last().Code == ECommandRejectCode::NotNow);
		TestTrue(TEXT("(a) the voided attacker gets no lockout"), R.Defence.Find(B) == nullptr);
		TestEqual(TEXT("(a) exactly one pawn fell"), R.Kills.Num(), 1);
	}
	// (b) Two attackers on one victim.
	{
		FRoundEncounterRig R(TEXT("SameDrainTwoAttackers"));
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		APawn* A1 = R.Spawn(FVector::ZeroVector, 1);
		APawn* A2 = R.Spawn(FVector(0.0, 40.0, 0.0), 2);
		APawn* B = R.Spawn(FVector(150.0, 0.0, 0.0), 3);
		R.Spawn(FVector(150.0, 50.0, 0.0)); // a live NPC beside B: the terminal ref never strikes it
		R.Drain(Interleave(R, R.AttackAt(A1, B), R.AttackAt(A2, B)));
		const TArray<FAttackResolution>& Res = R.Core->GetResolutions();
		TestTrue(TEXT("(b) A1 kills B, A2 is NoContact"), Res.Num() == 2 && Res[0].Victim == B && Res[0].Outcome == EAttackOutcome::Kill
			&& Res[1].Victim == nullptr && Res[1].StrikeResult.Result == EStrikeResult::NoContact);
		TestEqual(TEXT("(b) one OnPawnKilled"), R.Kills.Num(), 1);
	}
	// (c) A final completion latches first; the strike is Rejected_Latched.
	{
		FRoundEncounterRig R(TEXT("SameDrainCompletion"));
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		APawn* A = R.Spawn(FVector::ZeroVector, 3);
		APawn* B = R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		R.Drain(Interleave(R, R.W.Completion(7, 1, 0), R.AttackAt(A, B)));
		const FQueueSlotRecord* Strike = R.CommandRecord(0);
		TestTrue(TEXT("(c) the completion latched the round"), R.W.Rig.Core->IsTerminated());
		TestTrue(TEXT("(c) the strike is Rejected_Latched"), Strike != nullptr && Strike->Outcome == EQueueSlotOutcome::Rejected_Latched);
		TestTrue(TEXT("(c) no lockout, no FStrikeResult, no kill"), R.Defence.Find(A) == nullptr && R.Core->GetResolutions().Num() == 0 && R.Kills.Num() == 0 && !R.Core->IsDead(B));
		TestTrue(TEXT("(c) the owner reply is a held NotNow"), R.Core->GetReplies().Num() == 1 && R.Core->GetReplies()[0].bHeld && R.Core->GetReplies()[0].Code == ECommandRejectCode::NotNow);
	}
	// (d) A braced attacker strikes a braced defender.
	{
		FRoundEncounterRig R(TEXT("SameDrainBothBraced"));
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		APawn* A = R.Spawn(FVector::ZeroVector, 1);
		APawn* B = R.Spawn(FVector(150.0, 0.0, 0.0), 3);
		R.Drain({ R.Command(A, ECommandKind::PrepareBegin), R.Command(B, ECommandKind::PrepareBegin) });
		TestTrue(TEXT("(d) both braced"), R.Core->GetRole(A) == EStrikeRole::Bracing && R.Core->GetRole(B) == EStrikeRole::Bracing);
		R.Drain({ R.AttackAt(A, B) });
		TestTrue(TEXT("(d) the braced attacker is reversed"), R.Core->GetResolutions().Num() == 1 && R.Core->GetResolutions()[0].Outcome == EAttackOutcome::Reversal
			&& R.Core->IsDead(A) && !R.Core->IsDead(B));
		TestTrue(TEXT("(d) A's own brace was superseded at its key"), R.Defence.Find(A) != nullptr && R.Defence.Find(A)->GetWindow().EndKey == R.CommandRecord(0)->Key);
	}
	return true;
}

#endif
