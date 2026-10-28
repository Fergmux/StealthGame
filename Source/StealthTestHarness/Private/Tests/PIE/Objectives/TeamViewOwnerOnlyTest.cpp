// Objectives Chain story oc-011 (production/epics/objectives-chain/story-011-team-view-component-and-replication.md), AC1
// (ADR-0020 OBJ-V1, AC-OC-03): StealthGame.Objectives.PIE.TeamViewOwnerOnly.EachTeamsViewReachesOnlyItsOwnClients. Listen
// server + 2 clients on L_Prototype (NpcCount 0; the teams are {host, client 2} and {client 1}, so each team has one remote
// client). Sampled on every frame of the run, on every client machine: exactly one UObjectiveTeamViewComponent exists in that
// world (its own controller's: the other players' controllers never reach it), and its view never holds a court that is live
// only for the other team. At Active entry each client's view equals its team's server-built view (Stage1, its own P1). When
// team A banks Stage1 (DebugCompleteStage through Round's DebugItem), team A's client reaches Stage2 and team B's client
// receives no change at all (its replicated change count and value are unchanged). Rejoin (Session's PIE rejoin is not built,
// so the test reproduces it on the server: each client's controller view is reset to the default a new controller has, then the
// Roster's RegisterTeamView raises OnParticipantViewRebound and UObjectivesService writes the team view in the handler): each
// client receives its own team's view again, and still never the other team's. The seamless-travel case needs the Session
// multi-process harness (single-process PIE cannot travel, ADR-0005): not run here (story notes).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/ObjectivesService.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthCompositionRoot.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "UObject/UObjectIterator.h"
#include "View/ObjectiveTeamViewComponent.h"

namespace TeamViewOwnerOnlyPieData
{
	/** Courts in Theirs that are not in Ours (the sites live only for the other team). */
	TSet<uint16> ExclusiveCourts(const FObjectiveTeamView& Theirs, const FObjectiveTeamView& Ours)
	{
		TSet<uint16> Out;
		for (const FObjectiveLiveSite& Site : Theirs.LiveSites)
		{
			if (Ours.FindLiveSite(Site.Court) == nullptr)
			{
				Out.Add(Site.Court.Value);
			}
		}
		return Out;
	}
}

NETWORK_TEST_CLASS(TeamViewOwnerOnly, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	/** Per team: the server controller of its remote client and that client's local controller. */
	TWeakObjectPtr<AStealthPlayerController> ServerSide[2];
	TWeakObjectPtr<AStealthPlayerController> Machine[2];
	uint8 TeamA = 0;
	int32 Samples = 0;
	int32 ComponentCountViolations = 0;
	int32 LeakViolations = 0;
	int32 BChangesBefore = 0;
	FObjectiveTeamView BViewBefore;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousNetworkTimeout.IsEmpty())
		{
			Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
		}
		if (PreviousNpcCount >= 0)
		{
			GetMutableDefault<UStealthSettings>()->NpcCount = PreviousNpcCount;
		}
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	static UObjectivesService* Service() { return UObjectivesService::Get(StealthAssassinationPie::ServerWorld()); }

	static const FObjectiveTeamView& ViewOf(const AStealthPlayerController* Controller)
	{
		static const FObjectiveTeamView Empty;
		const UObjectiveTeamViewComponent* Component = Controller != nullptr ? Controller->GetObjectiveTeamView() : nullptr;
		return Component != nullptr ? Component->GetView() : Empty;
	}

	/** One frame's owner-only check on both client machines. */
	void Sample()
	{
		using namespace StealthAssassinationPie;
		const UObjectivesService* Objectives = Service();
		if (Objectives == nullptr)
		{
			return;
		}
		++Samples;
		for (int32 Team = 0; Team < 2; ++Team)
		{
			const AStealthPlayerController* Local = Machine[Team].Get();
			if (Local == nullptr)
			{
				continue;
			}
			int32 Components = 0;
			for (TObjectIterator<UObjectiveTeamViewComponent> It; It; ++It)
			{
				Components += It->GetWorld() == Local->GetWorld() && !It->IsTemplate() ? 1 : 0;
			}
			ComponentCountViolations += Components == 1 ? 0 : 1;
			const TSet<uint16> Theirs = TeamViewOwnerOnlyPieData::ExclusiveCourts(Objectives->BuildTeamView(1 - Team), Objectives->BuildTeamView(Team));
			for (const FObjectiveLiveSite& Site : ViewOf(Local).LiveSites)
			{
				LeakViolations += Theirs.Contains(Site.Court.Value) ? 1 : 0;
			}
		}
	}

	TEST_METHOD(EachTeamsViewReachesOnlyItsOwnClients)
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("Active, one remote client per team, both holding their Stage1 view"), [this]()
			{
				if (!RoundActive() || Service() == nullptr)
				{
					return false;
				}
				for (AStealthPlayerController* Player : ServerPlayers())
				{
					const uint8 Team = TeamOf(Player);
					if (!Player->IsLocalController() && Team < 2)
					{
						ServerSide[Team] = Player;
						Machine[Team] = OwnerMachineController(Player);
					}
				}
				Sample();
				return Machine[0].IsValid() && Machine[1].IsValid() && ViewOf(Machine[0].Get()).StageIndex == EObjectiveTeamStage::Stage1
					&& ViewOf(Machine[1].Get()).StageIndex == EObjectiveTeamStage::Stage1;
			})
			.Then(TEXT("assert each client holds exactly its own team's view"), [this]()
			{
				const UObjectivesService* Objectives = Service();
				for (int32 Team = 0; Team < 2; ++Team)
				{
					const FObjectiveTeamView& Client = ViewOf(Machine[Team].Get());
					ASSERT_THAT(IsTrue(Client == Objectives->BuildTeamView(Team), TEXT("the client's view equals its team's server-built view")));
					ASSERT_THAT(IsTrue(ViewOf(ServerSide[Team].Get()) == Client, TEXT("the server copy on that controller is the same value")));
					ASSERT_THAT(IsTrue(Client.LiveSites.Num() == 1, TEXT("one live site at Stage1 (its own P1)")));
					TestRunner->AddInfo(FString::Printf(TEXT("OC011 OBJ-V1 team %d client: stage %d court %u def %u"), Team, static_cast<int32>(Client.StageIndex),
						Client.LiveSites[0].Court.Value, Client.LiveSites[0].DefIndex));
				}
				ASSERT_THAT(IsTrue(ViewOf(Machine[0].Get()).LiveSites[0].Court != ViewOf(Machine[1].Get()).LiveSites[0].Court, TEXT("the two teams' P1 courts differ")));
				TeamA = 0; // team A = team 0, team B = team 1 (each has one remote client)
				BChangesBefore = Machine[1]->GetObjectiveTeamView()->GetClientChangeCount();
				BViewBefore = ViewOf(Machine[1].Get());
				ASSERT_THAT(IsTrue(Service()->DebugCompleteStage(TeamA), TEXT("team A's DebugCompleteStage was accepted")));
			})
			.Until(TEXT("team A's client reaches Stage2"), [this]()
			{
				Sample();
				return ViewOf(Machine[0].Get()).StageIndex == EObjectiveTeamStage::Stage2;
			})
			.Then(TEXT("assert team B received nothing"), [this]()
			{
				ASSERT_THAT(IsTrue(Machine[1]->GetObjectiveTeamView()->GetClientChangeCount() == BChangesBefore, TEXT("team B's client received no change")));
				ASSERT_THAT(IsTrue(ViewOf(Machine[1].Get()) == BViewBefore, TEXT("team B's view is unchanged by team A's progress")));
				ASSERT_THAT(IsTrue(ViewOf(Machine[0].Get()) == Service()->BuildTeamView(0), TEXT("team A's client holds team A's new view")));
				// The rejoin reproduction, step 1: each controller's view back to a new controller's default.
				for (int32 Team = 0; Team < 2; ++Team)
				{
					ServerSide[Team]->GetObjectiveTeamView()->ApplyView(FObjectiveTeamView());
				}
			})
			.Until(TEXT("both clients show the default view"), [this]()
			{
				Sample();
				return ViewOf(Machine[0].Get()).IsEmpty() && ViewOf(Machine[1].Get()).IsEmpty();
			})
			.Then(TEXT("the Roster rebinds both participants' views (OnParticipantViewRebound)"), [this]()
			{
				URosterStore* Roster = URosterStore::Get(ServerWorld());
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld());
				ASSERT_THAT(IsTrue(Roster != nullptr && Root != nullptr, TEXT("the server's Roster and composition root")));
				for (int32 Team = 0; Team < 2; ++Team)
				{
					Roster->RegisterTeamView(Root->GetParticipant(ServerSide[Team].Get()), ServerSide[Team]->GetTeamView());
					ASSERT_THAT(IsTrue(ViewOf(ServerSide[Team].Get()) == Service()->BuildTeamView(Team), TEXT("the rebind handler wrote the team view inline")));
				}
			})
			.Until(TEXT("both clients hold their own team's view again"), [this]()
			{
				Sample();
				return ViewOf(Machine[0].Get()) == Service()->BuildTeamView(0) && ViewOf(Machine[1].Get()) == Service()->BuildTeamView(1);
			})
			.Then(TEXT("assert owner-only over the whole run"), [this]()
			{
				TestRunner->AddInfo(FString::Printf(TEXT("OC011 OBJ-V1 samples=%d component-count violations=%d leak violations=%d"), Samples, ComponentCountViolations, LeakViolations));
				ASSERT_THAT(IsTrue(Samples >= 4, TEXT("the run was sampled (at least once per wait)")));
				ASSERT_THAT(IsTrue(ComponentCountViolations == 0, TEXT("every client world held exactly one Objectives view component (its own)")));
				ASSERT_THAT(IsTrue(LeakViolations == 0, TEXT("no client ever held a court live only for the other team")));
			});
	}
};

#endif
