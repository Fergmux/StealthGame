// Story psr-013 (production/epics/participant-state-roster/story-013-resolve-victim-kind-matrix.md):
// AC-PSR-26, ResolveVictimKind answers the full pawn-state matrix in the R10 evaluation order (Stale, then
// AlreadyResolved, then RoundOver, then the live kinds) and never yields Npc for a NotTargetable input.
// RoundEnded-with-binding and Retired fixtures come from the core seams until EndRoundAll (psr-023) and
// RetireBinding (psr-020).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterResolveVictimKindMatrixTest, "StealthGame.Roster.ResolveVictimKindMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterResolveVictimKindMatrixTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;
	using K = ERosterVictimKind;
	using R = ERosterNotTargetableReason;

	// Arrange: A Active, B Informant, C RoundEnded (bound while Active), D RoundEnded (retired), E Active (pawn destroyed).
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 5);
	const FParticipantId A = Ids[0], B = Ids[1], C = Ids[2], D = Ids[3], E = Ids[4];
	for (const FParticipantId Id : Ids)
	{
		F.Core.SeamSetState(Id, EParticipantState::Active);
	}
	F.Core.SeamSetState(B, EParticipantState::Informant);

	const FRosterPawnHandle LiveActive = Pawn(1), LiveInformant = Pawn(2), Unbound = Pawn(3), Retired = Pawn(4);
	const FRosterPawnHandle Destroyed = Pawn(5), PendingKill = Pawn(6), Null;
	const FRosterPawnHandle LiveRoundEnded = Pawn(8), RetiredRoundEnded = Pawn(9);
	TestEqual(TEXT("fixture: A binds"), F.Core.BindPawn(A, LiveActive), ERosterResult::Applied);
	TestEqual(TEXT("fixture: B binds"), F.Core.BindPawn(B, LiveInformant), ERosterResult::Applied);
	TestEqual(TEXT("fixture: C binds"), F.Core.BindPawn(C, LiveRoundEnded), ERosterResult::Applied);
	TestEqual(TEXT("fixture: E binds"), F.Core.BindPawn(E, Destroyed), ERosterResult::Applied);
	F.Core.SeamSetBinding(Retired, A, ERosterBindingStatus::Retired, 0);
	F.Core.SeamSetBinding(RetiredRoundEnded, D, ERosterBindingStatus::Retired, 0);
	F.Core.SeamSetState(C, EParticipantState::RoundEnded, true);
	F.Core.SeamSetState(D, EParticipantState::RoundEnded, true);
	F.Liveness.Overrides.Add(Destroyed, ERosterPawnLiveness::Destroyed);   // bound to an Active participant: Stale wins
	F.Liveness.Overrides.Add(PendingKill, ERosterPawnLiveness::PendingKill); // unbound: Stale wins over Npc

	struct FRow
	{
		const TCHAR* Name;
		FRosterPawnHandle Pawn;
		FRosterVictimKind Expected;
	};
	const FRow Matrix[] = {
		{ TEXT("BoundLive + Active"), LiveActive, FRosterVictimKind::Make(K::Player) },
		{ TEXT("BoundLive + Informant"), LiveInformant, FRosterVictimKind::Make(K::Informant) },
		{ TEXT("Unbound"), Unbound, FRosterVictimKind::Make(K::Npc) },
		{ TEXT("Retired"), Retired, FRosterVictimKind::NotTargetable(R::AlreadyResolved) },
		{ TEXT("destroyed"), Destroyed, FRosterVictimKind::NotTargetable(R::Stale) },
		{ TEXT("pending kill"), PendingKill, FRosterVictimKind::NotTargetable(R::Stale) },
		{ TEXT("null"), Null, FRosterVictimKind::NotTargetable(R::Stale) },
		{ TEXT("BoundLive + RoundEnded"), LiveRoundEnded, FRosterVictimKind::NotTargetable(R::RoundOver) },
		{ TEXT("Retired + RoundEnded"), RetiredRoundEnded, FRosterVictimKind::NotTargetable(R::AlreadyResolved) },
	};

	// Act + Assert
	for (const FRow& Row : Matrix)
	{
		const FRosterVictimKind Actual = F.Core.ResolveVictimKind(Row.Pawn);
		TestTrue(FString::Printf(TEXT("%s: kind %d reason %d, expected kind %d reason %d"), Row.Name,
			static_cast<int32>(Actual.Kind), static_cast<int32>(Actual.Reason), static_cast<int32>(Row.Expected.Kind), static_cast<int32>(Row.Expected.Reason)),
			Actual == Row.Expected);
		// No NotTargetable input ever yields Npc.
		if (Row.Expected.Kind == K::NotTargetable)
		{
			TestTrue(FString::Printf(TEXT("%s: a NotTargetable input never resolves Npc"), Row.Name), Actual.Kind != K::Npc);
		}
	}
	TestEqual(TEXT("the server matrix raises no fault"), F.Sink.GetRecords().Num(), 0);
	return true;
}

#endif
