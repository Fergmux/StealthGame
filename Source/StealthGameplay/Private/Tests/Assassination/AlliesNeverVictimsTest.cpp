// Story asn-005, AC2 / AC-ASN-09 AlliesNeverVictims (ASN-R9). A forged PawnRef naming the attacker's ally falls through
// with allies excluded: a rival 50 cm further along the attacker-to-ally line is the victim; with no rival NoContact; a
// bush holding only allies NoContact. Every case executes with the same reply (Executed, same reply tick), and the forged
// ref resolves exactly as a TargetSpot at the same point. Run for a team-0 and a team-1 attacker and ally.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationAlliesNeverVictimsTest, "StealthGame.Assassination.AlliesNeverVictims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationAlliesNeverVictimsTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	for (const int32 Team : { 0, 1 })
	{
		const int32 Rival = 1 - Team;
		TArray<FEncounterCommandReply> Replies;

		// Forged ally ref with a rival 50 cm beyond the ally.
		{
			FRig R(TEXT("AlliesRivalBehind"));
			APawn* A = R.Spawn(FVector::ZeroVector, Team);
			APawn* Ally = R.Spawn(FVector(120.0, 0.0, 0.0), Team);
			APawn* Behind = R.Spawn(FVector(170.0, 0.0, 0.0), Rival);
			FVictimRequest Ref;
			Ref.Attacker = R.Facts(A);
			Ref.WorldPoint = R.Facts(Ally).Location;
			Ref.PawnRef = Ally;
			FVictimRequest Spot = Ref;
			Spot.PawnRef = nullptr;
			TestTrue(FString::Printf(TEXT("team %d: the forged ally ref is indistinguishable from a spot strike"), Team),
				FVictimResolverCore::Resolve(R.Params, R.Core->MakeResolverDeps(), Ref) == FVictimResolverCore::Resolve(R.Params, R.Core->MakeResolverDeps(), Spot));
			R.AttackRef(A, Ally);
			TestTrue(FString::Printf(TEXT("team %d: the rival behind the ally is the victim"), Team), R.Last().Victim == Behind && R.Last().StrikeResult.Result == EStrikeResult::RivalEliminated);
			TestTrue(FString::Printf(TEXT("team %d: the ally is never struck"), Team), !R.Core->IsDead(Ally) && R.Kills.Num() == 1);
			Replies.Add(R.LastReply());
		}
		// Forged ally ref, no rival.
		{
			FRig R(TEXT("AlliesNoRival"));
			APawn* A = R.Spawn(FVector::ZeroVector, Team);
			APawn* Ally = R.Spawn(FVector(120.0, 0.0, 0.0), Team);
			R.AttackRef(A, Ally);
			TestTrue(FString::Printf(TEXT("team %d: no rival: NoContact"), Team), R.Last().Victim == nullptr && R.Last().StrikeResult.Result == EStrikeResult::NoContact);
			TestTrue(FString::Printf(TEXT("team %d: no kill, no transition"), Team), R.Kills.Num() == 0 && R.Roster.Transitions.Num() == 0);
			Replies.Add(R.LastReply());
		}
		// A bush holding only allies.
		{
			FRig R(TEXT("AlliesOnlyBush"));
			APawn* A = R.Spawn(FVector::ZeroVector, Team);
			R.AddBush(1, FVector(150.0, 0.0, 0.0), 60.0);
			APawn* Hidden = R.Spawn(FVector(150.0, 10.0, 0.0), Team);
			R.Conceal(Hidden, 1);
			R.Attack(A, FVector(150.0, 0.0, 0.0));
			TestTrue(FString::Printf(TEXT("team %d: allies-only bush: NoContact"), Team), R.Last().Victim == nullptr && R.Last().Rule == EVictimRule::None
				&& R.Last().BushId == 1 && R.Last().StrikeResult.Result == EStrikeResult::NoContact);
			Replies.Add(R.LastReply());
		}
		for (int32 Index = 1; Index < Replies.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("team %d: case %d replies exactly as case 0 (Executed, same tick)"), Team, Index), Replies[Index].SameReplyAs(Replies[0]) && Replies[Index].bExecuted);
		}
	}
	return true;
}

#endif
