// Story rmf-029 (production/epics/round-match-framework/story-029-public-state-replication.md), GDD AC 57 / R20:
//   - a joiner whose first replicated Phase arrives runs the handler (class default Unknown = 0, never a real phase);
//   - the host runs the same handler explicitly after every write (a server receives no RepNotify);
//   - a GameState created after the round started (the travel re-seed path: Round writes the whole struct from the
//     game-instance store in InitGameState) reads the store's phase, never the default; with the store in Preparing
//     it reads Preparing;
//   - PhaseChangeCount strictly increases as seen by the client;
//   - LastResult survives Reset into the lobby.
// Representativeness (ADR-0005 guideline 10): no Session travel exists yet, so the travel half is exercised by
// recreating the GameState in-process (the InitGameState re-seed), not by a seamless ServerTravel; the multi-process
// travel run waits for the Session epic. CQTest exemption: NETWORK_TEST_CLASS(PublicStateReplication,
// "StealthGame.Round.PIE"); GDD name StealthGame.Round.PublicStateReplication.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/Testing/StealthRoundFakeGateways.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "StealthSettings.h"

namespace PublicStateReplicationTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	constexpr int32 ClientCount = 2;
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("240");
	/** The winning team and public reason the test ends the round with. */
	constexpr uint8 WinningTeam = 1;
	constexpr uint8 ObjectivePublicReason = 1;
	/** Ended dwell (R9) plus margin, in server seconds. */
	constexpr double DwellWaitSeconds = 3.5;
}

NETWORK_TEST_CLASS(PublicStateReplication, "StealthGame.Round.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };

	inline static FString PreviousNetworkTimeout;
	inline static bool bPreviousAutoStart = true;

	uint32 ClientLobbyChanges = 0;
	uint32 ClientActiveChanges = 0;
	int32 ServerHandlerCallsBefore = 0;
	uint32 ServerWritesBefore = 0;
	double EndedAt = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(PublicStateReplicationTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
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
		using namespace PublicStateReplicationTestData;
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(ClientCount)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(FixtureMap);
	}

	TEST_METHOD(PhaseReplicatesReseedsAndKeepsLastResult)
	{
		using namespace PublicStateReplicationTestData;
		Network
			.UntilClients(TEXT("joiners received Lobby"), [](FBasePIENetworkComponentState& Client)
			{
				const AStealthGameState* GS = Client.World ? Client.World->GetGameState<AStealthGameState>() : nullptr;
				return GS != nullptr && GS->GetRoundPublicState().Phase == ERoundPhase::Lobby;
			})
			.ThenClients(TEXT("joiner ran the handler for its first value"), [this](FBasePIENetworkComponentState& Client)
			{
				const AStealthGameState* GS = Client.World->GetGameState<AStealthGameState>();
				ASSERT_THAT(IsTrue(GS->GetRoundPublicStateHandlerCalls() >= 1, TEXT("the late joiner's first Phase ran the handler")));
				ASSERT_THAT(IsTrue(GS->GetPreviousRoundPublicState().Phase == ERoundPhase::Lobby, TEXT("the handler saw Lobby, not the class default")));
				ASSERT_THAT(AreEqual(static_cast<int32>(ERoundPhase::Unknown), static_cast<int32>(GetDefault<AStealthGameState>()->GetRoundPublicState().Phase),
					TEXT("the class default Phase is the explicit Unknown (0)")));
				ClientLobbyChanges = GS->GetRoundPublicState().PhaseChangeCount;
			})
			.ThenServer(TEXT("host handler runs explicitly; start the round"), [this](FBasePIENetworkComponentState& Server)
			{
				AStealthGameState* GS = Server.World->GetGameState<AStealthGameState>();
				URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				ASSERT_THAT(IsNotNull(Round, TEXT("server Round store")));
				ASSERT_THAT(IsTrue(GS->GetRoundPublicStateHandlerCalls() >= 1, TEXT("the host ran the handler (no RepNotify on a server)")));
				ServerHandlerCallsBefore = GS->GetRoundPublicStateHandlerCalls();
				ServerWritesBefore = GS->GetRoundPublicState().PhaseChangeCount;
				ASSERT_THAT(IsTrue(Round->ExecuteConsoleCommand(Server.World, ERoundHostCommand::Start, FRoundCommandArgs()) == ERoundCommandResult::Executed,
					TEXT("host start accepted")));
			})
			.UntilServer(TEXT("Active"), [](FBasePIENetworkComponentState& Server)
			{
				const URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				return Round != nullptr && Round->GetPhase() == ERoundPhase::Active;
			})
			.ThenServer(TEXT("host handler ran once per write; a recreated GameState is re-seeded"), [this](FBasePIENetworkComponentState& Server)
			{
				AStealthGameState* GS = Server.World->GetGameState<AStealthGameState>();
				const uint32 Writes = GS->GetRoundPublicState().PhaseChangeCount - ServerWritesBefore;
				ASSERT_THAT(IsTrue(Writes >= 2, TEXT("Lobby -> Preparing -> Active wrote at least twice")));
				ASSERT_THAT(IsTrue(GS->GetRoundPublicStateHandlerCalls() - ServerHandlerCallsBefore >= static_cast<int32>(Writes),
					TEXT("the host handler ran for every phase write")));

				// The travel re-seed path: a GameState created now (here in a scratch world, so the PIE worlds keep their own)
				// is written whole from the store, as InitGameState does after a travel.
				UWorld* Scratch = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PublicStateReseedWorld"));
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(Scratch);
				Scratch->InitializeActorsForPlay(FURL());
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AStealthGameState* Fresh = Scratch->SpawnActor<AStealthGameState>(Params);
				URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				Round->RegisterPublicStateSink(Fresh, Fresh);
				const bool bReseededActive = Fresh->GetRoundPublicState().Phase == ERoundPhase::Active;
				const bool bSameCount = GS->GetRoundPublicState().PhaseChangeCount == Fresh->GetRoundPublicState().PhaseChangeCount;
				Round->RegisterPublicStateSink(GS, GS);

				// The Preparing case: a phase core in Preparing re-seeds a new GameState with Preparing.
				FRoundFakeCallLog Log;
				FRoundFakeGatewaySet Fakes(Log);
				Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 2);
				FRoundPhaseCore Core(&Fakes.Session);
				Core.RequestPhase(ERoundPhase::Lobby, 1.0);
				Core.RequestPhase(ERoundPhase::Preparing, 2.0);
				AStealthGameState* Travelled = Scratch->SpawnActor<AStealthGameState>(Params);
				FRoundPublicStateMirror Mirror;
				Mirror.RegisterSink(Travelled, Travelled, Core);
				const bool bReseededPreparing = Travelled->GetRoundPublicState().Phase == ERoundPhase::Preparing;

				GEngine->DestroyWorldContext(Scratch);
				Scratch->DestroyWorld(false);
				ASSERT_THAT(IsTrue(bReseededActive, TEXT("a re-seeded GameState reads the store's phase, never Unknown")));
				ASSERT_THAT(IsTrue(bSameCount, TEXT("the re-seed carries the monotonic PhaseChangeCount")));
				ASSERT_THAT(IsTrue(bReseededPreparing, TEXT("after the travel re-seed the new GameState reads Preparing")));
			})
			.UntilClients(TEXT("clients see Active"), [](FBasePIENetworkComponentState& Client)
			{
				const AStealthGameState* GS = Client.World->GetGameState<AStealthGameState>();
				return GS != nullptr && GS->GetRoundPublicState().Phase == ERoundPhase::Active;
			})
			.ThenClients(TEXT("PhaseChangeCount strictly increased"), [this](FBasePIENetworkComponentState& Client)
			{
				const AStealthGameState* GS = Client.World->GetGameState<AStealthGameState>();
				ClientActiveChanges = GS->GetRoundPublicState().PhaseChangeCount;
				ASSERT_THAT(IsTrue(ClientActiveChanges > ClientLobbyChanges, TEXT("PhaseChangeCount strictly increases")));
			})
			.ThenServer(TEXT("end the round and ask for the reset"), [this](FBasePIENetworkComponentState& Server)
			{
				URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				ASSERT_THAT(IsTrue(Round->EndRound(WinningTeam, ObjectivePublicReason), TEXT("Active -> Ended")));
				EndedAt = Server.World->GetTimeSeconds();
			})
			.UntilServer(TEXT("Ended dwell"), [this](FBasePIENetworkComponentState& Server) { return Server.World->GetTimeSeconds() >= EndedAt + DwellWaitSeconds; })
			.ThenServer(TEXT("reset"), [this](FBasePIENetworkComponentState& Server)
			{
				URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				ASSERT_THAT(IsTrue(Round->ExecuteConsoleCommand(Server.World, ERoundHostCommand::Reset, FRoundCommandArgs()) == ERoundCommandResult::Executed,
					TEXT("reset after the dwell executes")));
			})
			.UntilClients(TEXT("clients back in Lobby"), [](FBasePIENetworkComponentState& Client)
			{
				const AStealthGameState* GS = Client.World->GetGameState<AStealthGameState>();
				return GS != nullptr && GS->GetRoundPublicState().Phase == ERoundPhase::Lobby;
			})
			.ThenClients(TEXT("LastResult survived into the lobby"), [this](FBasePIENetworkComponentState& Client)
			{
				const FRoundPublicState& State = Client.World->GetGameState<AStealthGameState>()->GetRoundPublicState();
				ASSERT_THAT(AreEqual(static_cast<int32>(WinningTeam), static_cast<int32>(State.LastResult.WinningTeam), TEXT("LastResult.WinningTeam kept in Lobby")));
				ASSERT_THAT(AreEqual(1u, State.LastResult.RoundIndex, TEXT("LastResult belongs to round 1")));
				ASSERT_THAT(IsTrue(State.PhaseChangeCount > ClientActiveChanges, TEXT("PhaseChangeCount kept increasing through Ended and Reset")));
			});
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS && ENABLE_PIE_NETWORK_TEST
