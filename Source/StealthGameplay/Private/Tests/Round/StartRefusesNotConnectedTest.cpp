// Story rmf-023 (production/epics/round-match-framework/story-023-start-refuses-not-connected.md), GDD AC 4 / R5
// check 3: with one participant each Dormant, Unresponsive or Connecting (and all three at once), Start refuses with
// ParticipantNotConnected whose ParticipantIds equal exactly the offending set; the roster count is unchanged (no
// prune), the phase stays Lobby and no admission or lock call is made. Layouts are otherwise valid (2v2).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundStartRefusesNotConnectedTest, "StealthGame.Round.StartRefusesNotConnected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundStartRefusesNotConnectedTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	struct FOffender
	{
		uint32 Id;
		ERoundConnectionState State;
	};
	struct FCase
	{
		const TCHAR* Name;
		TArray<FOffender> Offenders;
	};
	const FCase Cases[] = {
		{ TEXT("Dormant"), { { 2u, ERoundConnectionState::Dormant } } },
		{ TEXT("Unresponsive"), { { 3u, ERoundConnectionState::Unresponsive } } },
		{ TEXT("Connecting"), { { 4u, ERoundConnectionState::Connecting } } },
		{ TEXT("all three"), { { 2u, ERoundConnectionState::Dormant }, { 3u, ERoundConnectionState::Unresponsive }, { 4u, ERoundConnectionState::Connecting } } },
	};

	for (const FCase& Case : Cases)
	{
		// Arrange
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		TSet<uint32> Expected;
		for (const FOffender& Offender : Case.Offenders)
		{
			Rig.Fakes.Session.ConnectionStates.Add(Offender.Id, Offender.State);
			Expected.Add(Offender.Id);
		}
		Rig.Fakes.Tuning.Script(TEXT("PreviewPreset"), static_cast<int64>(ERoundPresetPreview::Unavailable));
		const int32 RosterBefore = Rig.Fakes.Roster.Members.Num();
		const int32 Mark = Rig.Mark();

		// Act
		const FStartRefusal Result = Rig.Core->Start(FRoundExitBRig::HostCaller());

		// Assert
		TestTrue(FString::Printf(TEXT("%s: ParticipantNotConnected"), Case.Name), Result.Code == ERoundStartRefusalCode::ParticipantNotConnected);
		TSet<uint32> Got;
		for (const FParticipantId& Id : Result.ParticipantIds)
		{
			Got.Add(Id.Value);
		}
		TestEqual(FString::Printf(TEXT("%s: as many ids as offenders"), Case.Name), Result.ParticipantIds.Num(), Expected.Num());
		TestTrue(FString::Printf(TEXT("%s: ParticipantIds equal exactly the offending set"), Case.Name), Got.Num() == Expected.Num() && Got.Includes(Expected));
		TestEqual(FString::Printf(TEXT("%s: roster count unchanged"), Case.Name), Rig.Fakes.Roster.Members.Num(), RosterBefore);
		TestEqual(FString::Printf(TEXT("%s: no RemoveParticipant (no prune)"), Case.Name), Rig.CountSince(Mark, TEXT("RemoveParticipant")), 0);
		TestEqual(FString::Printf(TEXT("%s: no admission or lock call"), Case.Name), Rig.CallsSince(Mark).Num(), 0);
		TestTrue(FString::Printf(TEXT("%s: phase stays Lobby"), Case.Name), Rig.Core->GetPhase() == ERoundPhase::Lobby);
		TestTrue(FString::Printf(TEXT("%s: the refusal reached only the host sink"), Case.Name), Rig.Host.StartRefusals.Num() == 1);
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Case.Name), Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
