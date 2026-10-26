// Story rmf-020 (production/epics/round-match-framework/story-020-swap-team-rules.md), GDD AC 38 / R3, R22:
// in Lobby a single SwapTeam is refused iff the target team holds TeamSize (one below succeeds); a 5/5 pair exchange
// succeeds as ONE atomic AssignTeams batch; any swap outside Lobby returns WrongPhase with no Roster call; after the
// lock a disconnect changes no count and a Dormant slot is freed only by remove.
// Pair-exchange entry point (story open point): stealth.round.swapteam <participant> <participant>, which reaches
// ExchangeTeams; one argument is the single move (R22 lists the one-participant form).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSwapTeamRulesTest, "StealthGame.Round.SwapTeamRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundSwapTeamRulesTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	const FRoundCommandCaller Host = FRoundExitBRig::HostCaller();

	// --- Single swap refused into a full team -------------------------------------------------------------------
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout4v5.Team0, Layout4v5.Team1);
		FRoundCommandArgs Args;
		Args.Participant = FParticipantId(FRoundExitBRig::HostIdValue); // on team 0; target team 1 holds 5
		const int32 Mark = Rig.Mark();
		TestTrue(TEXT("swap into a team holding TeamSize is Refused"), Rig.Core->ExecuteHostCommand(Host, ERoundHostCommand::SwapTeam, Args) == ERoundCommandResult::Refused);
		TestEqual(TEXT("the refused swap made no AssignTeams call"), Rig.CountSince(Mark, TEXT("AssignTeams")), 0);
		TestEqual(TEXT("the refusal reached the host sink"), Rig.Host.CommandRefusals.Num(), 1);
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}

	// --- Single swap into a team one below TeamSize succeeds -----------------------------------------------------
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout5v4.Team0, Layout5v4.Team1);
		Rig.Fakes.Roster.Script(TEXT("AssignTeams"), static_cast<int64>(ERosterResult::Applied));
		FRoundCommandArgs Args;
		Args.Participant = FParticipantId(FRoundExitBRig::HostIdValue); // team 0 -> team 1 (holds 4)
		TestTrue(TEXT("swap into a team one below TeamSize is Executed"), Rig.Core->ExecuteHostCommand(Host, ERoundHostCommand::SwapTeam, Args) == ERoundCommandResult::Executed);
		TestEqual(TEXT("team 1 now holds 5"), Rig.Fakes.Roster.CountOn(ERosterTeam::Team1), FixtureTeamSize);
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}

	// --- 5/5 pair exchange: one atomic batch ------------------------------------------------------------------------
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout5v5.Team0, Layout5v5.Team1);
		Rig.Fakes.Roster.Script(TEXT("AssignTeams"), static_cast<int64>(ERosterResult::Applied));
		FRoundCommandArgs Args;
		Args.Participant = FParticipantId(FRoundExitBRig::HostIdValue);                                  // team 0
		Args.Other = FParticipantId(FRoundExitBRig::HostIdValue + static_cast<uint32>(Layout5v5.Team0)); // team 1
		const int32 Mark = Rig.Mark();
		TestTrue(TEXT("5/5 pair exchange is Executed"), Rig.Core->ExecuteHostCommand(Host, ERoundHostCommand::SwapTeam, Args) == ERoundCommandResult::Executed);
		const TArray<FRoundFakeCall> Mutations = Rig.CallsSince(Mark);
		TestEqual(TEXT("the exchange is one mutating call"), Mutations.Num(), 1);
		TestTrue(TEXT("that call is AssignTeams with both entries"), Mutations.Num() == 1 && Mutations[0].Call == FName(TEXT("AssignTeams")) && Mutations[0].Args.Num() == 4);
		TestEqual(TEXT("team 0 holds 5 after (never above TeamSize)"), Rig.Fakes.Roster.CountOn(ERosterTeam::Team0), FixtureTeamSize);
		TestEqual(TEXT("team 1 holds 5 after"), Rig.Fakes.Roster.CountOn(ERosterTeam::Team1), FixtureTeamSize);
		TestTrue(TEXT("the pair swapped"), Rig.Fakes.Roster.Members[0].Team == ERosterTeam::Team1 && Rig.Fakes.Roster.Members[Layout5v5.Team0].Team == ERosterTeam::Team0);
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}

	// --- Outside Lobby: WrongPhase with no Roster call ---------------------------------------------------------------
	const ERoundPhase OtherPhases[] = { ERoundPhase::Preparing, ERoundPhase::Active, ERoundPhase::Ended, ERoundPhase::Reset };
	for (const ERoundPhase Phase : OtherPhases)
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.DriveTo(Phase);
		TestTrue(FString::Printf(TEXT("arranged phase %s"), LexRoundPhase(Phase)), Rig.Core->GetPhase() == Phase);
		const int32 Mark = Rig.Mark();
		FRoundCommandArgs Single;
		Single.Participant = FParticipantId(FRoundExitBRig::HostIdValue);
		FRoundCommandArgs Pair = Single;
		Pair.Other = FParticipantId(FRoundExitBRig::HostIdValue + 2);
		TestTrue(FString::Printf(TEXT("%s: single swap is WrongPhase"), LexRoundPhase(Phase)), Rig.Core->ExecuteHostCommand(Host, ERoundHostCommand::SwapTeam, Single) == ERoundCommandResult::WrongPhase);
		TestTrue(FString::Printf(TEXT("%s: pair exchange is WrongPhase"), LexRoundPhase(Phase)), Rig.Core->ExecuteHostCommand(Host, ERoundHostCommand::SwapTeam, Pair) == ERoundCommandResult::WrongPhase);
		TestEqual(FString::Printf(TEXT("%s: no Roster call"), LexRoundPhase(Phase)), Rig.CallsSince(Mark, /*bMutatingOnly*/ false).Num(), 0);
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), LexRoundPhase(Phase)), Rig.Log.Unexpected.IsEmpty());
	}

	// --- After the lock: a disconnect changes no count; only remove frees a Dormant slot --------------------------------
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.DriveTo(ERoundPhase::Active);
		TestTrue(TEXT("teams locked by the Start"), Rig.Core->AreTeamsLockedByRound());
		const FParticipantId Leaver(FRoundExitBRig::HostIdValue + 1);
		Rig.Fakes.Session.ConnectionStates.Add(Leaver.Value, ERoundConnectionState::Dormant);
		int32 Counts[2];
		FRoundTeamRules Rules(Rig.Fakes.Roster, &Rig.Host);
		Rules.CountTeams(Counts);
		TestEqual(TEXT("after the lock the disconnect leaves team 0 at 2"), Counts[0], Layout2v2.Team0);
		TestEqual(TEXT("after the lock the disconnect leaves team 1 at 2"), Counts[1], Layout2v2.Team1);
		TestTrue(TEXT("remove outside Lobby is WrongPhase"), Rules.RemoveParticipant(Leaver, ERoundPhase::Active) == ERoundCommandResult::WrongPhase);
		Rules.CountTeams(Counts);
		TestEqual(TEXT("the Dormant slot is still counted"), Counts[0], Layout2v2.Team0);
		Rig.Fakes.Roster.Script(TEXT("RemoveParticipant"), static_cast<int64>(ERosterResult::Applied));
		TestTrue(TEXT("remove in Lobby frees the slot"), Rules.RemoveParticipant(Leaver, ERoundPhase::Lobby) == ERoundCommandResult::Executed);
		Rules.CountTeams(Counts);
		TestEqual(TEXT("only the explicit remove decremented team 0"), Counts[0], Layout2v2.Team0 - 1);
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
