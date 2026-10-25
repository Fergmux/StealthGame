// Story psr-003 (production/epics/participant-state-roster/story-003-off-game-thread-mutation-rejected.md):
// AC-PSR-03, a mutation from a worker thread is rejected and faulted and changes nothing (R2, R8, I7).
// The worker thread is the injected thread check reporting "not game thread" (deterministic, no wall-clock
// wait). Authority stays true because Rejected_NotAuthority outranks Rejected_WrongThread (R8 steps 1, 2).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterOffGameThreadMutationAssertsTest, "StealthGame.Roster.OffGameThreadMutationAsserts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterOffGameThreadMutationAssertsTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;

	// Arrange: one Active participant with a bound pawn (Active through the core seam until ResetAll, psr-022).
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 2);
	F.Core.SeamSetState(Ids[0], EParticipantState::Active);
	TestEqual(TEXT("fixture pawn binds"), F.Core.BindPawn(Ids[0], Pawn(1)), ERosterResult::Applied);
	const FRosterSnapshot Before = F.Core.Snapshot();
	F.ThreadCheck.bGameThread = false;

	// Act + Assert: each call returns Rejected_WrongThread and adds exactly one fault with the expected code.
	const ERosterResult Transition = F.Core.RequestTransition(Ids[0], EParticipantState::Informant, ERosterReason::Assassinated);
	TestEqual(TEXT("RequestTransition off the game thread"), Transition, ERosterResult::Rejected_WrongThread);
	TestEqual(TEXT("one fault after RequestTransition"), F.Sink.GetRecords().Num(), 1);

	const ERosterResult Bind = F.Core.BindPawn(Ids[1], Pawn(2));
	TestEqual(TEXT("BindPawn off the game thread"), Bind, ERosterResult::Rejected_WrongThread);
	TestEqual(TEXT("one fault per call"), F.Sink.GetRecords().Num(), 2);

	for (const FStealthFaultRecord& Record : F.Sink.GetRecords())
	{
		TestTrue(TEXT("fault code is RosterWrongThread"), Record.Fault == EStealthFault::RosterWrongThread);
		TestEqual(TEXT("fault source is the Roster"), Record.Source, RosterFaultSource());
	}
	TestEqual(TEXT("first fault names RequestTransition"), F.Sink.GetRecords()[0].Detail, static_cast<int64>(ERosterCall::RequestTransition));
	TestEqual(TEXT("second fault names BindPawn"), F.Sink.GetRecords()[1].Detail, static_cast<int64>(ERosterCall::BindPawn));

	// Nothing changed: records, bindings, EventSeq, RoundEpoch, MatchId.
	TestTrue(TEXT("state snapshot unchanged"), F.Core.Snapshot() == Before);

	// Control: back on the game thread the same transition applies, so the rejection was the thread check.
	F.ThreadCheck.bGameThread = true;
	TestEqual(TEXT("control: on the game thread the transition applies"),
		F.Core.RequestTransition(Ids[0], EParticipantState::Informant, ERosterReason::Assassinated), ERosterResult::Applied);
	return true;
}

#endif
