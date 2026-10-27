// Story psr-020 (production/epics/participant-state-roster/story-020-informant-kill-is-not-a-transition.md), AC-PSR-21:
// killing an informant retires its binding without a state event (R9, R10, F2; decision D10). An Informant (InRound) with
// a BoundLive pawn: RetireBinding(pawn, T, InformantKilled) leaves the state Informant, dispatches no event and consumes no
// EventSeq, raises AwaitingPawn(T) by exactly one (InformantCount(T) unchanged, AwaitingPawn <= InformantCount) and the body
// resolves NotTargetable(AlreadyResolved) with RetireAtServerTime stored on the tombstone; a second call returns
// NoOp_AlreadyRetired and an Unbound pawn returns NoOp_NotBound, neither changing anything. Landed with NPC-Kill Penalty
// story nkp-008 (its informant-kill caller). AwaitingPawn is computed here from the records (F2: InRound Informants of
// the team with no live pawn); the Roster exposes no AwaitingPawn query yet.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

namespace InformantKillIsNotATransitionTestData
{
	/** F2 AwaitingPawn(Team): InRound Informants of Team with no live pawn. */
	int32 AwaitingPawn(const FRosterTestFixture& F, const TArray<FParticipantId>& Ids, ERosterTeam Team)
	{
		int32 Count = 0;
		for (const FParticipantId Id : Ids)
		{
			const FParticipantRecord Record = F.Record(Id);
			Count += Record.Team == Team && Record.bInRound && Record.State == EParticipantState::Informant && !Record.HasLivePawn() ? 1 : 0;
		}
		return Count;
	}

	/** The retire time the call passes (a fixture value; the Roster only stores it). */
	constexpr int64 RetireAtMs = 123456;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterInformantKillIsNotATransitionTest, "StealthGame.Roster.InformantKillIsNotATransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterInformantKillIsNotATransitionTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;
	using namespace InformantKillIsNotATransitionTestData;

	// Arrange: 2v1, every participant InRound; P (team 0) an Informant with a BoundLive pawn (its replacement body).
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 3);
	F.AssignSplit(Ids, 2);
	for (const FParticipantId Id : Ids)
	{
		F.Core.SeamSetState(Id, EParticipantState::Active, true);
	}
	const FParticipantId P = Ids[0];
	F.Core.SeamSetState(P, EParticipantState::Informant, true);
	const FRosterPawnHandle Body = Pawn(1);
	TestTrue(TEXT("the informant's body binds"), F.Core.BindPawn(P, Body) == ERosterResult::Applied);
	const ERosterTeam Team = F.Record(P).Team;
	FRosterEventRecorder Spy;
	F.Core.AddListener(&Spy);
	const uint64 SeqBefore = F.Core.GetEventSeq();
	const int32 AwaitingBefore = AwaitingPawn(F, Ids, Team);
	const int32 InformantsBefore = F.Core.CountInformant(0);
	TestTrue(TEXT("before: the informant resolves Informant"), F.Core.ResolveVictimKind(Body) == FRosterVictimKind::Make(ERosterVictimKind::Informant));

	// Act: the informant kill's retire.
	const ERosterResult First = F.Core.RetireBinding(Body, RetireAtMs, ERosterReason::InformantKilled);

	// Assert: retired, not a transition.
	TestTrue(TEXT("RetireBinding applies"), First == ERosterResult::Applied);
	TestTrue(TEXT("the state stays Informant"), F.Record(P).State == EParticipantState::Informant);
	TestEqual(TEXT("no event is dispatched"), Spy.Events.Num(), 0);
	TestTrue(TEXT("no EventSeq is consumed"), F.Core.GetEventSeq() == SeqBefore);
	TestFalse(TEXT("the live pawn is cleared"), F.Record(P).HasLivePawn());
	TestEqual(TEXT("AwaitingPawn(T) rises by exactly one"), AwaitingPawn(F, Ids, Team), AwaitingBefore + 1);
	TestEqual(TEXT("InformantCount(T) is unchanged"), F.Core.CountInformant(0), InformantsBefore);
	TestTrue(TEXT("AwaitingPawn(T) <= InformantCount(T)"), AwaitingPawn(F, Ids, Team) <= F.Core.CountInformant(0));
	TestTrue(TEXT("the body resolves NotTargetable(AlreadyResolved)"), F.Core.ResolveVictimKind(Body)
		== FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::AlreadyResolved));
	const FRosterBindingEntry* Tombstone = F.Core.FindBinding(Body);
	TestTrue(TEXT("the tombstone keeps the participant and RetireAtServerTime"), Tombstone != nullptr && Tombstone->Status == ERosterBindingStatus::Retired
		&& Tombstone->Participant == P && Tombstone->RetireAtServerTimeMs == RetireAtMs);
	TestTrue(TEXT("a recycled pawn (next generation) does not inherit the tombstone"), F.Core.ResolveVictimKind(Pawn(1, 2)) == FRosterVictimKind::Make(ERosterVictimKind::Npc));

	// A second call: NoOp_AlreadyRetired, nothing changes.
	const FRosterSnapshot AfterFirst = F.Core.Snapshot();
	TestTrue(TEXT("a second RetireBinding is NoOp_AlreadyRetired"), F.Core.RetireBinding(Body, RetireAtMs + 1, ERosterReason::InformantKilled) == ERosterResult::NoOp_AlreadyRetired);
	TestTrue(TEXT("the second call changes nothing"), F.Core.Snapshot() == AfterFirst);

	// Edge: an Unbound pawn is NoOp_NotBound and changes nothing.
	TestTrue(TEXT("an Unbound pawn is NoOp_NotBound"), F.Core.RetireBinding(Pawn(9), RetireAtMs, ERosterReason::InformantKilled) == ERosterResult::NoOp_NotBound);
	TestTrue(TEXT("the NoOp_NotBound call changes nothing"), F.Core.Snapshot() == AfterFirst);
	TestEqual(TEXT("still no event"), Spy.Events.Num(), 0);
	TestEqual(TEXT("no fault"), F.Sink.GetRecords().Num(), 0);

	F.Core.RemoveListener(&Spy);
	return true;
}

#endif
