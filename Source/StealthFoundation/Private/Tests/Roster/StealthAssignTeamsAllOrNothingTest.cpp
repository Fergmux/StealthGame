// Story psr-007 (production/epics/participant-state-roster/story-007-assign-teams-all-or-nothing.md):
// AC-PSR-09, an invalid team batch changes nothing and fires no OnTeamsChanged (R5, F5). Each batch carries
// exactly one defect so the R5 order (locked, unknown, invalid, full) does not mask the intended code.
// The "no event" clause is asserted against the real event queue (story psr-008 is built in the same batch),
// through a recording listener, and through an unchanged EventSeq.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterAssignTeamsAllOrNothingTest, "StealthGame.Roster.AssignTeamsAllOrNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterAssignTeamsAllOrNothingTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;

	// Arrange: ten participants on 5/5, unlocked, a recording listener.
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 10);
	TestEqual(TEXT("5/5 fixture assigns"), F.AssignSplit(Ids, 5), ERosterResult::Applied);
	FRosterEventRecorder Spy;
	F.Core.AddListener(&Spy);
	const FRosterSnapshot Before = F.Core.Snapshot();

	struct FCase
	{
		const TCHAR* Name;
		TArray<FRosterTeamAssignment> Batch;
		ERosterResult Expected;
	};
	const TArray<FCase> Cases = {
		// 6 on team 0: F moves to team 0 and nothing else is wrong.
		{ TEXT("six on one team"), { { Ids[5], 0 } }, ERosterResult::Rejected_TeamFull },
		{ TEXT("unknown id"), { { FParticipantId(99), 0 } }, ERosterResult::Rejected_UnknownParticipant },
		{ TEXT("duplicate id"), { { Ids[0], 0 }, { Ids[0], 0 } }, ERosterResult::Rejected_InvalidBatch },
		{ TEXT("team 2"), { { Ids[0], 2 } }, ERosterResult::Rejected_InvalidBatch },
		// F5: an empty batch is invalid (outside the AC's four cases; cheap to cover).
		{ TEXT("empty batch"), {}, ERosterResult::Rejected_InvalidBatch },
	};

	for (const FCase& Case : Cases)
	{
		// Act
		const ERosterResult Result = F.Core.AssignTeams(Case.Batch);
		// Assert: the code, nothing changed (validation is pure: it returns before any write), no event.
		TestEqual(FString::Printf(TEXT("%s: result"), Case.Name), Result, Case.Expected);
		TestEqual(FString::Printf(TEXT("%s: ValidateTeamBatch agrees"), Case.Name), F.Core.ValidateTeamBatch(Case.Batch), Case.Expected);
		TestTrue(FString::Printf(TEXT("%s: store unchanged"), Case.Name), F.Core.Snapshot() == Before);
		TestEqual(FString::Printf(TEXT("%s: no event dispatched"), Case.Name), Spy.Events.Num(), 0);
		TestEqual(FString::Printf(TEXT("%s: EventSeq unchanged"), Case.Name), F.Core.GetEventSeq(), Before.EventSeq);
	}
	F.Core.RemoveListener(&Spy);
	return true;
}

#endif
