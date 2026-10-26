// Story rmf-030 (production/epics/round-match-framework/story-030-match-scope-survives-travel.md), GDD AC 58 / R21, R4, R5, R10:
//   GIVEN two rounds, MatchScore, RoundsPlayedCount, the host id, teams and the archived Event Log slices persist across
//   the travel between them, while phase, Seq and Terminated reset, and RoundEpoch increases by one per ResetAll.
// Both rounds run through the production rules (Preparing readiness sequence, the real queue, the latch): each is ended
// by stealth.round.debugwin through the queue, then reset by the host.
// Representativeness (ADR-0005 guideline 10): no Session travel exists yet (the same world hosts every round), so "the
// travel" is the Reset -> Lobby hand-over plus a GameState recreated in-process (the InitGameState re-seed path), not a
// seamless ServerTravel in multi-process standalone. Non-representative for ADR-0005 S3; re-run when Session travel lands.
// CQTest exemption: NETWORK_TEST_CLASS(MatchScopeSurvivesTravel, "StealthGame.Round.PIE").

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/Roster/StealthRosterStore.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "StealthSettings.h"

namespace MatchScopeSurvivesTravelTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	constexpr int32 ClientCount = 2;
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("300");
	/** Ended dwell (R9) plus margin, in server seconds. */
	constexpr double DwellWaitSeconds = 3.5;

	/** The match-scoped values that must survive the travel. */
	struct FMatchScope
	{
		int32 Score[2] = { 0, 0 };
		uint32 RoundsPlayed = 0;
		uint32 HostId = 0;
		TMap<uint32, uint8> Teams;
		int32 ArchivedSlices = 0;
		uint32 RoundEpoch = 0;
	};

	FMatchScope Capture(UWorld* World)
	{
		FMatchScope Out;
		const URoundMatchStore* Round = URoundMatchStore::Get(World);
		const URosterStore* Roster = URosterStore::Get(World);
		if (Round == nullptr || Round->GetMatchCore() == nullptr || Roster == nullptr)
		{
			return Out;
		}
		const FRoundPhaseCore& Phase = Round->GetMatchCore()->GetPhaseCore();
		Out.Score[0] = Phase.GetMatchScore(0);
		Out.Score[1] = Phase.GetMatchScore(1);
		Out.RoundsPlayed = Phase.GetRoundsPlayedCount();
		Out.HostId = Round->GetHostParticipant().Value;
		TArray<FParticipantId> Ids;
		Roster->GetParticipantIds(Ids);
		for (const FParticipantId& Id : Ids)
		{
			if (const FParticipantRecord* Record = Roster->FindRecord(Id))
			{
				Out.Teams.Add(Id.Value, static_cast<uint8>(Record->Team));
			}
		}
		Out.ArchivedSlices = Round->GetMatchCore()->GetLogArchive().GetArchive().Num();
		Out.RoundEpoch = Roster->GetEpochToken().RoundEpoch;
		return Out;
	}
}

NETWORK_TEST_CLASS(MatchScopeSurvivesTravel, "StealthGame.Round.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };

	inline static FString PreviousNetworkTimeout;
	inline static bool bPreviousAutoStart = true;

	MatchScopeSurvivesTravelTestData::FMatchScope Lobby0;
	MatchScopeSurvivesTravelTestData::FMatchScope Ended1;
	MatchScopeSurvivesTravelTestData::FMatchScope Ended2;
	double EndedAt = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(MatchScopeSurvivesTravelTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		bPreviousAutoStart = UStealthSettings::Get()->bAutoStartRound;
		GetMutableDefault<UStealthSettings>()->bAutoStartRound = false;
	}

	AFTER_ALL()
	{
		GetMutableDefault<UStealthSettings>()->bAutoStartRound = bPreviousAutoStart;
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			if (!PreviousNetworkTimeout.IsEmpty())
			{
				Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
			}
		}
	}

	BEFORE_EACH()
	{
		using namespace MatchScopeSurvivesTravelTestData;
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(ClientCount)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(FixtureMap);
	}

	/** Start, wait for Active, debugwin Team through the queue, wait for Ended. */
	void PlayRound(int32 Team, MatchScopeSurvivesTravelTestData::FMatchScope& OutEnded)
	{
		Network
			.ThenServer(TEXT("host start"), [this](FBasePIENetworkComponentState& Server)
			{
				URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				ASSERT_THAT(IsTrue(Round->ExecuteConsoleCommand(Server.World, ERoundHostCommand::Start, FRoundCommandArgs()) == ERoundCommandResult::Executed,
					TEXT("host start accepted")));
			})
			.UntilServer(TEXT("the production Preparing sequence reaches Active"), [](FBasePIENetworkComponentState& Server)
			{
				const URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				return Round != nullptr && Round->GetPhase() == ERoundPhase::Active;
			})
			.ThenServer(TEXT("debugwin through the queue"), [this, Team](FBasePIENetworkComponentState& Server)
			{
				URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				FRoundCommandArgs Args;
				Args.Team = Team;
				ASSERT_THAT(IsTrue(Round->ExecuteConsoleCommand(Server.World, ERoundHostCommand::DebugWin, Args) == ERoundCommandResult::Executed, TEXT("debugwin submitted")));
			})
			.UntilServer(TEXT("Ended"), [](FBasePIENetworkComponentState& Server)
			{
				const URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				return Round != nullptr && Round->GetPhase() == ERoundPhase::Ended;
			})
			.ThenServer(TEXT("capture at Ended"), [this, &OutEnded](FBasePIENetworkComponentState& Server)
			{
				OutEnded = MatchScopeSurvivesTravelTestData::Capture(Server.World);
				EndedAt = Server.World->GetTimeSeconds();
				ASSERT_THAT(IsTrue(URoundMatchStore::Get(Server.World)->GetMatchCore()->IsTerminated(), TEXT("the round latched")));
			})
			.UntilServer(TEXT("the Ended dwell"), [this](FBasePIENetworkComponentState& Server) { return Server.World->GetTimeSeconds() - EndedAt >= MatchScopeSurvivesTravelTestData::DwellWaitSeconds; })
			.ThenServer(TEXT("host reset"), [this](FBasePIENetworkComponentState& Server)
			{
				URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				ASSERT_THAT(IsTrue(Round->ExecuteConsoleCommand(Server.World, ERoundHostCommand::Reset, FRoundCommandArgs()) == ERoundCommandResult::Executed, TEXT("reset after the dwell")));
			})
			.UntilServer(TEXT("back in Lobby"), [](FBasePIENetworkComponentState& Server)
			{
				const URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				return Round != nullptr && Round->GetPhase() == ERoundPhase::Lobby;
			});
	}

	TEST_METHOD(TwoRoundsKeepMatchScopeAndResetRoundScope)
	{
		using namespace MatchScopeSurvivesTravelTestData;
		Network
			.UntilServer(TEXT("Lobby with every player registered"), [](FBasePIENetworkComponentState& Server)
			{
				const URosterStore* Roster = URosterStore::Get(Server.World);
				TArray<FParticipantId> Ids;
				if (Roster != nullptr && Roster->IsMatchOpen())
				{
					Roster->GetParticipantIds(Ids);
				}
				const URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				return Round != nullptr && Round->GetPhase() == ERoundPhase::Lobby && Ids.Num() == ClientCount + 1;
			})
			.ThenServer(TEXT("capture the lobby"), [this](FBasePIENetworkComponentState& Server) { Lobby0 = Capture(Server.World); });

		PlayRound(1, Ended1);

		Network.ThenServer(TEXT("round 1 -> travel -> lobby: match scope kept, round scope reset"), [this](FBasePIENetworkComponentState& Server)
		{
			URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
			const FRoundMatchCore* Core = Round->GetMatchCore();
			ASSERT_THAT(IsTrue(Ended1.Score[1] == 1 && Ended1.RoundsPlayed == 1, TEXT("round 1 scored for team 1")));
			ASSERT_THAT(AreEqual(Lobby0.RoundEpoch + 1, Ended1.RoundEpoch, TEXT("RoundEpoch + 1 for the first ResetAll")));

			// The travel stand-in: a GameState created now is re-seeded whole from the game-instance store.
			UWorld* Scratch = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MatchScopeReseedWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(Scratch);
			Scratch->InitializeActorsForPlay(FURL());
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStealthGameState* Fresh = Scratch->SpawnActor<AStealthGameState>(Params);
			Round->RegisterPublicStateSink(Fresh, Fresh);
			const FRoundPublicState Reseeded = Fresh->GetRoundPublicState();
			Round->RegisterPublicStateSink(Server.World->GetGameState<AStealthGameState>(), Server.World->GetGameState<AStealthGameState>());
			GEngine->DestroyWorldContext(Scratch);
			Scratch->DestroyWorld(false);

			const FMatchScope AfterTravel = Capture(Server.World);
			ASSERT_THAT(IsTrue(AfterTravel.Score[0] == Ended1.Score[0] && AfterTravel.Score[1] == Ended1.Score[1], TEXT("MatchScore persists")));
			ASSERT_THAT(AreEqual(Ended1.RoundsPlayed, AfterTravel.RoundsPlayed, TEXT("RoundsPlayedCount persists")));
			ASSERT_THAT(AreEqual(Ended1.HostId, AfterTravel.HostId, TEXT("the host id persists")));
			ASSERT_THAT(IsTrue(AfterTravel.Teams.OrderIndependentCompareEqual(Ended1.Teams), TEXT("teams persist")));
			ASSERT_THAT(AreEqual(1, AfterTravel.ArchivedSlices, TEXT("round 1's slice is archived (server only)")));
			ASSERT_THAT(IsTrue(Reseeded.MatchScore[1] == 1 && Reseeded.Phase == ERoundPhase::Lobby && Reseeded.LastResult.RoundIndex == 1,
				TEXT("a recreated GameState reads the persisted score, the Lobby and LastResult from the store")));
			ASSERT_THAT(IsTrue(!Core->IsTerminated(), TEXT("Terminated reset")));
			ASSERT_THAT(IsTrue(Round->GetRoundQueue() == nullptr || Round->GetRoundQueue()->PeekNextSeq() == 1, TEXT("Seq reset (restarts at 1)")));
			ASSERT_THAT(AreEqual(Ended1.RoundEpoch, AfterTravel.RoundEpoch, TEXT("Reset never calls ResetAll (the epoch is unchanged)")));
		});

		PlayRound(0, Ended2);

		Network.ThenServer(TEXT("round 2: the match scope accumulated, the epoch advanced once more"), [this](FBasePIENetworkComponentState& Server)
		{
			const FMatchScope AfterTravel = Capture(Server.World);
			ASSERT_THAT(IsTrue(Ended2.Score[0] == 1 && Ended2.Score[1] == 1, TEXT("MatchScore carried across the travel and added round 2")));
			ASSERT_THAT(AreEqual(2u, Ended2.RoundsPlayed, TEXT("RoundsPlayedCount 2")));
			ASSERT_THAT(AreEqual(Lobby0.HostId, Ended2.HostId, TEXT("the host id is kept")));
			ASSERT_THAT(IsTrue(Lobby0.Teams.OrderIndependentCompareEqual(Ended2.Teams), TEXT("teams are kept")));
			ASSERT_THAT(AreEqual(Lobby0.RoundEpoch + 2, Ended2.RoundEpoch, TEXT("RoundEpoch + 1 per ResetAll (two starts)")));
			ASSERT_THAT(AreEqual(2, AfterTravel.ArchivedSlices, TEXT("both rounds' slices are archived")));
		});
	}
};

#endif
