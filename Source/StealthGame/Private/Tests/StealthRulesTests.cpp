// Unit tests for FStealthRules. Run headless:
//   UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthGame.; Quit" -unattended -nullrhi
// Values below are boundary values from the confirmed rules (that exact number IS the point).
// StealthGame.Rules.Assassination and StealthGame.Rules.Defence were ported to the production cores by story asn-027
// (StealthGame.Assassination.PrototypeParity, Source/StealthGameplay/Private/Tests/Assassination/PrototypeParityTest.cpp)
// and removed here with the prototype resolver; the NPC-kill removal test was replaced by the production Penalty tests
// (StealthGame.Penalty.StrikeRuleMatrix, ExileOnLthNpcKill and the PIE PrototypeParityExile) and removed by story nkp-021.
// The tests below belong to the Objectives, Round, Session and SCL epics.

#include "Misc/AutomationTest.h"
#include "StealthRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace StealthRulesTestValues
{
	constexpr float RunThreshold = 300.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthRulesObjectiveTest, "StealthGame.Rules.ObjectiveStages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStealthRulesObjectiveTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Own stage-1 objective at stage 1"), FStealthRules::CanCompleteObjective(0, 1, 0, 1));
	TestFalse(TEXT("Other team's preliminary objective"), FStealthRules::CanCompleteObjective(0, 1, 1, 1));
	TestFalse(TEXT("Locked stage-2 objective at stage 1"), FStealthRules::CanCompleteObjective(0, 1, 0, 2));
	TestFalse(TEXT("Final objective locked before preliminaries"), FStealthRules::CanCompleteObjective(1, 2, STEALTH_NO_TEAM, STEALTH_FINAL_STAGE));
	TestTrue(TEXT("Shared final objective for team 0"), FStealthRules::CanCompleteObjective(0, STEALTH_FINAL_STAGE, STEALTH_NO_TEAM, STEALTH_FINAL_STAGE));
	TestTrue(TEXT("Shared final objective for team 1"), FStealthRules::CanCompleteObjective(1, STEALTH_FINAL_STAGE, STEALTH_NO_TEAM, STEALTH_FINAL_STAGE));
	TestFalse(TEXT("No team cannot complete anything"), FStealthRules::CanCompleteObjective(STEALTH_NO_TEAM, 1, STEALTH_NO_TEAM, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthRulesVictoryTest, "StealthGame.Rules.EliminationVictory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStealthRulesVictoryTest::RunTest(const FString& Parameters)
{
	{
		const int32 Players[2] = { 5, 5 };
		const int32 Active[2] = { 0, 2 };
		TestEqual(TEXT("Team 1 wins when team 0 has no active players"), FStealthRules::EvaluateEliminationWinner(Players, Active), static_cast<uint8>(1));
	}
	{
		const int32 Players[2] = { 5, 5 };
		const int32 Active[2] = { 1, 1 };
		TestEqual(TEXT("No winner while both teams have active players"), FStealthRules::EvaluateEliminationWinner(Players, Active), STEALTH_NO_TEAM);
	}
	{
		const int32 Players[2] = { 1, 0 };
		const int32 Active[2] = { 1, 0 };
		TestEqual(TEXT("A single-team test session never ends by elimination"), FStealthRules::EvaluateEliminationWinner(Players, Active), STEALTH_NO_TEAM);
	}
	{
		const int32 Players[2] = { 2, 2 };
		const int32 Active[2] = { 2, 0 };
		TestEqual(TEXT("Team 0 wins when team 1 is wiped out"), FStealthRules::EvaluateEliminationWinner(Players, Active), static_cast<uint8>(0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthRulesTeamsTest, "StealthGame.Rules.TeamsAndMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStealthRulesTeamsTest::RunTest(const FString& Parameters)
{
	using namespace StealthRulesTestValues;
	{
		const int32 Counts[2] = { 0, 0 };
		TestEqual(TEXT("First player joins team 0"), FStealthRules::ChooseTeamForNewPlayer(Counts, 5), static_cast<uint8>(0));
	}
	{
		const int32 Counts[2] = { 3, 2 };
		TestEqual(TEXT("Join the smaller team"), FStealthRules::ChooseTeamForNewPlayer(Counts, 5), static_cast<uint8>(1));
	}
	{
		const int32 Counts[2] = { 5, 5 };
		TestEqual(TEXT("Full session has no team"), FStealthRules::ChooseTeamForNewPlayer(Counts, 5), STEALTH_NO_TEAM);
	}
	TestFalse(TEXT("Walking (200) is not running"), FStealthRules::IsRunningSpeed(200.f, RunThreshold));
	TestTrue(TEXT("Sprinting (700) is running"), FStealthRules::IsRunningSpeed(700.f, RunThreshold));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
