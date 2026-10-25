// Story tdc-019 (production/epics/tuning-data-config/story-019-cqtest-headless-proof.md):
// ADR-0008 Verification Required (1). A CQTest in-process PIE network test (T2) with a listen server
// and three clients, AStealthGameMode and the fixture map, run headless under
// `UnrealEditor-Cmd -nullrhi`. Full name: StealthGame.Harness.PIE.ListenServerThreeClients.<Method>.
//
// Bounds (ADR-0008 section 4): every wait is a condition or a frame count; wall-clock time appears
// only as the failure ceiling TestFramework.CQTest.CommandTimeout.Network, raised for this class to
// a generous value so a hung step fails instead of hanging the run. No assertion depends on how much
// wall time passed.
//
// Fixture map: the stock component starts PIE on a new empty map; TStealthPIENetworkComponent
// (StealthPIENetworkComponent.h) queues the same steps with "load fixture map" instead.

#include "CoreMinimal.h"
#include "StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"

namespace ListenServerThreeClientsTestData
{
	constexpr int32 ClientCount = 3;
	/** Fixture map (decided in story tdc-019: the only map in Content/StealthGame/Maps/). */
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	/** Frames every world must tick after the clients are ready, before the assertions. */
	constexpr uint64 SettleFrames = 30;
	/** Failure ceiling only (seconds of wall clock per CQTest step); never a pass/fail predicate. */
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("180");
}

NETWORK_TEST_CLASS(ListenServerThreeClients, "StealthGame.Harness.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };

	/** GFrameCounter when the clients were ready (single process: every world ticks once per engine frame). */
	uint64 ReadyFrame = 0;

	inline static FString PreviousNetworkTimeout;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(ListenServerThreeClientsTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
	}

	AFTER_ALL()
	{
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
		using namespace ListenServerThreeClientsTestData;

		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(ClientCount)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);

		Network.StartOnFixtureMap(FixtureMap)
			.Then(TEXT("Record ready frame"), [this]() { ReadyFrame = GFrameCounter; });
	}

	TEST_METHOD(OneServerAndThreeClientWorldsAreUp)
	{
		using namespace ListenServerThreeClientsTestData;

		Network
			.UntilServer(TEXT("Settle frames"), [this](FBasePIENetworkComponentState&) { return GFrameCounter >= ReadyFrame + SettleFrames; })
			.ThenServer(TEXT("Server world"), [this](FBasePIENetworkComponentState& Server)
			{
				ASSERT_THAT(IsNotNull(Server.World, TEXT("server world")));
				ASSERT_THAT(AreEqual(static_cast<int32>(NM_ListenServer), static_cast<int32>(Server.World->GetNetMode()), TEXT("server net mode is listen server")));
				ASSERT_THAT(IsTrue(Server.World->GetAuthGameMode() != nullptr && Server.World->GetAuthGameMode()->IsA<AStealthGameMode>(), TEXT("server game mode is AStealthGameMode")));
				const FString MapName = UWorld::RemovePIEPrefix(Server.World->GetOutermost()->GetName());
				ASSERT_THAT(AreEqual(FString(FixtureMap), MapName, TEXT("server world runs the fixture map")));
				ASSERT_THAT(AreEqual(ClientCount, Server.World->GetNetDriver()->ClientConnections.Num(), TEXT("server has three client connections")));
				for (const UNetConnection* Connection : Server.ClientConnections)
				{
					ASSERT_THAT(IsNotNull(Connection, TEXT("each client is matched to a server connection")));
				}
			})
			.UntilClients(TEXT("Client game state replicated"), [](FBasePIENetworkComponentState& Client)
			{
				return Client.World != nullptr && Client.World->GetGameState() != nullptr;
			})
			.ThenClients(TEXT("Client worlds"), [this](FBasePIENetworkComponentState& Client)
			{
				ASSERT_THAT(IsNotNull(Client.World, TEXT("client world")));
				ASSERT_THAT(AreEqual(static_cast<int32>(NM_Client), static_cast<int32>(Client.World->GetNetMode()), TEXT("client net mode")));
				ASSERT_THAT(IsNotNull(Client.World->GetNetDriver()->ServerConnection.Get(), TEXT("client has a server connection")));
				ASSERT_THAT(IsTrue(Client.World->GetGameState()->IsA<AStealthGameState>(), TEXT("client received the replicated AStealthGameState")));
			})
			.Then(TEXT("World census"), [this]()
			{
				int32 ServerWorlds = 0;
				TSet<const UWorld*> ClientWorlds;
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					const UWorld* World = Context.World();
					if (Context.WorldType != EWorldType::PIE || World == nullptr)
					{
						continue;
					}
					if (World->GetNetMode() == NM_ListenServer)
					{
						++ServerWorlds;
					}
					else if (World->GetNetMode() == NM_Client)
					{
						ClientWorlds.Add(World);
					}
				}
				ASSERT_THAT(AreEqual(1, ServerWorlds, TEXT("exactly one listen-server PIE world")));
				ASSERT_THAT(AreEqual(ClientCount, ClientWorlds.Num(), TEXT("exactly three distinct client PIE worlds")));
			});
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS && ENABLE_PIE_NETWORK_TEST
