// Story psr-010 (production/epics/participant-state-roster/story-010-bind-pawn-guards.md):
// AC-PSR-22, BindPawn rejects an already-bound participant, an in-use pawn and a RoundEnded participant, and
// the binding table is unchanged (R9, I3). Each case violates exactly one rule, so the R9 precedence
// (WrongState, AlreadyBound, PawnInUse) does not mask it. Active states and the Retired row come from the
// core seams until ResetAll (psr-022) and RetireBinding (psr-020).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterBindPawnGuardsTest, "StealthGame.Roster.BindPawnGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterBindPawnGuardsTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;

	// Arrange: P0 Active with live pawn X; P1 Active with no pawn; P2 RoundEnded (a fresh registration);
	// pawn Z Retired for P0; W and Y unbound.
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 3);
	F.Core.SeamSetState(Ids[0], EParticipantState::Active);
	F.Core.SeamSetState(Ids[1], EParticipantState::Active);
	const FRosterPawnHandle X = Pawn(1), Y = Pawn(2), Z = Pawn(3), W = Pawn(4);
	TestEqual(TEXT("fixture: X binds to P0"), F.Core.BindPawn(Ids[0], X), ERosterResult::Applied);
	F.Core.SeamSetBinding(Z, Ids[0], ERosterBindingStatus::Retired, 0);
	const FRosterSnapshot Before = F.Core.Snapshot();
	FRosterEventRecorder Rec;
	F.Core.AddListener(&Rec);

	const auto ExpectRejected = [&](const TCHAR* What, ERosterResult Actual, ERosterResult Expected)
	{
		TestEqual(FString::Printf(TEXT("%s: result"), What), Actual, Expected);
		TestTrue(FString::Printf(TEXT("%s: binding table and records unchanged"), What), F.Core.Snapshot() == Before);
	};

	// Act + Assert
	ExpectRejected(TEXT("participant already has a live pawn"), F.Core.BindPawn(Ids[0], Y), ERosterResult::Rejected_AlreadyBound);
	ExpectRejected(TEXT("pawn BoundLive for another participant"), F.Core.BindPawn(Ids[1], X), ERosterResult::Rejected_PawnInUse);
	ExpectRejected(TEXT("pawn Retired"), F.Core.BindPawn(Ids[1], Z), ERosterResult::Rejected_PawnInUse);
	ExpectRejected(TEXT("RoundEnded participant"), F.Core.BindPawn(Ids[2], W), ERosterResult::Rejected_WrongState);

	// Extra (R9, same lookup): null, destroyed and pending-kill pawns are PawnInUse.
	F.Liveness.Overrides.Add(Pawn(5), ERosterPawnLiveness::Destroyed);
	F.Liveness.Overrides.Add(Pawn(6), ERosterPawnLiveness::PendingKill);
	ExpectRejected(TEXT("null pawn"), F.Core.BindPawn(Ids[1], FRosterPawnHandle()), ERosterResult::Rejected_PawnInUse);
	ExpectRejected(TEXT("destroyed pawn"), F.Core.BindPawn(Ids[1], Pawn(5)), ERosterResult::Rejected_PawnInUse);
	ExpectRejected(TEXT("pending-kill pawn"), F.Core.BindPawn(Ids[1], Pawn(6)), ERosterResult::Rejected_PawnInUse);
	// Precedence: a RoundEnded participant with an in-use pawn is WrongState (checked first).
	ExpectRejected(TEXT("precedence: WrongState before PawnInUse"), F.Core.BindPawn(Ids[2], X), ERosterResult::Rejected_WrongState);
	// Precedence: StaleEpoch before WrongState.
	FRosterEpochToken Stale = F.Core.GetEpochToken();
	Stale.RoundEpoch += 1;
	ExpectRejected(TEXT("precedence: StaleEpoch before WrongState"), F.Core.BindPawn(Ids[2], W, Stale), ERosterResult::Rejected_StaleEpoch);
	ExpectRejected(TEXT("unknown participant"), F.Core.BindPawn(FParticipantId(42), W), ERosterResult::Rejected_UnknownParticipant);

	TestEqual(TEXT("BindPawn emits no event"), Rec.Events.Num(), 0);

	// Control: the one legal bind applies (P1 Active, no pawn, W unbound) with the current epoch.
	TestEqual(TEXT("control: a legal bind applies"), F.Core.BindPawn(Ids[1], W, F.Core.GetEpochToken()), ERosterResult::Applied);
	TestTrue(TEXT("control: W is BoundLive"), F.Core.IsPawnBound(W));
	TestEqual(TEXT("control: the bind emitted no event"), Rec.Events.Num(), 0);
	F.Core.RemoveListener(&Rec);
	return true;
}

#endif
