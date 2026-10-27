// Story asn-005 (production/epics/assassination-defence-reversal/story-005-victim-resolution-order.md), AC1 / AC-ASN-08
// VictimResolutionOrder (ASN-R8 rules 1, 3, 4). Attacker at the origin facing +X, every pawn inside F1 reach unless stated;
// defaults PawnRefAgreementCm 75, OpenSpotToleranceCm 75, r_c 35 (open-spot radius 110). Fixture positions per the QA
// rewrite: a named pawn beats a nearer one; a culled ref, or a ref 76 cm from the point, equals the TargetSpot strike
// (full result equality); 109 cm is a candidate and 111 cm never is (110 inclusive); exact ties go to the lower spawn
// serial in both spawn orders of kind; a ref at exactly 75 cm is honoured; an honoured ref out of reach is a whiff.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

namespace VictimResolutionOrderTestData
{
	using namespace StealthEncounterTest;

	/** The pure resolution of a strike (no state change), so two forms of one strike compare in one rig. */
	inline FVictimResolution Probe(FRig& R, APawn* Attacker, const FVector& WorldPoint, APawn* Ref)
	{
		FVictimRequest Request;
		Request.Attacker = R.Facts(Attacker);
		Request.WorldPoint = WorldPoint;
		Request.PawnRef = Ref;
		Request.Key = FQueueKey(7000, 3);
		return FVictimResolverCore::Resolve(R.Params, R.Core->MakeResolverDeps(), Request);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationVictimResolutionOrderTest, "StealthGame.Assassination.VictimResolutionOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationVictimResolutionOrderTest::RunTest(const FString& Parameters)
{
	using namespace VictimResolutionOrderTestData;

	// Rule 1: an honoured PawnRef beats a pawn nearer the WorldPoint.
	{
		FRig R(TEXT("VictimOrderHonoured"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Named = R.Spawn(FVector(150.0, 0.0, 0.0));
		APawn* Nearer = R.Spawn(FVector(150.0, 60.0, 0.0));
		const FVector Point(150.0, 40.0, 0.0); // Named 40 cm, Nearer 20 cm from the point
		TestTrue(TEXT("honoured ref: the named pawn is the victim"), Probe(R, A, Point, Named).Victim == Named);
		TestTrue(TEXT("TargetSpot at the same point: the nearer pawn"), Probe(R, A, Point, nullptr).Victim == Nearer);
		TestTrue(TEXT("the honoured strike executes and kills the named pawn"), R.Attack(A, Point, Named) == EResolveResult::Executed
			&& R.Last().Victim == Named && R.Last().Outcome == EAttackOutcome::Kill && R.Last().Rule == EVictimRule::PawnRef);

		// A culled ref is identical to a TargetSpot at the same point.
		R.Visibility.Culled.Add(Nearer); // (Named is now Dead; probe the culled ref on Nearer against the spot strike)
		APawn* Other = R.Spawn(FVector(150.0, 90.0, 0.0));
		const FVector Point2(150.0, 75.0, 0.0); // Nearer 15 cm, Other 15 cm: the tie goes to Nearer's lower serial
		TestTrue(TEXT("culled ref equals the TargetSpot result (full struct)"), Probe(R, A, Point2, Nearer) == Probe(R, A, Point2, nullptr));
		TestTrue(TEXT("culled ref: Other is never favoured by the ref"), Probe(R, A, FVector(150.0, 100.0, 0.0), Nearer).Victim == Other);
	}

	// A ref 76 cm from the point is not honoured; exactly 75 cm is.
	{
		FRig R(TEXT("VictimOrderAgreement"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Named = R.Spawn(FVector(150.0, 0.0, 0.0));
		APawn* Spot = R.Spawn(FVector(150.0, 96.0, 0.0));
		const FVector Far(150.0, 76.0, 0.0);
		const FVector Edge(150.0, 75.0, 0.0);
		TestTrue(TEXT("a ref 76 cm away equals the TargetSpot at that point"), Probe(R, A, Far, Named) == Probe(R, A, Far, nullptr));
		TestTrue(TEXT("and the spot strike takes the nearer pawn"), Probe(R, A, Far, nullptr).Victim == Spot);
		TestTrue(TEXT("a ref exactly 75 cm away is honoured"), Probe(R, A, Edge, Named).Victim == Named && Probe(R, A, Edge, Named).Rule == EVictimRule::PawnRef);
	}

	// Rule 3: 109 cm is a candidate, 111 cm never is, 110 is inclusive.
	{
		FRig R(TEXT("VictimOrderRadius"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* In = R.Spawn(FVector(150.0, 109.0, 0.0));
		APawn* Out = R.Spawn(FVector(150.0, -111.0, 0.0));
		const FVector Point(150.0, 0.0, 0.0);
		TestTrue(TEXT("the 109 cm pawn is the victim"), Probe(R, A, Point, nullptr).Victim == In);
		R.Pawns.Remove(In);
		TestTrue(TEXT("the 111 cm pawn alone: NoContact"), Probe(R, A, Point, nullptr).Victim == nullptr);
		TestTrue(TEXT("and the strike executes as a whiff"), R.Attack(A, Point) == EResolveResult::Executed && R.Last().StrikeResult.Result == EStrikeResult::NoContact && Out != nullptr);
		APawn* Boundary = R.Spawn(FVector(150.0, 110.0, 0.0));
		TestTrue(TEXT("110 cm is inclusive"), Probe(R, A, Point, nullptr).Victim == Boundary);
	}

	// Exact ties: the lower spawn serial, in both spawn orders of kind.
	for (const bool bNpcFirst : { true, false })
	{
		FRig R(bNpcFirst ? TEXT("VictimOrderTieNpcFirst") : TEXT("VictimOrderTiePlayerFirst"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* First = bNpcFirst ? R.Spawn(FVector(150.0, 30.0, 0.0)) : R.Spawn(FVector(150.0, 30.0, 0.0), 1);
		APawn* Second = bNpcFirst ? R.Spawn(FVector(150.0, -30.0, 0.0), 1) : R.Spawn(FVector(150.0, -30.0, 0.0));
		const FVector Point(150.0, 0.0, 0.0);
		TestTrue(FString::Printf(TEXT("%s spawned first: the lower serial wins the tie"), bNpcFirst ? TEXT("NPC") : TEXT("player")),
			Probe(R, A, Point, nullptr).Victim == First && Second != nullptr);
	}

	// An honoured ref that fails F1 is a whiff, never a fall-through.
	{
		FRig R(TEXT("VictimOrderRefOutOfReach"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Climber = R.Spawn(FVector(150.0, 0.0, 100.0)); // dz 100 > StrikeVerticalReachCm 75
		APawn* Beside = R.Spawn(FVector(150.0, 50.0, 0.0));
		const FVector Point(150.0, 0.0, 0.0);
		TestTrue(TEXT("honoured ref out of reach: no victim"), Probe(R, A, Point, Climber).Victim == nullptr);
		TestTrue(TEXT("the same point as a spot strike finds the pawn beside it (no fall-through by the ref)"), Probe(R, A, Point, nullptr).Victim == Beside);
	}
	return true;
}

#endif
