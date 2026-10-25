// Story production/epics/session-travel/story-001-harness-connection-event-hook.md (TR-session-006):
// the PIE harness hook forces (a) a drop, (b) a same-key rejoin, (c) a supersede and (d) a host kick of a
// chosen client in a listen server with three clients, through the real net driver and the engine's own
// login, leave and kick paths, with no edit to the game code under test (AStealthGameMode, the fixture map).
// Full name: StealthGame.Session.PIE.ConnectionEventHook.<Method>.
//
// Bounds (ADR-0008 section 4): every wait is a condition on server/client state or a frame count; wall-clock
// time appears only as the failure ceiling of each wait, never as a pass/fail predicate.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Session/StealthConnectionEventHook.h"
#include "StealthGameMode.h"

namespace ConnectionEventHookTestData
{
	constexpr int32 ClientCount = 3;
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	/** Failure ceilings only (wall clock per wait); never a pass/fail predicate. */
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("240");
	const FTimespan StepCeiling = FTimespan::FromSeconds(120.0);
	/** Frames the silent client must stay silent before the server-side check (frame count, not time). */
	constexpr uint64 SilentFrames = 60;
	/** Frames ignored after the silence starts (packets already on the wire may still land). */
	constexpr uint64 SilentSettleFrames = 10;
	using EKind = FStealthConnectionObservation::EKind;

	/** True when the client of Handle is connected and its controller exists on both ends. */
	static bool IsJoined(const FStealthConnectionEventHook& Hook, FName Handle, const FString& Key)
	{
		const TArray<UNetConnection*> Live = Hook.LiveConnectionsWithKey(Key);
		const UWorld* ClientWorld = FStealthConnectionEventHook::WorldOf(Handle);
		return FStealthConnectionEventHook::IsClientConnected(Handle) && ClientWorld && ClientWorld->GetFirstPlayerController() != nullptr
			&& Live.ContainsByPredicate([](const UNetConnection* C) { return C->PlayerController && C->PlayerController->HasActorBegunPlay(); });
	}

	/** True when the client of Handle has left the server and finished loading its own (standalone) world. */
	static bool IsOffline(FName Handle)
	{
		const UWorld* World = FStealthConnectionEventHook::WorldOf(Handle);
		return World && World->GetNetMode() == NM_Standalone && World->HasBegunPlay();
	}
}

NETWORK_TEST_CLASS(ConnectionEventHook, "StealthGame.Session.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };

	TUniquePtr<FStealthConnectionEventHook> Hook;
	TArray<FName> Clients;
	TArray<FString> Keys;
	TWeakObjectPtr<UNetConnection> FirstKeyedC1;
	TWeakObjectPtr<UNetConnection> SilentConnection;
	double SilentLastReceive = 0.0;
	uint64 StepFrame = 0;

	inline static FString PreviousNetworkTimeout;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(ConnectionEventHookTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
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
		using namespace ConnectionEventHookTestData;
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(ClientCount)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(FixtureMap);
	}

	AFTER_EACH()
	{
		Hook.Reset();
	}

	TEST_METHOD(DropRejoinSupersedeAndKickEachProduceTheirEvent)
	{
		using namespace ConnectionEventHookTestData;

		Network.ThenServer(TEXT("Install the hook; name the clients by world context"), [this](FBasePIENetworkComponentState& Server)
		{
			// A kicked or server-closed client logs the engine's own connection-lost error: expected here (0 = at least once).
			TestRunner->AddExpectedError(TEXT("BroadcastNetworkFailure"), EAutomationExpectedErrorFlags::Contains, 0);
			Hook = MakeUnique<FStealthConnectionEventHook>(Server.World);
			TArray<TPair<int32, FName>> ByInstance;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Client)
				{
					ByInstance.Emplace(Context.PIEInstance, Context.ContextHandle);
				}
			}
			ByInstance.Sort([](const TPair<int32, FName>& A, const TPair<int32, FName>& B) { return A.Key < B.Key; });
			for (const TPair<int32, FName>& Pair : ByInstance)
			{
				Clients.Add(Pair.Value);
				Keys.Add(FStealthConnectionEventHook::IdentityKeyForSlot(Clients.Num()));
			}
			ASSERT_THAT(AreEqual(ClientCount, Clients.Num(), TEXT("three client world contexts")));
			ASSERT_THAT(IsTrue(Keys[0] != Keys[1] && Keys[1] != Keys[2] && Keys[0] != Keys[2], TEXT("distinct identity keys per slot")));
		});

		Network
			.Then(TEXT("Every client re-joins with its own IdentityKey"), [this]()
			{
				for (int32 Index = 0; Index < Clients.Num(); ++Index)
				{
					FString Error;
					ASSERT_THAT(IsTrue(FStealthConnectionEventHook::Rejoin(Clients[Index], Keys[Index], FString::Printf(TEXT("C%d"), Index + 1), FString(), &Error), *Error));
				}
			})
			.Until(TEXT("Keyed connections joined"), [this]()
			{
				return IsJoined(*Hook, Clients[0], Keys[0]) && IsJoined(*Hook, Clients[1], Keys[1]) && IsJoined(*Hook, Clients[2], Keys[2]);
			}, StepCeiling)
			.Then(TEXT("Each key has exactly one live connection; the unkeyed ones logged out"), [this]()
			{
				for (int32 Index = 0; Index < Clients.Num(); ++Index)
				{
					ASSERT_THAT(AreEqual(1, Hook->LiveConnectionsWithKey(Keys[Index]).Num(), TEXT("one live connection per key")));
					ASSERT_THAT(AreEqual(1, Hook->CountObservations(EKind::Login, Keys[Index]), TEXT("one keyed login per client")));
				}
				ASSERT_THAT(AreEqual(ClientCount, Hook->CountObservations(EKind::Logout, FString()), TEXT("the three unkeyed PIE connections logged out")));
				FirstKeyedC1 = Hook->LiveConnectionsWithKey(Keys[0])[0];
			})
			// (a) Drop: client 1 leaves through the engine's own disconnect path.
			.Then(TEXT("(a) drop client 1"), [this]()
			{
				FString Error;
				ASSERT_THAT(IsTrue(Hook->Drop(Clients[0], EStealthDropMode::ClientQuit, &Error), *Error));
			})
			.Until(TEXT("Client 1 is off the server"), [this]()
			{
				return Hook->LiveConnectionsWithKey(Keys[0]).Num() == 0 && IsOffline(Clients[0]);
			}, StepCeiling)
			.Then(TEXT("(a) the server logged client 1 out exactly once"), [this]()
			{
				ASSERT_THAT(AreEqual(1, Hook->CountObservations(EKind::Logout, Keys[0]), TEXT("one logout carrying key 1")));
				ASSERT_THAT(IsFalse(FStealthConnectionEventHook::IsClientConnected(Clients[0]), TEXT("client 1 has no server connection")));
			})
			// (b) Rejoin: the same IdentityKey reconnects.
			.Then(TEXT("(b) client 1 rejoins with key 1"), [this]()
			{
				FString Error;
				ASSERT_THAT(IsTrue(FStealthConnectionEventHook::Rejoin(Clients[0], Keys[0], TEXT("C1"), FString(), &Error), *Error));
			})
			.Until(TEXT("Client 1 is back"), [this]() { return IsJoined(*Hook, Clients[0], Keys[0]); }, StepCeiling)
			.Then(TEXT("(b) a new connection carries key 1"), [this]()
			{
				const TArray<UNetConnection*> Live = Hook->LiveConnectionsWithKey(Keys[0]);
				ASSERT_THAT(AreEqual(1, Live.Num(), TEXT("one live connection with key 1")));
				ASSERT_THAT(IsTrue(Live[0] != FirstKeyedC1.Get(), TEXT("the rejoin is a new UNetConnection")));
				ASSERT_THAT(AreEqual(2, Hook->CountObservations(EKind::Login, Keys[0]), TEXT("two logins with key 1 (join, rejoin)")));
			})
			// (c) Supersede: client 2 opens a second connection with client 1's key while client 1 is live.
			.Then(TEXT("(c) client 2 supersedes with key 1"), [this]()
			{
				FString Error;
				ASSERT_THAT(IsTrue(Hook->Supersede(Clients[1], Keys[0], TEXT("C2"), &Error), *Error));
			})
			.Until(TEXT("Two live connections carry key 1"), [this]()
			{
				return Hook->LiveConnectionsWithKey(Keys[0]).Num() == 2 && FStealthConnectionEventHook::IsClientConnected(Clients[1])
					&& Hook->ServerConnectionForClient(Clients[1]) != nullptr && Hook->ServerConnectionForClient(Clients[1])->PlayerController != nullptr;
			}, StepCeiling)
			.Then(TEXT("(c) both key-1 connections are live at once; client 2's own key left"), [this]()
			{
				const TArray<UNetConnection*> Live = Hook->LiveConnectionsWithKey(Keys[0]);
				ASSERT_THAT(IsTrue(Live.Contains(Hook->ServerConnectionForClient(Clients[0])), TEXT("client 1's connection is still live")));
				ASSERT_THAT(IsTrue(Live.Contains(Hook->ServerConnectionForClient(Clients[1])), TEXT("client 2's new connection carries key 1")));
				ASSERT_THAT(AreEqual(0, Hook->LiveConnectionsWithKey(Keys[1]).Num(), TEXT("no live connection with key 2")));
				ASSERT_THAT(AreEqual(1, Hook->CountObservations(EKind::Logout, Keys[1]), TEXT("key 2 logged out once")));
				ASSERT_THAT(AreEqual(3, Hook->CountObservations(EKind::Login, Keys[0]), TEXT("three logins with key 1")));
			})
			// (d) Kick: the host kicks client 3.
			.Then(TEXT("(d) the host kicks client 3"), [this]()
			{
				FString Error;
				ASSERT_THAT(IsTrue(Hook->Kick(Keys[2], FText::FromString(TEXT("harness kick")), &Error), *Error));
			})
			.Until(TEXT("Client 3 is off the server"), [this]()
			{
				return Hook->LiveConnectionsWithKey(Keys[2]).Num() == 0 && !FStealthConnectionEventHook::IsClientConnected(Clients[2]);
			}, StepCeiling)
			.Then(TEXT("(d) the server logged client 3 out exactly once"), [this]()
			{
				ASSERT_THAT(AreEqual(1, Hook->CountObservations(EKind::Logout, Keys[2]), TEXT("one logout carrying key 3")));
			})
			// (a, server side) the server closes client 2's key-1 connection.
			.Then(TEXT("(a) the server closes client 2's connection"), [this]()
			{
				FString Error;
				ASSERT_THAT(IsTrue(Hook->Drop(Clients[1], EStealthDropMode::ServerClose, &Error), *Error));
			})
			.Until(TEXT("Client 2 is off the server"), [this]()
			{
				return Hook->LiveConnectionsWithKey(Keys[0]).Num() == 1 && !FStealthConnectionEventHook::IsClientConnected(Clients[1]);
			}, StepCeiling)
			.Then(TEXT("(a) only client 1's key-1 connection is left"), [this]()
			{
				ASSERT_THAT(IsTrue(Hook->LiveConnectionsWithKey(Keys[0])[0] == Hook->ServerConnectionForClient(Clients[0]), TEXT("client 1 keeps its connection")));
				ASSERT_THAT(AreEqual(2, Hook->CountObservations(EKind::Logout, Keys[0]), TEXT("two key-1 logouts (drop, server close)")));
			})
			// (a, silent) client 1 goes silent: the connection stays open and the server hears nothing.
			.Then(TEXT("(a) client 1 goes silent"), [this]()
			{
				SilentConnection = Hook->ServerConnectionForClient(Clients[0]);
				FString Error;
				ASSERT_THAT(IsTrue(Hook->Drop(Clients[0], EStealthDropMode::Silent, &Error), *Error));
				StepFrame = GFrameCounter;
			})
			.Until(TEXT("Silence settles"), [this]() { return GFrameCounter >= StepFrame + SilentSettleFrames; }, StepCeiling)
			.Then(TEXT("Record the last receipt"), [this]()
			{
				ASSERT_THAT(IsTrue(SilentConnection.IsValid(), TEXT("silent connection still exists")));
				SilentLastReceive = SilentConnection->LastReceiveRealtime;
				StepFrame = GFrameCounter;
			})
			.Until(TEXT("Silent frames pass"), [this]() { return GFrameCounter >= StepFrame + SilentFrames; }, StepCeiling)
			.Then(TEXT("(a) silent: open connection, no receipt"), [this]()
			{
				ASSERT_THAT(IsTrue(SilentConnection.IsValid() && SilentConnection->GetConnectionState() == USOCK_Open, TEXT("the silent connection stays open")));
				ASSERT_THAT(IsTrue(SilentLastReceive == SilentConnection->LastReceiveRealtime, TEXT("the server received nothing while silent")));
				FString Error;
				ASSERT_THAT(IsTrue(Hook->ResumeFromSilent(Clients[0], &Error), *Error));
			})
			.Until(TEXT("Client 1 is heard again"), [this]()
			{
				return SilentConnection.IsValid() && SilentConnection->LastReceiveRealtime > SilentLastReceive;
			}, StepCeiling)
			.Then(TEXT("No unexpected connection events"), [this]()
			{
				// Logins: 3 keyed + rejoin + supersede = 5; logouts: 3 unkeyed + drop + key-2 leave + kick + server close = 7.
				int32 Logins = 0;
				int32 Logouts = 0;
				for (const FStealthConnectionObservation& Observation : Hook->GetObservations())
				{
					(Observation.Kind == EKind::Login ? Logins : Logouts)++;
				}
				ASSERT_THAT(AreEqual(5, Logins, TEXT("exactly five logins")));
				ASSERT_THAT(AreEqual(7, Logouts, TEXT("exactly seven logouts")));
			});
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS && ENABLE_PIE_NETWORK_TEST
