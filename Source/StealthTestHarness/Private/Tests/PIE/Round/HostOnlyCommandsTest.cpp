// Story rmf-026 (production/epics/round-match-framework/story-026-host-only-commands.md), GDD AC 5 / R22, R2:
// a non-host client invokes each of the stealth.round.* commands (the frozen nine-name list, dumplog included: it is
// host-gated by the same check) by client console and by a development ServerExecRPC; each returns NotHost or is
// unreachable, and a hash of (phase, teams, RoundsPlayedCount, log entry count, Seq) is unchanged. Repeated in Lobby
// and in Active (Preparing and Ended need the Core stories 031..055 for the PIE run to dwell there).
// CQTest exemption: NETWORK_TEST_CLASS(HostOnlyCommands, "StealthGame.Round.PIE"); the GDD name
// StealthGame.Round.HostOnlyCommands maps to it. Hash note: the composition root has no Event Log or resolution queue
// wired yet, so "log entry count" and "Seq" are the Roster's EventSeq and the Round host-message count, which every
// accepted command changes.
// Bounds: every wait is a condition or a frame count; wall time is only the CQTest failure ceiling.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/Testing/StealthRoundTestConstants.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"

namespace HostOnlyCommandsTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	constexpr int32 ClientCount = 1;
	/** Frames the server gets to receive and run every ServerExecRPC. */
	constexpr uint64 RpcSettleFrames = 30;
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("240");

	/** The state hash of AC 5. */
	uint32 StateHash(UWorld* ServerWorld, int32 HostMessages)
	{
		const URoundMatchStore* Round = URoundMatchStore::Get(ServerWorld);
		const URosterStore* Roster = URosterStore::Get(ServerWorld);
		if (Round == nullptr || Roster == nullptr || Round->GetMatchCore() == nullptr)
		{
			return 0;
		}
		const FRoundPhaseCore& Phase = Round->GetMatchCore()->GetPhaseCore();
		uint32 Hash = GetTypeHash(static_cast<uint8>(Phase.GetPhase()));
		Hash = HashCombine(Hash, GetTypeHash(Phase.GetPhaseChangeCount()));
		Hash = HashCombine(Hash, GetTypeHash(Phase.GetRoundsPlayedCount()));
		TArray<FParticipantId> Ids;
		Roster->GetParticipantIds(Ids);
		for (const FParticipantId& Id : Ids)
		{
			const FParticipantRecord* Record = Roster->FindRecord(Id);
			Hash = HashCombine(Hash, HashCombine(GetTypeHash(Id), GetTypeHash(Record != nullptr ? static_cast<uint8>(Record->Team) : 0xEE)));
		}
		Hash = HashCombine(Hash, GetTypeHash(Roster->GetCore().GetEventSeq()));
		Hash = HashCombine(Hash, GetTypeHash(HostMessages));
		return Hash;
	}
}

NETWORK_TEST_CLASS(HostOnlyCommands, "StealthGame.Round.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };

	inline static FString PreviousNetworkTimeout;
	inline static bool bPreviousAutoStart = true;

	int32 HostMessages = 0;
	uint32 HashBefore = 0;
	uint64 SentFrame = 0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(HostOnlyCommandsTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		// The dev auto-start would change the phase under the hash; this test starts the round itself.
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
		using namespace HostOnlyCommandsTestData;
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(ClientCount)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(FixtureMap);
	}

	/** Queues: hash, every command by client console and by ServerExecRPC, settle, hash again. */
	void QueueClientAttempts(const TCHAR* PhaseName)
	{
		using namespace HostOnlyCommandsTestData;
		const FString Label(PhaseName);
		Network
			.ThenServer(*(Label + TEXT(": record the state hash")), [this, Label](FBasePIENetworkComponentState& Server)
			{
				URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				ASSERT_THAT(IsNotNull(Round, TEXT("server Round store")));
				Round->OnHostMessage.AddLambda([this](const FString&) { ++HostMessages; });
				HashBefore = StateHash(Server.World, HostMessages);
				ASSERT_THAT(IsTrue(HashBefore != 0, TEXT("server Round core exists")));

				// The server-side entry point under a remote exec scope answers NotHost for every command.
				for (int32 Index = 0; Index < URoundMatchStore::CommandCount; ++Index)
				{
					FRoundRemoteExecScope Remote;
					ASSERT_THAT(IsTrue(Round->ExecuteConsoleCommand(Server.World, static_cast<ERoundHostCommand>(Index), FRoundCommandArgs()) == ERoundCommandResult::NotHost,
						*FString::Printf(TEXT("%s: %s through a remote exec is NotHost"), *Label, URoundMatchStore::CommandNames[Index])));
				}
			})
			.ThenClients(*(Label + TEXT(": client console and ServerExecRPC")), [this, Label](FBasePIENetworkComponentState& Client)
			{
				APlayerController* ClientController = Client.World->GetFirstPlayerController();
				ASSERT_THAT(IsNotNull(ClientController, TEXT("client controller")));
				for (const TCHAR* Command : RoundTestFixtures::RoundHostCommandNames)
				{
					const FString Line = FString::Printf(TEXT("%s 1"), Command);
					// Client console: the client game instance holds no Round core, so the command is NotHost there.
					GEngine->Exec(Client.World, *Line);
					ClientController->ConsoleCommand(Line);
					// Development ServerExec: reaches the server's remote controller, inside the remote exec scope.
					ClientController->ServerExecRPC(Line);
				}
				URoundMatchStore* ClientRound = URoundMatchStore::Get(Client.World);
				ASSERT_THAT(IsTrue(ClientRound == nullptr || !ClientRound->HasMatchCore(), *(Label + TEXT(": the client holds no Round core"))));
				if (ClientRound != nullptr)
				{
					ASSERT_THAT(IsTrue(ClientRound->ExecuteConsoleCommand(Client.World, ERoundHostCommand::Start, FRoundCommandArgs()) == ERoundCommandResult::NotHost,
						*(Label + TEXT(": a client-world start is NotHost"))));
				}
				SentFrame = GFrameCounter;
			})
			.UntilServer(*(Label + TEXT(": RPCs settle")), [this](FBasePIENetworkComponentState&) { return GFrameCounter >= SentFrame + RpcSettleFrames; })
			.ThenServer(*(Label + TEXT(": the state hash is unchanged")), [this, Label](FBasePIENetworkComponentState& Server)
			{
				ASSERT_THAT(AreEqual(HashBefore, StateHash(Server.World, HostMessages), *(Label + TEXT(": hash of (phase, teams, RoundsPlayedCount, log, Seq) unchanged"))));
			});
	}

	TEST_METHOD(NonHostCommandsAreNotHostInLobbyAndActive)
	{
		Network.UntilServer(TEXT("Lobby with two participants"), [](FBasePIENetworkComponentState& Server)
		{
			const URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
			const URosterStore* Roster = URosterStore::Get(Server.World);
			if (Round == nullptr || Roster == nullptr || !Roster->IsMatchOpen())
			{
				return false;
			}
			TArray<FParticipantId> Ids;
			Roster->GetParticipantIds(Ids);
			return Round->GetPhase() == ERoundPhase::Lobby && Ids.Num() == 2;
		});
		QueueClientAttempts(TEXT("Lobby"));
		Network
			.ThenServer(TEXT("host starts the round"), [this](FBasePIENetworkComponentState& Server)
			{
				URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				ASSERT_THAT(IsTrue(Round->ExecuteConsoleCommand(Server.World, ERoundHostCommand::Start, FRoundCommandArgs()) == ERoundCommandResult::Executed,
					TEXT("the host's own console start is accepted")));
			})
			.UntilServer(TEXT("Active"), [](FBasePIENetworkComponentState& Server)
			{
				const URoundMatchStore* Round = URoundMatchStore::Get(Server.World);
				return Round != nullptr && Round->GetPhase() == ERoundPhase::Active;
			});
		QueueClientAttempts(TEXT("Active"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS && ENABLE_PIE_NETWORK_TEST
