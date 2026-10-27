// Story asn-006 (production/epics/assassination-defence-reversal/story-006-bush-spot-rule-and-victim-pick.md), AC1 /
// AC-ASN-12 BlindBushStrikeIsACommitment (ASN-R8 rule 2), on the stub-Concealment harness (IBushSpotQuery fake; the
// Concealment epic is not built). A bush (footprint 60 cm, zone = footprint + BushSpotToleranceCm 75 = 135 cm) concealing
// one rival: 10 enumerated WorldPoints across the zone (centre, footprint edge and zone boundary included), each in a
// fresh round, resolve the same single candidate and RivalEliminated. An empty bush with a visible NPC 10 cm outside the
// footprint inside the zone: a TargetSpot inside the zone is NoContact and the NPC is untouched (rule 2 is terminal;
// without the bush the same strike kills it). An Exiting occupant already un-culled is still a zone candidate.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

namespace BlindBushStrikeTestData
{
	const FVector BushCenter(180.0, 0.0, 0.0);
	constexpr double FootprintCm = 60.0;
	/** Ten points of the zone (radius 135 around the centre): centre, footprint edge, zone boundary, interior. */
	const FVector Points[] = {
		FVector(180.0, 0.0, 0.0), FVector(240.0, 0.0, 0.0), FVector(120.0, 0.0, 0.0), FVector(180.0, 60.0, 0.0), FVector(180.0, -60.0, 0.0),
		FVector(180.0, 135.0, 0.0), FVector(180.0, -135.0, 0.0), FVector(315.0, 0.0, 0.0), FVector(45.0, 0.0, 0.0), FVector(275.0, 95.0, 0.0),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationBlindBushStrikeIsACommitmentTest, "StealthGame.Assassination.BlindBushStrikeIsACommitment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationBlindBushStrikeIsACommitmentTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace BlindBushStrikeTestData;

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Points); ++Index)
	{
		FRig R(TEXT("BlindBushPoint")); // a fresh round per point (no lockout carried over)
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.AddBush(1, BushCenter, FootprintCm);
		APawn* Rival = R.Spawn(BushCenter, 1);
		R.Conceal(Rival, 1);
		R.Attack(A, Points[Index]);
		const FAttackResolution& Res = R.Last();
		TestTrue(FString::Printf(TEXT("point %d (%.0f, %.0f): rule 2 kills the concealed rival"), Index, Points[Index].X, Points[Index].Y),
			Res.Rule == EVictimRule::BushSpot && Res.BushId == 1 && Res.Victim == Rival && Res.StrikeResult.Result == EStrikeResult::RivalEliminated);
		TestTrue(FString::Printf(TEXT("point %d: the victim killed concealed leaves the occupancy in the slot"), Index), Res.bVictimConcealed && R.Bushes.Removed.Contains(Rival));
	}

	// An empty bush: a visible NPC 10 cm outside the footprint, inside the zone, is never struck through rule 2.
	{
		FRig R(TEXT("BlindBushEmpty"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.AddBush(1, BushCenter, FootprintCm);
		APawn* Outside = R.Spawn(BushCenter + FVector(0.0, FootprintCm + 10.0, 0.0));
		const FVector Point = BushCenter + FVector(0.0, FootprintCm, 0.0);
		R.Attack(A, Point);
		TestTrue(TEXT("empty bush: NoContact"), R.Last().Victim == nullptr && R.Last().BushId == 1 && R.Last().StrikeResult.Result == EStrikeResult::NoContact);
		TestTrue(TEXT("empty bush: the NPC outside is untouched"), !R.Core->IsDead(Outside) && R.Kills.Num() == 0);
		// Control: with no bush there, the same point is an open-spot hit on that NPC (so rule 2 is what stopped it).
		R.Bushes.Bushes.Reset();
		FVictimRequest Request;
		Request.Attacker = R.Facts(A);
		Request.WorldPoint = Point;
		TestTrue(TEXT("control: without the bush rule 3 would strike it"), FVictimResolverCore::Resolve(R.Params, R.Core->MakeResolverDeps(), Request).Victim == Outside);
	}

	// An Exiting occupant already un-culled (listed by Concealment, not concealed) is still a zone candidate.
	{
		FRig R(TEXT("BlindBushExiting"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		FFakeBushes::FBush& Bush = R.AddBush(1, BushCenter, FootprintCm);
		APawn* Exiting = R.Spawn(BushCenter + FVector(0.0, 40.0, 0.0), 1);
		Bush.Occupants.Add(Exiting);
		R.Attack(A, BushCenter);
		TestTrue(TEXT("an Exiting, un-culled occupant is struck by rule 2"), R.Last().Rule == EVictimRule::BushSpot && R.Last().Victim == Exiting && !R.Last().bVictimConcealed);
	}
	return true;
}

#endif
