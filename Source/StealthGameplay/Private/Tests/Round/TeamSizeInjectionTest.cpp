// Story rmf-021 (production/epics/round-match-framework/story-021-team-size-injection.md), GDD AC 39 / R3:
// a preset with a different TeamSize in Lobby calls SetTeamSize once; Rejected_TeamSizeBelowRoster keeps the previous
// preset and tells the host; with teams locked SetTeamSize is never called; the same TeamSize makes no call.
// Rollback open point: the previous preset is the Round-held applied TeamSize (FRoundRuleParams), committed only on
// Applied, so a refusal needs no TDC call.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundTeamSizeInjectionTest, "StealthGame.Round.TeamSizeInjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundTeamSizeInjectionTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	const FRoundCommandCaller Host = FRoundExitBRig::HostCaller();

	// BeginMatch(TeamSize) once when the match is created.
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		const TArray<FRoundFakeCall> Calls = Rig.CallsSince(0);
		TestTrue(TEXT("OpenMatch calls BeginMatch(TeamSize) once"), Calls.Num() >= 1 && Calls[0].Call == FName(TEXT("BeginMatch")) && Calls[0].Args[0] == FixtureTeamSize);
		TestEqual(TEXT("exactly one BeginMatch"), Rig.CountSince(0, TEXT("BeginMatch")), 1);
	}

	// A different TeamSize: one SetTeamSize call, the preset is committed.
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.Fakes.Roster.Script(TEXT("SetTeamSize"), static_cast<int64>(ERosterResult::Applied));
		FRoundCommandArgs Args;
		Args.PresetTeamSize = PresetTeamSize;
		const int32 Mark = Rig.Mark();
		TestTrue(TEXT("a different TeamSize is Executed"), Rig.Core->ExecuteHostCommand(Host, ERoundHostCommand::Preset, Args) == ERoundCommandResult::Executed);
		TestEqual(TEXT("SetTeamSize called once"), Rig.CountSince(Mark, TEXT("SetTeamSize")), 1);
		TestTrue(TEXT("SetTeamSize carried the preset's TeamSize"), Rig.CallsSince(Mark).Num() == 1 && Rig.CallsSince(Mark)[0].Args[0] == PresetTeamSize);
		TestEqual(TEXT("the applied TeamSize is the preset's"), Rig.Core->GetParams().TeamSize, PresetTeamSize);
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}

	// Rejected_TeamSizeBelowRoster: the previous preset stays and the host sees the refusal.
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.Fakes.Roster.Script(TEXT("SetTeamSize"), static_cast<int64>(ERosterResult::Rejected_TeamSizeBelowRoster));
		FRoundCommandArgs Args;
		Args.PresetTeamSize = PresetTeamSize;
		const int32 Mark = Rig.Mark();
		TestTrue(TEXT("a refused preset is Refused"), Rig.Core->ExecuteHostCommand(Host, ERoundHostCommand::Preset, Args) == ERoundCommandResult::Refused);
		TestEqual(TEXT("the refusal path made the one SetTeamSize call"), Rig.CountSince(Mark, TEXT("SetTeamSize")), 1);
		TestEqual(TEXT("the previous preset's TeamSize stays"), Rig.Core->GetParams().TeamSize, FixtureTeamSize);
		TestTrue(TEXT("the host sink received the refusal"), Rig.Host.CommandRefusals.Num() == 1
			&& Rig.Host.CommandRefusals[0].Detail == static_cast<uint8>(ERosterResult::Rejected_TeamSizeBelowRoster));
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}

	// The same TeamSize: no call.
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		FRoundCommandArgs Args;
		Args.PresetTeamSize = FixtureTeamSize;
		const int32 Mark = Rig.Mark();
		TestTrue(TEXT("the same TeamSize is a NoOp"), Rig.Core->ExecuteHostCommand(Host, ERoundHostCommand::Preset, Args) == ERoundCommandResult::NoOp);
		TestEqual(TEXT("the same TeamSize makes no SetTeamSize call"), Rig.CountSince(Mark, TEXT("SetTeamSize")), 0);
	}

	// Locked teams: SetTeamSize is never called (the unit rule; the console preset is WrongPhase outside Lobby anyway).
	{
		FRoundFakeCallLog Log;
		FRoundFakeGatewaySet Fakes(Log);
		FRoundFakeHostSink HostSink;
		FRoundTeamRules Rules(Fakes.Roster, &HostSink);
		int32 Applied = FixtureTeamSize;
		TestTrue(TEXT("locked teams: refused"), Rules.ApplyPresetTeamSize(Applied, PresetTeamSize, /*bTeamsLocked*/ true, ERoundPhase::Lobby) == ERoundCommandResult::Refused);
		TestEqual(TEXT("locked teams: SetTeamSize called 0 times"), Log.Calls.Num(), 0);
		TestEqual(TEXT("locked teams: applied TeamSize unchanged"), Applied, FixtureTeamSize);

		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.DriveTo(ERoundPhase::Active);
		FRoundCommandArgs Args;
		Args.PresetTeamSize = PresetTeamSize;
		const int32 Mark = Rig.Mark();
		TestTrue(TEXT("preset while locked (Active) is WrongPhase"), Rig.Core->ExecuteHostCommand(Rig.HostCaller(), ERoundHostCommand::Preset, Args) == ERoundCommandResult::WrongPhase);
		TestEqual(TEXT("preset while locked: SetTeamSize called 0 times"), Rig.CountSince(Mark, TEXT("SetTeamSize")), 0);
	}
	return true;
}

#endif
