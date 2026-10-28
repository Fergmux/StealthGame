// Objectives Chain story oc-021 AC3 (GDD Edge Cases, 1v0 dev round [PIE-2]): StealthGame.Objectives.PIE.OneVersusZeroDevRound.
// Listen server + 1 client on L_Prototype (NpcCount 0) with the dev auto-start off: in Lobby the host removes the client from the
// roster (Round's Remove host command; CQTest needs at least one client, and a 2v0 start is refused as an imbalance), so the round
// starts 1v0 with the host alone (bAllowEmptyTeamDev, set by the composition root):
//   - the chain runs for the one team (Stage1 -> Stage2 -> Final -> ObjectiveWin for that team, through Round's DebugItem);
//   - bRivalSignalEnabled is forced off (it is true in the tuning): no StageSignal timer is ever scheduled and no
//     RivalPrelimCount is published (the host's view keeps 0; the removed client holds no team view);
//   - the round is excluded from the F5 stall metrics through the round-record flag: the service reports it and the server
//     Round Event Log holds the ObjectivePacing exclusion entry that the F5 calculator (FObjectivesMetrics::StallInputFromLog)
//     filters on.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Core/Round/StealthRoundMatchCore.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/StealthCompositionRoot.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "Tests/PIE/Objectives/ObjectivesPieFixture.h"

NETWORK_TEST_CLASS(OneVersusZeroDevRound, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static StealthObjectivesPie::FScopedPieSettings Settings;

	TWeakObjectPtr<AStealthPlayerController> Host;
	TWeakObjectPtr<AStealthPlayerController> Client;
	TWeakObjectPtr<AStealthPlayerController> ClientMachine;
	uint8 Team = 0;
	int32 MaxRival = 0;
	double Mark = 0.0;

	BEFORE_ALL() { Settings.Apply(/*bAutoStart*/ false); }
	AFTER_ALL() { Settings.Restore(); }

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(1).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	static const AStealthGameState* ServerState()
	{
		UWorld* Server = StealthAssassinationPie::ServerWorld();
		return Server != nullptr ? Server->GetGameState<AStealthGameState>() : nullptr;
	}

	void Watch()
	{
		using namespace StealthObjectivesPie;
		for (const AStealthPlayerController* Each : { Host.Get(), ClientMachine.Get() })
		{
			MaxRival = FMath::Max<int32>(MaxRival, Each != nullptr ? ViewOf(Each).RivalPrelimCount : 0);
		}
	}

	TEST_METHOD(OneTeamRunsTheChainWithoutARivalSignalAndOutOfTheStallMetrics)
	{
		using namespace StealthObjectivesPie;
		Network
			.Until(TEXT("Lobby with both players registered and connected for a second"), [this]()
			{
				const URoundMatchStore* Round = URoundMatchStore::Get(ServerWorld());
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld());
				if (Round == nullptr || Root == nullptr || Round->GetPhase() != ERoundPhase::Lobby)
				{
					return false;
				}
				for (FConstPlayerControllerIterator It = ServerWorld()->GetPlayerControllerIterator(); It; ++It)
				{
					AStealthPlayerController* Each = Cast<AStealthPlayerController>(It->Get());
					if (Each != nullptr && Root->GetParticipant(Each).IsValid())
					{
						(Each->IsLocalController() ? Host : Client) = Each;
					}
				}
				if (!Host.IsValid() || !Client.IsValid())
				{
					return false;
				}
				Mark = Mark <= 0.0 ? ServerNow(ServerWorld()) : Mark;
				return ServerNow(ServerWorld()) >= Mark + 1.0;
			})
			.Then(TEXT("the host removes the client from the roster, then starts the round"), [this]()
			{
				URoundMatchStore* Round = URoundMatchStore::Get(ServerWorld());
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld());
				FRoundCommandArgs Args;
				Args.Participant = Root->GetParticipant(Client.Get());
				const ERoundCommandResult Result = Round->ExecuteCommand(FRoundCommandCaller::ServerAsHost(Round->GetHostParticipant()), ERoundHostCommand::Remove, Args);
				ASSERT_THAT(IsTrue(Result == ERoundCommandResult::Executed, TEXT("the client was removed from the roster in Lobby")));
				ServerWorld()->GetAuthGameMode<AStealthGameMode>()->StartRound();
			})
			.Until(TEXT("Active with the host alone (1v0)"), [this]()
			{
				ClientMachine = OwnerMachineController(Client.Get());
				Team = TeamOf(Host.Get());
				return RoundActive() && Service() != nullptr && Team < 2 && ViewOf(Host.Get()).StageIndex == EObjectiveTeamStage::Stage1;
			})
			.Then(TEXT("assert the 1v0 flags; complete Stage1"), [this]()
			{
				TestRunner->AddInfo(FString::Printf(TEXT("OC21 1v0: team %d, excluded %d, rival signal tuning on %d"), Team, Service()->IsExcludedFromStallMetrics() ? 1 : 0,
					FObjectivesTuning_Server().bRivalSignalEnabled ? 1 : 0));
				ASSERT_THAT(IsTrue(Service()->IsExcludedFromStallMetrics(), TEXT("the round is flagged out of the stall metrics")));
				ASSERT_THAT(IsTrue(Service()->DebugCompleteStage(Team), TEXT("Stage1 debug completion accepted")));
			})
			.Until(TEXT("Stage2"), [this]() { Watch(); return Service()->GetObjectiveTeamStage(Team) == EObjectiveTeamStage::Stage2; })
			.Then(TEXT("complete Stage2"), [this]() { ASSERT_THAT(IsTrue(Service()->DebugCompleteStage(Team), TEXT("Stage2 accepted"))); })
			.Until(TEXT("Final"), [this]() { Watch(); return Service()->GetObjectiveTeamStage(Team) == EObjectiveTeamStage::Final; })
			.Then(TEXT("complete the final"), [this]() { ASSERT_THAT(IsTrue(Service()->DebugCompleteStage(Team), TEXT("final accepted"))); })
			.Until(TEXT("the round ends"), [this]()
			{
				Watch();
				return ServerState() != nullptr && ServerState()->GetRoundPublicState().Phase == ERoundPhase::Ended;
			})
			.Then(TEXT("assert"), [this]()
			{
				const FRoundResult& Result = ServerState()->GetRoundPublicState().LastResult;
				ASSERT_THAT(IsTrue(Result.WinningTeam == Team && Result.PublicReason == 1, TEXT("the chain ran for the one team: ObjectiveWin")));
				ASSERT_THAT(IsTrue(Service()->GetScheduledStageSignalCount() == 0, TEXT("no StageSignal timer was ever scheduled")));
				ASSERT_THAT(IsTrue(MaxRival == 0, TEXT("no RivalPrelimCount was ever published")));
				const FRoundMatchCore* Core = URoundMatchStore::Get(ServerWorld())->GetMatchCore();
				int32 Exclusions = 0;
				int32 Released = 0;
				for (const FRoundLogEntry& Entry : Core->GetLogArchive().GetCurrentEntries())
				{
					Exclusions += Entry.Type == ERoundLogType::ObjectivePacing && Entry.Code == UObjectivesService::ObjectivePacingExcludeFromStallMetrics ? 1 : 0;
					Released += Entry.Type == ERoundLogType::StageSignalReleased ? 1 : 0;
				}
				TestRunner->AddInfo(FString::Printf(TEXT("OC21 1v0 log: ObjectivePacing exclusion entries %d, StageSignalReleased %d"), Exclusions, Released));
				ASSERT_THAT(IsTrue(Exclusions == 1, TEXT("the round record carries the F5 exclusion flag (one ObjectivePacing entry)")));
				ASSERT_THAT(IsTrue(Released == 0, TEXT("no StageSignalReleased entry")));
			});
	}
};

#endif
