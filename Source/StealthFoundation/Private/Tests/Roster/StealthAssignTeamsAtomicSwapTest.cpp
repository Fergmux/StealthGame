// Story psr-006 (production/epics/participant-state-roster/story-006-assign-teams-atomic-swap.md):
// AC-PSR-08, a batch swap of two players across full teams applies atomically; the same moves one at a
// time fail on the first (F5).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterAssignTeamsAtomicSwapTest, "StealthGame.Roster.AssignTeamsAtomicSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterAssignTeamsAtomicSwapTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;

	// Batch swap on a 5/5 fixture.
	{
		// Arrange: BeginMatch(5), ten participants, 5/5 through AssignTeams itself, unlocked.
		FRosterTestFixture F;
		const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 10);
		TestEqual(TEXT("5/5 fixture assigns"), F.AssignSplit(Ids, 5), ERosterResult::Applied);
		const FParticipantId A = Ids[0];
		const FParticipantId Fp = Ids[5];

		// Act: one batch swaps A (team 0) with F (team 1).
		const FRosterTeamAssignment Swap[] = { { A, 1 }, { Fp, 0 } };
		const ERosterResult Result = F.Core.AssignTeams(Swap);

		// Assert
		TestEqual(TEXT("batch swap is Applied"), Result, ERosterResult::Applied);
		TestEqual(TEXT("team 0 still 5"), F.Core.CountRoster(0), 5);
		TestEqual(TEXT("team 1 still 5"), F.Core.CountRoster(1), 5);
		TestTrue(TEXT("A is on team 1"), F.Record(A).Team == ERosterTeam::Team1);
		TestTrue(TEXT("F is on team 0"), F.Record(Fp).Team == ERosterTeam::Team0);
	}

	// The same two moves one at a time, from a fresh 5/5 fixture (not the post-swap state).
	{
		FRosterTestFixture F;
		const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 10);
		TestEqual(TEXT("fresh 5/5 fixture assigns"), F.AssignSplit(Ids, 5), ERosterResult::Applied);
		const FRosterSnapshot Before = F.Core.Snapshot();

		const FRosterTeamAssignment MoveA[] = { { Ids[0], 1 } };
		TestEqual(TEXT("the first single move returns Rejected_TeamFull"), F.Core.AssignTeams(MoveA), ERosterResult::Rejected_TeamFull);
		TestTrue(TEXT("the rejected move leaves the store unchanged"), F.Core.Snapshot() == Before);
	}
	return true;
}

#endif
