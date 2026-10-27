// Story asn-005, AC3 / AC-ASN-17 DeadBodyIsNoContact (ASN-R8, F4). A dying NPC body and a dying player body (strike role
// Dead from their kill slot; the NPC's death write still pending, the player's binding also retired: Roster answers
// AlreadyResolved) are struck by PawnRef and by TargetSpot with no other pawn near: all four NoContact, no OnPawnKilled,
// no strike charged (a Penalty stub spy counting Cause = Strike and VictimKind = Npc stays at the body's own kill). With a
// live NPC 50 cm beside each body the PawnRef strikes stay NoContact (terminal; the neighbour is untouched), while the
// TargetSpot strike resolves exactly as if the body were absent, field for field identically for the NPC and player body.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

namespace DeadBodyIsNoContactTestData
{
	using namespace StealthEncounterTest;

	struct FRun
	{
		FAttackResolution SpotWithNeighbour;
		int32 NeighbourKills = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationDeadBodyIsNoContactTest, "StealthGame.Assassination.DeadBodyIsNoContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationDeadBodyIsNoContactTest::RunTest(const FString& Parameters)
{
	using namespace DeadBodyIsNoContactTestData;
	FRun Runs[2];
	for (int32 BodyKind = 0; BodyKind < 2; ++BodyKind)
	{
		const TCHAR* Name = BodyKind == 0 ? TEXT("NPC body") : TEXT("player body");
		FRig R(TEXT("DeadBodyWorld"));
		int32 PenaltyStrikes = 0; // the Penalty stub: one strike per OnPawnKilled(Cause = Strike, VictimKind = Npc)
		R.Core->OnPawnKilled.AddLambda([&PenaltyStrikes](APawn*, APawn*, EVictimKind Kind, EKillCause Cause) { PenaltyStrikes += Kind == EVictimKind::Npc && Cause == EKillCause::Strike ? 1 : 0; });
		APawn* Killer = R.Spawn(FVector::ZeroVector, 0);
		APawn* Body = BodyKind == 0 ? R.Spawn(FVector(150.0, 0.0, 0.0)) : R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		R.AttackRef(Killer, Body);
		TestTrue(FString::Printf(TEXT("%s: the body is Dead from its kill slot (death write pending)"), Name), R.Core->IsDead(Body) && R.Queue.Staged.Num() == 1);
		if (BodyKind == 1)
		{
			R.Roster.Resolved.Add(Body); // the retired binding answers AlreadyResolved too
		}
		const int32 KillsBefore = R.Kills.Num();
		const int32 StrikesBefore = PenaltyStrikes;
		const FVector BodyPoint = R.Facts(Body).Location;

		// Alone: PawnRef and TargetSpot, each by a fresh attacker.
		APawn* ByRef = R.Spawn(FVector(0.0, 10.0, 0.0), 0);
		APawn* BySpot = R.Spawn(FVector(0.0, -10.0, 0.0), 0);
		R.AttackRef(ByRef, Body);
		TestTrue(FString::Printf(TEXT("%s, PawnRef alone: NoContact"), Name), R.Last().Victim == nullptr && R.Last().StrikeResult.Result == EStrikeResult::NoContact);
		R.Attack(BySpot, BodyPoint);
		TestTrue(FString::Printf(TEXT("%s, TargetSpot alone: NoContact"), Name), R.Last().Victim == nullptr && R.Last().StrikeResult.Result == EStrikeResult::NoContact);
		TestEqual(FString::Printf(TEXT("%s: no OnPawnKilled"), Name), R.Kills.Num(), KillsBefore);
		TestEqual(FString::Printf(TEXT("%s: no strike charged"), Name), PenaltyStrikes, StrikesBefore);

		// A live NPC 50 cm beside the body.
		APawn* Neighbour = R.Spawn(FVector(150.0, 50.0, 0.0));
		APawn* ByRef2 = R.Spawn(FVector(0.0, 20.0, 0.0), 0);
		R.AttackRef(ByRef2, Body);
		TestTrue(FString::Printf(TEXT("%s, PawnRef with a neighbour: still NoContact (terminal)"), Name), R.Last().Victim == nullptr && !R.Core->IsDead(Neighbour));

		// The TargetSpot strike equals the same strike with the body removed (probe both, then execute).
		APawn* BySpot2 = R.Spawn(FVector(0.0, -20.0, 0.0), 0);
		FVictimRequest Request;
		Request.Attacker = R.Facts(BySpot2);
		Request.WorldPoint = BodyPoint;
		const FVictimResolution WithBody = FVictimResolverCore::Resolve(R.Params, R.Core->MakeResolverDeps(), Request);
		const FEncounterPawnFacts BodyFacts = R.Facts(Body);
		R.Pawns.Remove(Body);
		const FVictimResolution WithoutBody = FVictimResolverCore::Resolve(R.Params, R.Core->MakeResolverDeps(), Request);
		R.Pawns.Pawns.Add(BodyFacts);
		TestTrue(FString::Printf(TEXT("%s: TargetSpot with the body equals the body-removed strike (full struct)"), Name), WithBody == WithoutBody && WithBody.Victim == Neighbour);
		R.Attack(BySpot2, BodyPoint);
		Runs[BodyKind].SpotWithNeighbour = R.Last();
		Runs[BodyKind].NeighbourKills = R.Kills.FilterByPredicate([Neighbour](const FKill& K) { return K.Victim == Neighbour; }).Num();
		TestTrue(FString::Printf(TEXT("%s: the spot strike kills the neighbour"), Name), Runs[BodyKind].SpotWithNeighbour.Victim == Neighbour && Runs[BodyKind].NeighbourKills == 1);
	}
	// NPC body vs player body: every decided field equal (the victims are each run's own neighbour, at the same place).
	FAttackResolution Npc = Runs[0].SpotWithNeighbour;
	FAttackResolution Player = Runs[1].SpotWithNeighbour;
	Npc.Victim = nullptr;
	Player.Victim = nullptr;
	TestTrue(TEXT("the TargetSpot result is identical for the NPC body and the player body"), Npc.SameOutcomeAs(Player));
	return true;
}

#endif
