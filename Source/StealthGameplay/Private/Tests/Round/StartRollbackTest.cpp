// Story rmf-025 (production/epics/round-match-framework/story-025-start-rollback.md), GDD AC 6b / R5, R10:
//   - Session refuses at RequestRoundTravel: admission Open, teams unlocked, phase Lobby, StartRolledBack logged
//     through the Event Log writer, SessionRefused to the host sink only; RoundsPlayedCount unchanged and the next
//     Start calls ResetAll with the same index.
//   - CanRequestRoundTravel() false: none of the 6a calls happens.
//   - Two consecutive starts draw different seeds (Session's FStealthRoundSeedDrawer on the fixed fixture secret,
//     deterministic; Round never draws a seed).
//   - Reset never calls ResetAll.
// Open point (GDD R19): the Lobby-time StartRolledBack entry is appended through the IRoundLogWriter the match core
// holds (the round log of the match), here the recording writer.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Random/StealthRoundSeedDrawer.h"
#include "StealthRoundExitBTestRig.h"

namespace StartRollbackTestData
{
	/** A fake Session that draws a seed for every travel request, as Session's RequestRoundTravel does. */
	class FSeedDrawingSession : public FRoundFakeSessionGateway
	{
	public:
		FSeedDrawingSession(FRoundFakeCallLog& InLog, const IServerSubSeedSource& Source) : FRoundFakeSessionGateway(InLog), Drawer(Source) {}

		virtual ERoundTravelRequestResult RequestRoundTravel(FName MapId, FName PresetId, uint32 RoundIndex) override
		{
			Seeds.Add(Drawer.DrawRoundSeed(RoundIndex));
			return FRoundFakeSessionGateway::RequestRoundTravel(MapId, PresetId, RoundIndex);
		}

		FStealthRoundSeedDrawer Drawer;
		TArray<int32> Seeds;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundStartRollbackTest, "StealthGame.Round.StartRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundStartRollbackTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace StartRollbackTestData;

	// --- Refusal at RequestRoundTravel rolls back; two starts draw two seeds --------------------------------------
	{
		FStealthFakeEntropySource Entropy(SecretS1);
		FStealthServerSeeds Seeds;
		Seeds.Open(Entropy);
		FRoundExitBRig Rig;
		FSeedDrawingSession Session(Rig.Log, Seeds);
		FRoundGateways Gateways = Rig.Fakes.AsGateways();
		Gateways.Session = &Session;
		Rig.Core = MakeUnique<FRoundMatchCore>(Gateways, &Rig.Host, &Rig.EventLog, &Rig.Clock);
		Rig.Core->SetParams(FRoundExitBRig::DefaultParams());
		Rig.Fakes.Roster.Script(TEXT("BeginMatch"), static_cast<int64>(ERosterResult::Applied));
		Session.Script(TEXT("SetAdmissionMode"));
		Rig.Core->OpenMatch();
		Rig.Fakes.Roster.SetLayout(Layout2v2.Team0, Layout2v2.Team1, FRoundExitBRig::HostIdValue);
		for (const FRoundRosterMember& Member : Rig.Fakes.Roster.Members)
		{
			Session.ConnectionStates.Add(Member.Participant.Value, ERoundConnectionState::Connected);
		}
		const uint32 RoundsPlayed = Rig.Core->GetPhaseCore().GetRoundsPlayedCount();

		for (int32 Attempt = 0; Attempt < 2; ++Attempt)
		{
			Rig.Fakes.Tuning.Script(TEXT("PreviewPreset"), static_cast<int64>(ERoundPresetPreview::Unavailable));
			Session.Script(TEXT("CanRequestRoundTravel"), 1);
			Session.Script(TEXT("SetAdmissionMode"), 0, 2);
			Rig.Fakes.Roster.Script(TEXT("SetTeamsLocked"), static_cast<int64>(ERosterResult::Applied), 2);
			Rig.Fakes.Roster.Script(TEXT("ResetAll"), static_cast<int64>(ERosterResult::Applied));
			Session.Script(TEXT("RequestRoundTravel"), static_cast<int64>(ERoundTravelRequestResult::Refused));
			const int32 Mark = Rig.Mark();
			const int32 RolledBackBefore = Rig.EventLog.CountOfType(ERoundLogType::StartRolledBack);
			const int32 RefusalsBefore = Rig.Host.StartRefusals.Num();

			const FStartRefusal Result = Rig.Core->Start(FRoundExitBRig::HostCaller());

			TestTrue(FString::Printf(TEXT("attempt %d: SessionRefused"), Attempt), Result.Code == ERoundStartRefusalCode::SessionRefused);
			const TArray<FRoundFakeCall> Calls = Rig.CallsSince(Mark);
			if (TestEqual(FString::Printf(TEXT("attempt %d: four calls then the two rollback calls"), Attempt), Calls.Num(), 6))
			{
				TestTrue(FString::Printf(TEXT("attempt %d: ResetAll with RoundsPlayedCount + 1 (same index every attempt)"), Attempt),
					Calls[2].Call == FName(TEXT("ResetAll")) && Calls[2].Args[0] == static_cast<int64>(RoundsPlayed + 1));
				TestTrue(FString::Printf(TEXT("attempt %d: rollback admission Open"), Attempt), Calls[4].Call == FName(TEXT("SetAdmissionMode"))
					&& Calls[4].Args[0] == static_cast<int64>(ERoundAdmissionMode::Open));
				TestTrue(FString::Printf(TEXT("attempt %d: rollback unlocks teams"), Attempt), Calls[5].Call == FName(TEXT("SetTeamsLocked")) && Calls[5].Args[0] == 0);
			}
			TestTrue(FString::Printf(TEXT("attempt %d: phase Lobby"), Attempt), Rig.Core->GetPhase() == ERoundPhase::Lobby);
			TestFalse(FString::Printf(TEXT("attempt %d: teams unlocked"), Attempt), Rig.Core->AreTeamsLockedByRound());
			TestFalse(FString::Printf(TEXT("attempt %d: no travel pending"), Attempt), Rig.Core->IsTravelPending());
			TestEqual(FString::Printf(TEXT("attempt %d: StartRolledBack logged once"), Attempt), Rig.EventLog.CountOfType(ERoundLogType::StartRolledBack), RolledBackBefore + 1);
			TestTrue(FString::Printf(TEXT("attempt %d: the host sink got SessionRefused"), Attempt), Rig.Host.StartRefusals.Num() == RefusalsBefore + 1
				&& Rig.Host.StartRefusals.Last().Code == ERoundStartRefusalCode::SessionRefused);
			TestEqual(FString::Printf(TEXT("attempt %d: RoundsPlayedCount unchanged"), Attempt), Rig.Core->GetPhaseCore().GetRoundsPlayedCount(), RoundsPlayed);
		}
		if (TestEqual(TEXT("each start drew one seed"), Session.Seeds.Num(), 2))
		{
			TestTrue(TEXT("two consecutive starts draw different seeds"), Session.Seeds[0] != Session.Seeds[1]);
			TestTrue(TEXT("seeds are non-zero"), Session.Seeds[0] != 0 && Session.Seeds[1] != 0);
		}
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}

	// --- CanRequestRoundTravel() false: none of the 6a calls ----------------------------------------------------------
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.Fakes.Tuning.Script(TEXT("PreviewPreset"), static_cast<int64>(ERoundPresetPreview::Unavailable));
		Rig.Fakes.Session.Script(TEXT("CanRequestRoundTravel"), 0);
		const int32 Mark = Rig.Mark();
		const FStartRefusal Result = Rig.Core->Start(FRoundExitBRig::HostCaller());
		TestTrue(TEXT("pre-check false: SessionRefused"), Result.Code == ERoundStartRefusalCode::SessionRefused);
		TestEqual(TEXT("pre-check false: no 6a call"), Rig.CallsSince(Mark).Num(), 0);
		TestTrue(TEXT("pre-check false: phase Lobby"), Rig.Core->GetPhase() == ERoundPhase::Lobby);
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}

	// --- Reset never calls ResetAll ---------------------------------------------------------------------------------------
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.DriveTo(ERoundPhase::Ended);
		const int32 Mark = Rig.Mark();
		Rig.Clock.AdvanceSeconds(EndedMinDwellSec + 1.0);
		Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 2);
		Rig.Fakes.Roster.Script(TEXT("SetTeamsLocked"), static_cast<int64>(ERosterResult::Applied));
		TestTrue(TEXT("reset in Ended executes"), Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Reset) == ERoundCommandResult::Executed);
		Rig.Core->Tick();
		TestTrue(TEXT("Reset completed into Lobby"), Rig.Core->GetPhase() == ERoundPhase::Lobby);
		TestEqual(TEXT("Reset never calls ResetAll"), Rig.CountSince(Mark, TEXT("ResetAll")), 0);
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
