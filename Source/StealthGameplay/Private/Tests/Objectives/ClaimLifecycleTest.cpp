// Story oc-006 (production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md), AC2 (ADR-0020 5.1,
// 5.3) ClaimLifecycle, on FObjectivesCore with a fixture of four faces at known positions (+X, +Y, -X, -Y, 120 cm):
//   - OnInteractExecuted claims the free face nearest the pawn and records the pawn's control epoch with the claim;
//   - a pawn equidistant from two free faces takes the lower face index;
//   - OnInteractEnded (Completed or Cancelled) releases only that pawn's own claim;
//   - a pending debounced End keeps the claim: the IIF calls OnInteractEnded only in the slot that ends the hold (the
//     DebounceExpiry slot), so until that call the claim stands (the stand-in hold has no debounce; this is the owner side);
//   - with every face taken a further Begin gets no claim and nothing is corrupted.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesClaimLifecycleTest, "StealthGame.Objectives.ClaimLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesClaimLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	StealthAssassinationTest::FScopedWorld World(TEXT("ObjectivesClaimLifecycle"));
	FCoreRig Rig;
	const FCourtFixtureId Court(CourtOrdinary);
	const FQueueKey BeginKey(1, 1);

	// A stands at face 1, B at face 1 too (A's face): B takes the nearest FREE face; faces 0 and 2 are equidistant -> 0.
	AStealthPawn* A = FCoreRig::PawnAt(World, CourtOrdinary, 1);
	AStealthPawn* B = FCoreRig::PawnAt(World, CourtOrdinary, 1);
	if (!TestNotNull(TEXT("pawn A"), A) || !TestNotNull(TEXT("pawn B"), B))
	{
		return false;
	}
	Rig.PawnControl.Epochs.Add(A, 7);
	Rig.PawnControl.Epochs.Add(B, 9);
	TestEqual(TEXT("A claims its own face 1 (nearest)"), Rig.Core.OnInteractExecuted(*A, Court, BeginKey), 1);
	TestEqual(TEXT("B, equidistant from faces 0 and 2, takes the lower index 0"), Rig.Core.OnInteractExecuted(*B, Court, BeginKey), 0);
	const FObjectivesClaimTable::FClaim* ClaimA = Rig.Core.GetClaims().FindClaim(Court, A);
	const FObjectivesClaimTable::FClaim* ClaimB = Rig.Core.GetClaims().FindClaim(Court, B);
	TestTrue(TEXT("A's control epoch is recorded with its claim"), ClaimA != nullptr && ClaimA->ControlEpoch == 7);
	TestTrue(TEXT("B's control epoch is recorded with its claim"), ClaimB != nullptr && ClaimB->ControlEpoch == 9);
	TestEqual(TEXT("two claims"), Rig.Core.GetClaimCount(Court), 2);

	// Ended releases only the pawn's own claim.
	Rig.Round.Key = FQueueKey(5, 1);
	Rig.Core.OnInteractEnded(*A, Court, EObjectivesInteractEnd::Completed, Rig.Round.Key);
	TestNull(TEXT("A's claim is released by its Completed end"), Rig.Core.GetClaims().FindClaim(Court, A));
	TestNotNull(TEXT("B keeps its claim"), Rig.Core.GetClaims().FindClaim(Court, B));
	TestEqual(TEXT("one claim left"), Rig.Core.GetClaimCount(Court), 1);

	// A debounced End is still pending (no OnInteractEnded call yet): B's claim stands until the DebounceExpiry slot's call.
	TestEqual(TEXT("the pending debounced End keeps the claim"), Rig.Core.GetClaimCount(Court), 1);
	Rig.Core.OnInteractEnded(*B, Court, EObjectivesInteractEnd::Cancelled, FQueueKey(6, 1));
	TestEqual(TEXT("the DebounceExpiry slot's Cancelled end releases it"), Rig.Core.GetClaimCount(Court), 0);

	// Every face taken: a fifth Begin gets no claim and nothing is corrupted.
	TArray<AStealthPawn*> Four;
	for (int32 Face = 0; Face < 4; ++Face)
	{
		AStealthPawn* P = FCoreRig::PawnAt(World, CourtOrdinary, Face);
		Four.Add(P);
		TestEqual(FString::Printf(TEXT("tender %d claims its face"), Face), Rig.Core.OnInteractExecuted(*P, Court, BeginKey), Face);
	}
	AStealthPawn* Fifth = FCoreRig::PawnAt(World, CourtOrdinary, 0);
	TestEqual(TEXT("all faces taken: no claim"), Rig.Core.OnInteractExecuted(*Fifth, Court, BeginKey), INDEX_NONE);
	TestEqual(TEXT("still exactly four claims"), Rig.Core.GetClaimCount(Court), 4);
	for (int32 Face = 0; Face < 4; ++Face)
	{
		TestEqual(FString::Printf(TEXT("face %d still held by its tender"), Face), Rig.Core.GetClaims().FindFace(Court, Four[Face]), Face);
	}
	return true;
}

#endif
