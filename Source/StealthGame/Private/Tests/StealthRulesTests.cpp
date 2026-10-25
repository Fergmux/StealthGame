// Unit tests for FStealthRules. Run headless:
//   UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthGame.; Quit" -unattended -nullrhi
// Values below are boundary values from the confirmed rules (that exact number IS the point).

#include "Misc/AutomationTest.h"
#include "StealthRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace StealthRulesTestValues
{
	constexpr float Cooldown = 60.f;
	constexpr float Duration = 4.f;
	constexpr int32 NpcLimit = 3;
	constexpr float RunThreshold = 300.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthRulesAssassinationTest, "StealthGame.Rules.Assassination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStealthRulesAssassinationTest::RunTest(const FString& Parameters)
{
	using O = EStealthAssassinationOutcome;
	using K = EStealthTargetKind;

	TestEqual(TEXT("Unprepared active enemy is eliminated"), FStealthRules::ResolveAssassination(true, K::ActiveEnemy, false), O::EnemyEliminated);
	TestEqual(TEXT("Prepared active enemy reverses the attack"), FStealthRules::ResolveAssassination(true, K::ActiveEnemy, true), O::Reversed);
	TestEqual(TEXT("NPC target is an NPC kill"), FStealthRules::ResolveAssassination(true, K::Npc, false), O::NpcKilled);
	TestEqual(TEXT("Enemy informant kill is not an NPC kill"), FStealthRules::ResolveAssassination(true, K::InformantEnemy, false), O::InformantKilled);
	TestEqual(TEXT("Teammates cannot be attacked"), FStealthRules::ResolveAssassination(true, K::Teammate, false), O::Rejected);
	TestEqual(TEXT("Informants cannot attack"), FStealthRules::ResolveAssassination(false, K::ActiveEnemy, false), O::Rejected);
	TestEqual(TEXT("Informants cannot attack NPCs"), FStealthRules::ResolveAssassination(false, K::Npc, false), O::Rejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthRulesNpcPenaltyTest, "StealthGame.Rules.NpcKillRemoval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStealthRulesNpcPenaltyTest::RunTest(const FString& Parameters)
{
	using namespace StealthRulesTestValues;
	TestFalse(TEXT("Second NPC kill keeps the player active"), FStealthRules::ShouldRemoveForNpcKills(2, NpcLimit));
	TestTrue(TEXT("Removal happens ON the third NPC kill"), FStealthRules::ShouldRemoveForNpcKills(3, NpcLimit));
	TestTrue(TEXT("Beyond the limit is still removed"), FStealthRules::ShouldRemoveForNpcKills(4, NpcLimit));
	TestFalse(TEXT("A zero limit disables the penalty"), FStealthRules::ShouldRemoveForNpcKills(10, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthRulesDefenceTest, "StealthGame.Rules.Defence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStealthRulesDefenceTest::RunTest(const FString& Parameters)
{
	using namespace StealthRulesTestValues;
	constexpr double Never = -1.0;

	TestTrue(TEXT("First use is allowed"), FStealthRules::CanStartDefence(true, false, false, 10.0, Never, Cooldown));
	TestFalse(TEXT("Blocked while running"), FStealthRules::CanStartDefence(true, true, false, 10.0, Never, Cooldown));
	TestFalse(TEXT("Blocked during parkour"), FStealthRules::CanStartDefence(true, false, true, 10.0, Never, Cooldown));
	TestFalse(TEXT("Blocked for informants"), FStealthRules::CanStartDefence(false, false, false, 10.0, Never, Cooldown));
	TestFalse(TEXT("Cooldown runs from activation"), FStealthRules::CanStartDefence(true, false, false, 69.9, 10.0, Cooldown));
	TestTrue(TEXT("Usable again once per minute"), FStealthRules::CanStartDefence(true, false, false, 70.0, 10.0, Cooldown));
	TestEqual(TEXT("Cooldown remaining"), FStealthRules::DefenceCooldownRemaining(40.0, 10.0, Cooldown), 30.f);
	TestEqual(TEXT("No cooldown before first use"), FStealthRules::DefenceCooldownRemaining(40.0, Never, Cooldown), 0.f);

	TestFalse(TEXT("Defence holds within its window"), FStealthRules::ShouldEndDefence(false, false, 13.9, 10.0, Duration));
	TestTrue(TEXT("Defence expires after its duration"), FStealthRules::ShouldEndDefence(false, false, 14.0, 10.0, Duration));
	TestTrue(TEXT("Running cancels the defence"), FStealthRules::ShouldEndDefence(true, false, 11.0, 10.0, Duration));
	TestTrue(TEXT("Parkour cancels the defence"), FStealthRules::ShouldEndDefence(false, true, 11.0, 10.0, Duration));
	return true;
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
