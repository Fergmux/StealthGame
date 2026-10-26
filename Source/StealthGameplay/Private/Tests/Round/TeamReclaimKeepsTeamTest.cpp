// Story rmf-019 (production/epics/round-match-framework/story-019-team-reclaim-keeps-team.md), GDD AC 37 / R3:
// a reclaimed Dormant key keeps its team, no AssignTeams call is made and the teams are not rebalanced, even when the
// key's team is the larger one. The new-key path (story rmf-018) is the control: it calls AssignTeams once.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundTeamReclaimKeepsTeamTest, "StealthGame.Round.TeamReclaimKeepsTeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundTeamReclaimKeepsTeamTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	// Arrange: Lobby, 3/2 with the Dormant key on the 3 side (id 1, team 0).
	FRoundExitBRig Rig;
	Rig.OpenLobby();
	Rig.SetLayout(Layout3v2.Team0, Layout3v2.Team1);
	const FParticipantId DormantKey(FRoundExitBRig::HostIdValue);
	Rig.Fakes.Session.ConnectionStates.Add(DormantKey.Value, ERoundConnectionState::Dormant);
	const int32 Mark = Rig.Mark();

	// Act: the Roster reports the reclaim.
	Rig.Core->OnParticipantReclaimed(DormantKey);

	// Assert
	TestEqual(TEXT("the reclaim makes no AssignTeams call"), Rig.CountSince(Mark, TEXT("AssignTeams")), 0);
	TestEqual(TEXT("the reclaim makes no Roster call at all"), Rig.CallsSince(Mark, /*bMutatingOnly*/ false).Num(), 0);
	TestTrue(TEXT("the reclaimed key keeps team 0"), Rig.Fakes.Roster.Members[0].Team == ERosterTeam::Team0);
	TestEqual(TEXT("team 0 still holds 3 (no rebalance)"), Rig.Fakes.Roster.CountOn(ERosterTeam::Team0), Layout3v2.Team0);
	TestEqual(TEXT("team 1 still holds 2"), Rig.Fakes.Roster.CountOn(ERosterTeam::Team1), Layout3v2.Team1);

	// Control: a NEW key on the same layout is assigned with exactly one call.
	const FParticipantId NewKey(static_cast<uint32>(Rig.Fakes.Roster.Members.Num() + 1));
	Rig.Fakes.Roster.Members.Add({ NewKey, ERosterTeam::None });
	Rig.Fakes.Roster.Script(TEXT("AssignTeams"), static_cast<int64>(ERosterResult::Applied));
	const int32 ControlMark = Rig.Mark();
	TestTrue(TEXT("control: the new key joins team 1 (the smaller)"), Rig.Core->OnParticipantRegistered(NewKey) == ERosterTeam::Team1);
	TestEqual(TEXT("control: one AssignTeams call for a new key"), Rig.CountSince(ControlMark, TEXT("AssignTeams")), 1);
	TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
