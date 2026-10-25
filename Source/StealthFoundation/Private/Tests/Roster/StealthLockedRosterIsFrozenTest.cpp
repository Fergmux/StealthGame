// Story psr-005 (production/epics/participant-state-roster/story-005-locked-roster-is-frozen.md):
// AC-PSR-06, a locked roster rejects registration, removal and team changes, keeps RosterCount, and still
// reclaims a known non-Dormant key (R4, R5, F3).
// The Dormant record comes from the real Lobby projection (NotifyConnectionChanged, story psr-009, built in
// the same batch), so no Dormant test seam is needed. R4 names no locked result for SetTeamSize; the AC's
// Rejected_TeamsLocked is asserted (GDD gap recorded in the story).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterLockedRosterIsFrozenTest, "StealthGame.Roster.LockedRosterIsFrozen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterLockedRosterIsFrozenTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;

	// Arrange: TeamSize 2; P0..P3 register, P3 leaves the Lobby (Dormant), P4 takes the freed slot; 2v2 assigned.
	FRosterTestFixture F;
	TArray<FParticipantId> Ids = F.BeginAndRegister(2, 4);
	TestEqual(TEXT("P3 goes Dormant"), F.Core.NotifyConnectionChanged(Ids[3], ERosterConnection::Dormant, 500), ERosterResult::Applied);
	const FParticipantId P4 = F.Register(4);
	TestTrue(TEXT("P4 registers into the Dormant record's slot"), P4.IsValid());
	const FRosterTeamAssignment Teams[] = { { Ids[0], 0 }, { Ids[1], 0 }, { Ids[2], 1 }, { P4, 1 } };
	TestEqual(TEXT("2v2 assigned"), F.Core.AssignTeams(Teams), ERosterResult::Applied);
	TestEqual(TEXT("teams lock (the Dormant record may stay Team None)"), F.Core.SetTeamsLocked(true), ERosterResult::Applied);
	TestEqual(TEXT("locking again is a no-op"), F.Core.SetTeamsLocked(true), ERosterResult::NoOp_Unchanged);
	TestTrue(TEXT("teams are locked"), F.Core.AreTeamsLocked());

	const int32 Roster0 = F.Core.CountRoster(0);
	const int32 Roster1 = F.Core.CountRoster(1);
	TestEqual(TEXT("RosterCount(0) is 2"), Roster0, 2);
	TestEqual(TEXT("RosterCount(1) is 2"), Roster1, 2);
	const auto CountsUnchanged = [&](const TCHAR* After)
	{
		TestEqual(FString::Printf(TEXT("RosterCount(0) unchanged after %s"), After), F.Core.CountRoster(0), Roster0);
		TestEqual(FString::Printf(TEXT("RosterCount(1) unchanged after %s"), After), F.Core.CountRoster(1), Roster1);
	};

	// Act + Assert: each locked call is Rejected_TeamsLocked and RosterCount does not move.
	TestEqual(TEXT("a new key is rejected"), F.Core.RegisterParticipant(Key(9), TEXT("New")).Result, ERosterResult::Rejected_TeamsLocked);
	CountsUnchanged(TEXT("new key"));
	TestEqual(TEXT("the Dormant record's key is rejected"), F.Core.RegisterParticipant(Key(3), TEXT("P3")).Result, ERosterResult::Rejected_TeamsLocked);
	CountsUnchanged(TEXT("Dormant key"));
	TestTrue(TEXT("the Dormant record stays Dormant"), F.Record(Ids[3]).Connection.Projection == ERosterConnection::Dormant);
	TestEqual(TEXT("RemoveParticipant is rejected"), F.Core.RemoveParticipant(Ids[0]), ERosterResult::Rejected_TeamsLocked);
	CountsUnchanged(TEXT("RemoveParticipant"));
	TestTrue(TEXT("the record survives the rejected removal"), F.Core.FindRecord(Ids[0]) != nullptr);
	const FRosterTeamAssignment Swap[] = { { Ids[0], 1 }, { Ids[2], 0 } };
	TestEqual(TEXT("AssignTeams is rejected"), F.Core.AssignTeams(Swap), ERosterResult::Rejected_TeamsLocked);
	CountsUnchanged(TEXT("AssignTeams"));
	TestTrue(TEXT("teams unchanged after the rejected batch"), F.Record(Ids[0]).Team == ERosterTeam::Team0);
	TestEqual(TEXT("SetTeamSize is rejected"), F.Core.SetTeamSize(3), ERosterResult::Rejected_TeamsLocked);
	TestEqual(TEXT("TeamSize frozen"), F.Core.GetTeamSize(), 2);
	CountsUnchanged(TEXT("SetTeamSize"));

	// A known non-Dormant key still reclaims (the reclaim match precedes the lock check, R4).
	const FRosterRegisterResult Reclaim = F.Core.RegisterParticipant(Key(1), TEXT("P1"));
	TestEqual(TEXT("a known non-Dormant key returns Reclaimed"), Reclaim.Result, ERosterResult::Reclaimed);
	TestEqual(TEXT("the reclaim returns the same id"), Reclaim.Id, Ids[1]);
	CountsUnchanged(TEXT("reclaim"));

	// Unlocked checks of SetTeamSize (R4): outside 1-5 invalid, below the largest team's roster rejected.
	TestEqual(TEXT("unlock"), F.Core.SetTeamsLocked(false), ERosterResult::Applied);
	TestEqual(TEXT("SetTeamSize(6) is invalid"), F.Core.SetTeamSize(6), ERosterResult::Rejected_InvalidTeamSize);
	TestEqual(TEXT("SetTeamSize(1) is below the roster"), F.Core.SetTeamSize(1), ERosterResult::Rejected_TeamSizeBelowRoster);
	TestEqual(TEXT("SetTeamSize(3) applies"), F.Core.SetTeamSize(3), ERosterResult::Applied);

	// Locking with a non-Dormant Team None record is refused (I2).
	TestEqual(TEXT("P5 registers unassigned"), F.Core.RegisterParticipant(Key(5), TEXT("P5")).Result, ERosterResult::Registered);
	TestEqual(TEXT("lock refused while a non-Dormant record has Team None"), F.Core.SetTeamsLocked(true), ERosterResult::Rejected_UnassignedParticipants);
	return true;
}

#endif
