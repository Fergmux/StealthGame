// Story rmf-022 (production/epics/round-match-framework/story-022-start-validity.md), GDD AC 3 / R5, F3:
// Start in Lobby for each fixture layout: 5v3 -> TeamsInvalid(Imbalance); 3 on a team with TeamSize 2 -> Oversize
// (defensive, fake Roster); MinPlayersPerTeamCount 2 with 2v1 -> TooSmall; 1v0 -> TooSmall, accepted with
// bAllowEmptyTeamDev; 5v4, 5v5 and 2v2 (TeamSize 2) accepted; Dormant-containing AND imbalanced ->
// ParticipantNotConnected (check order). Each refusal reaches only the host sink and the phase stays Lobby with no
// gateway mutation. The full check order is the R5 list (NotHost, WrongPhase/TravelInProgress, NotConnected,
// TeamsInvalid, PresetInvalid/DeckInfeasible), asserted pairwise below.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundStartValidityTest, "StealthGame.Round.StartValidity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundStartValidityTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	struct FCase
	{
		const TCHAR* Name;
		FRoundTeamLayout Layout;
		int32 TeamSize;
		int32 MinPlayers;
		bool bAllowEmpty;
		bool bOneDormant;
		ERoundStartRefusalCode Code;
		ERoundTeamsInvalidReason Reason;
	};
	const FCase Cases[] = {
		{ TEXT("5v3"), Layout5v3, FixtureTeamSize, 1, false, false, ERoundStartRefusalCode::TeamsInvalid, ERoundTeamsInvalidReason::Imbalance },
		{ TEXT("3 on a team with TeamSize 2"), Layout3v2Oversize, SmallTeamSize, 1, false, false, ERoundStartRefusalCode::TeamsInvalid, ERoundTeamsInvalidReason::Oversize },
		{ TEXT("2v1 with MinPlayersPerTeamCount 2"), Layout2v1, FixtureTeamSize, MinPlayersTwo, false, false, ERoundStartRefusalCode::TeamsInvalid, ERoundTeamsInvalidReason::TooSmall },
		{ TEXT("1v0"), Layout1v0, FixtureTeamSize, 1, false, false, ERoundStartRefusalCode::TeamsInvalid, ERoundTeamsInvalidReason::TooSmall },
		{ TEXT("1v0 with bAllowEmptyTeamDev"), Layout1v0, FixtureTeamSize, 1, true, false, ERoundStartRefusalCode::None, ERoundTeamsInvalidReason::None },
		{ TEXT("5v4"), Layout5v4, FixtureTeamSize, 1, false, false, ERoundStartRefusalCode::None, ERoundTeamsInvalidReason::None },
		{ TEXT("5v5"), Layout5v5, FixtureTeamSize, 1, false, false, ERoundStartRefusalCode::None, ERoundTeamsInvalidReason::None },
		{ TEXT("2v2 with TeamSize 2"), Layout2v2, SmallTeamSize, 1, false, false, ERoundStartRefusalCode::None, ERoundTeamsInvalidReason::None },
		{ TEXT("Dormant and imbalanced (5v3)"), Layout5v3, FixtureTeamSize, 1, false, true, ERoundStartRefusalCode::ParticipantNotConnected, ERoundTeamsInvalidReason::None },
	};

	for (const FCase& Case : Cases)
	{
		FRoundRuleParams Params = FRoundExitBRig::DefaultParams();
		Params.TeamSize = Case.TeamSize;
		Params.MinPlayersPerTeamCount = Case.MinPlayers;
		Params.bAllowEmptyTeamDev = Case.bAllowEmpty;
		FRoundExitBRig Rig(Params);
		Rig.OpenLobby();
		Rig.SetLayout(Case.Layout.Team0, Case.Layout.Team1);
		if (Case.bOneDormant)
		{
			Rig.Fakes.Session.ConnectionStates.Add(FRoundExitBRig::HostIdValue, ERoundConnectionState::Dormant);
		}
		const bool bAccepted = Case.Code == ERoundStartRefusalCode::None;
		if (bAccepted)
		{
			Rig.ScriptStartToTravel();
		}
		else
		{
			Rig.Fakes.Tuning.Script(TEXT("PreviewPreset"), static_cast<int64>(ERoundPresetPreview::Unavailable));
		}
		const int32 Mark = Rig.Mark();

		// Act
		const FStartRefusal Result = Rig.Core->Start(FRoundExitBRig::HostCaller());

		// Assert
		TestTrue(FString::Printf(TEXT("%s: code"), Case.Name), Result.Code == Case.Code);
		TestTrue(FString::Printf(TEXT("%s: sub-code"), Case.Name), Result.TeamsReason == Case.Reason);
		if (bAccepted)
		{
			TestEqual(FString::Printf(TEXT("%s: accepted, nothing reached the host sink"), Case.Name), Rig.Host.StartRefusals.Num(), 0);
			TestTrue(FString::Printf(TEXT("%s: accepted, travel pending"), Case.Name), Rig.Core->IsTravelPending());
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s: the refusal reached the host sink once"), Case.Name), Rig.Host.StartRefusals.Num(), 1);
			TestTrue(FString::Printf(TEXT("%s: phase stays Lobby"), Case.Name), Rig.Core->GetPhase() == ERoundPhase::Lobby);
			TestEqual(FString::Printf(TEXT("%s: no gateway mutation"), Case.Name), Rig.CallsSince(Mark).Num(), 0);
		}
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Case.Name), Rig.Log.Unexpected.IsEmpty());
	}

	// The R5 order, pairwise on the pure validator: NotHost before WrongPhase; WrongPhase before NotConnected; TravelInProgress
	// before NotConnected; TeamsInvalid before PresetInvalid.
	{
		FRoundStartInputs Inputs;
		Inputs.Params = FRoundExitBRig::DefaultParams();
		Inputs.Members.Add({ FParticipantId(1), ERosterTeam::Team0, ERoundConnectionState::Dormant });
		Inputs.Phase = ERoundPhase::Active;
		Inputs.bIsHost = false;
		TestTrue(TEXT("order: NotHost first"), FRoundStartValidator::Validate(Inputs).Code == ERoundStartRefusalCode::NotHost);
		Inputs.bIsHost = true;
		TestTrue(TEXT("order: WrongPhase before NotConnected"), FRoundStartValidator::Validate(Inputs).Code == ERoundStartRefusalCode::WrongPhase);
		Inputs.Phase = ERoundPhase::Lobby;
		Inputs.bTravelInProgress = true;
		TestTrue(TEXT("order: TravelInProgress before NotConnected"), FRoundStartValidator::Validate(Inputs).Code == ERoundStartRefusalCode::TravelInProgress);
		Inputs.bTravelInProgress = false;
		Inputs.Members[0].Connection = ERoundConnectionState::Connected;
		Inputs.Members.Add({ FParticipantId(2), ERosterTeam::Team0, ERoundConnectionState::Connected });
		Inputs.Members.Add({ FParticipantId(3), ERosterTeam::Team0, ERoundConnectionState::Connected });
		Inputs.PresetPreview = ERoundPresetPreview::Invalid;
		TestTrue(TEXT("order: TeamsInvalid before PresetInvalid"), FRoundStartValidator::Validate(Inputs).Code == ERoundStartRefusalCode::TeamsInvalid);
		Inputs.Members[2].Team = ERosterTeam::Team1;
		TestTrue(TEXT("order: PresetInvalid last"), FRoundStartValidator::Validate(Inputs).Code == ERoundStartRefusalCode::PresetInvalid);
		Inputs.PresetPreview = ERoundPresetPreview::Unavailable;
		TestFalse(TEXT("an Unavailable dry run is skipped"), FRoundStartValidator::Validate(Inputs).IsRefused());
	}
	return true;
}

#endif
