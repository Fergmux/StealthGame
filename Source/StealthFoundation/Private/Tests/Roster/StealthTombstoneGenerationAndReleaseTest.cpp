// Story psr-011 (production/epics/participant-state-roster/story-011-tombstone-generation-and-release.md):
// AC-PSR-24, tombstones respect generations and ReleaseToCrowd needs a Disconnected participant (R9, I5).
// Disconnected and Connected participants come from NotifyConnectionChanged (psr-009); the Retired handle
// from the core seam until RetireBinding (psr-020).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterTombstoneGenerationAndReleaseTest, "StealthGame.Roster.TombstoneGenerationAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterTombstoneGenerationAndReleaseTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;
	const FRosterVictimKind Npc = FRosterVictimKind::Make(ERosterVictimKind::Npc);

	// Arrange: P0 Disconnected with a live pawn L and a Retired pawn R {K, Gen1}; P1 Connected with live pawn C.
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 2);
	F.Core.SeamSetState(Ids[0], EParticipantState::Active);
	F.Core.SeamSetState(Ids[1], EParticipantState::Active);
	const FRosterPawnHandle L = Pawn(1), C = Pawn(3);
	const FRosterPawnHandle RetiredGen1 = Pawn(2, 1);
	const FRosterPawnHandle SameObjectGen2 = Pawn(2, 2);
	TestEqual(TEXT("fixture: L binds to P0"), F.Core.BindPawn(Ids[0], L), ERosterResult::Applied);
	TestEqual(TEXT("fixture: C binds to P1"), F.Core.BindPawn(Ids[1], C), ERosterResult::Applied);
	F.Core.SeamSetBinding(RetiredGen1, Ids[0], ERosterBindingStatus::Retired, 0);
	TestEqual(TEXT("fixture: P0 disconnects"), F.Core.NotifyConnectionChanged(Ids[0], ERosterConnection::Disconnected, 4000), ERosterResult::Applied);

	// The same object back with a new generation is Npc: the tombstone is a miss, not a mutation.
	TestTrue(TEXT("Gen1 handle is AlreadyResolved"),
		F.Core.ResolveVictimKind(RetiredGen1) == FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::AlreadyResolved));
	TestTrue(TEXT("Gen2 handle of the same object resolves Npc"), F.Core.ResolveVictimKind(SameObjectGen2) == Npc);
	TestFalse(TEXT("Gen2 handle is not bound"), F.Core.IsPawnBound(SameObjectGen2));
	TestTrue(TEXT("the Gen1 tombstone is untouched by the Gen2 lookup"), F.Core.FindBinding(RetiredGen1) != nullptr);

	// A Connected participant's pawn cannot be released.
	const FRosterSnapshot BeforeConnected = F.Core.Snapshot();
	TestEqual(TEXT("ReleaseToCrowd on a Connected participant's pawn"), F.Core.ReleaseToCrowd(C), ERosterResult::Rejected_NotDisconnected);
	TestTrue(TEXT("the Connected binding is unchanged"), F.Core.Snapshot() == BeforeConnected);
	TestTrue(TEXT("C still resolves Player"), F.Core.ResolveVictimKind(C) == FRosterVictimKind::Make(ERosterVictimKind::Player));

	// Release the Disconnected participant's BoundLive pawn.
	TestEqual(TEXT("ReleaseToCrowd on a Disconnected BoundLive pawn"), F.Core.ReleaseToCrowd(L), ERosterResult::Applied);
	TestTrue(TEXT("released live pawn resolves Npc"), F.Core.ResolveVictimKind(L) == Npc);
	TestFalse(TEXT("released live pawn is not bound"), F.Core.IsPawnBound(L));
	TestFalse(TEXT("P0 has no live pawn"), F.Core.GetPawn(Ids[0]).IsSet());

	// Release the Disconnected participant's Retired pawn.
	TestEqual(TEXT("ReleaseToCrowd on a Disconnected Retired pawn"), F.Core.ReleaseToCrowd(RetiredGen1), ERosterResult::Applied);
	TestTrue(TEXT("released retired pawn resolves Npc"), F.Core.ResolveVictimKind(RetiredGen1) == Npc);
	TestFalse(TEXT("released retired pawn is not bound"), F.Core.IsPawnBound(RetiredGen1));

	// An Unbound pawn: nothing to release.
	TestEqual(TEXT("ReleaseToCrowd on an Unbound pawn"), F.Core.ReleaseToCrowd(Pawn(9)), ERosterResult::NoOp_NotBound);

	// I5: a recycled object never inherits an old tombstone. Retire {K2, Gen1}, then bind {K2, Gen2}.
	const FRosterPawnHandle Recycled1 = Pawn(7, 1), Recycled2 = Pawn(7, 2);
	F.Core.SeamSetBinding(Recycled1, Ids[0], ERosterBindingStatus::Retired, 0);
	F.Core.SeamSetState(Ids[0], EParticipantState::Informant);
	TestEqual(TEXT("the recycled object's new generation binds as a fresh pawn"), F.Core.BindPawn(Ids[0], Recycled2), ERosterResult::Applied);
	TestTrue(TEXT("the new generation resolves as its own binding (Informant)"),
		F.Core.ResolveVictimKind(Recycled2) == FRosterVictimKind::Make(ERosterVictimKind::Informant));
	TestTrue(TEXT("the old generation is still the tombstone"),
		F.Core.ResolveVictimKind(Recycled1) == FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::AlreadyResolved));
	return true;
}

#endif
