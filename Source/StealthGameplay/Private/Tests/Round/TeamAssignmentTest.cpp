// Story rmf-018 (production/epics/round-match-framework/story-018-team-assignment.md), GDD AC 36 / R3:
// a new key in Lobby joins the smaller team; a tie goes to team RoundsPlayedCount mod 2 (team 0 at 0, team 1 at 1).
// One AssignTeams call per registration; existing members keep their teams. Headless (T1), fake Roster gateway.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundTeamAssignmentTest, "StealthGame.Round.TeamAssignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundTeamAssignmentTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	// Pure rule.
	TestTrue(TEXT("2/1 -> team 1"), FRoundTeamRules::ChooseTeamForNewKey(Layout2v1.Team0, Layout2v1.Team1, 0) == ERosterTeam::Team1);
	TestTrue(TEXT("1/2 -> team 0"), FRoundTeamRules::ChooseTeamForNewKey(Layout2v1.Team1, Layout2v1.Team0, 1) == ERosterTeam::Team0);
	TestTrue(TEXT("2/2 at RoundsPlayedCount 0 -> team 0"), FRoundTeamRules::ChooseTeamForNewKey(Layout2v2.Team0, Layout2v2.Team1, 0) == ERosterTeam::Team0);
	TestTrue(TEXT("2/2 at RoundsPlayedCount 1 -> team 1"), FRoundTeamRules::ChooseTeamForNewKey(Layout2v2.Team0, Layout2v2.Team1, 1) == ERosterTeam::Team1);
	TestTrue(TEXT("first registrant (0/0 at 0) -> team 0"), FRoundTeamRules::ChooseTeamForNewKey(0, 0, 0) == ERosterTeam::Team0);

	struct FCase
	{
		const TCHAR* Name;
		FRoundTeamLayout Layout;
		uint32 RoundsPlayed;
		ERosterTeam Expected;
	};
	const FCase Cases[] = {
		{ TEXT("2/1"), Layout2v1, 0, ERosterTeam::Team1 },
		{ TEXT("2/2 RoundsPlayedCount 0"), Layout2v2, 0, ERosterTeam::Team0 },
		{ TEXT("2/2 RoundsPlayedCount 1"), Layout2v2, 1, ERosterTeam::Team1 },
	};
	for (const FCase& Case : Cases)
	{
		// Arrange: the layout plus the new key (Team None) in a Lobby fake Roster.
		FRoundFakeCallLog Log;
		FRoundFakeGatewaySet Fakes(Log);
		FRoundFakeHostSink Host;
		Fakes.Roster.SetLayout(Case.Layout.Team0, Case.Layout.Team1);
		const FParticipantId NewKey(static_cast<uint32>(Fakes.Roster.Members.Num() + 1));
		Fakes.Roster.Members.Add({ NewKey, ERosterTeam::None });
		const TArray<FRoundRosterMember> Before = Fakes.Roster.Members;
		Fakes.Roster.Script(TEXT("AssignTeams"), static_cast<int64>(ERosterResult::Applied));
		FRoundTeamRules Rules(Fakes.Roster, &Host);

		// Act
		const ERosterTeam Chosen = Rules.OnParticipantRegistered(NewKey, ERoundPhase::Lobby, Case.RoundsPlayed);

		// Assert
		TestTrue(FString::Printf(TEXT("%s: joins the expected team"), Case.Name), Chosen == Case.Expected);
		int32 AssignCalls = 0;
		for (const FRoundFakeCall& Call : Log.Calls)
		{
			if (Call.Call == FName(TEXT("AssignTeams")))
			{
				++AssignCalls;
				TestTrue(FString::Printf(TEXT("%s: the one batch assigns only the new key"), Case.Name),
					Call.Args.Num() == 2 && Call.Args[0] == NewKey.Value && Call.Args[1] == static_cast<int64>(Case.Expected));
			}
		}
		TestEqual(FString::Printf(TEXT("%s: exactly one AssignTeams call"), Case.Name), AssignCalls, 1);
		for (int32 Index = 0; Index + 1 < Before.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("%s: member %d keeps its team"), Case.Name, Index), Fakes.Roster.Members[Index].Team == Before[Index].Team);
		}
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Case.Name), Log.Unexpected.IsEmpty());
	}

	// Outside Lobby nothing is assigned (a joiner between rounds is a rejoin path).
	{
		FRoundFakeCallLog Log;
		FRoundFakeGatewaySet Fakes(Log);
		Fakes.Roster.SetLayout(Layout2v1.Team0, Layout2v1.Team1);
		FRoundTeamRules Rules(Fakes.Roster, nullptr);
		TestTrue(TEXT("Preparing: not assigned"), Rules.OnParticipantRegistered(FParticipantId(9), ERoundPhase::Preparing, 0) == ERosterTeam::None);
		TestTrue(TEXT("Preparing: no Roster call"), Log.Calls.IsEmpty());
	}
	return true;
}

#endif
