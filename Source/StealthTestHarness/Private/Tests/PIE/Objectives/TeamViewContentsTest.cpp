// Objectives Chain story oc-019 AC1 (AC-OC-03 [PIE-2]): StealthGame.Objectives.PIE.TeamViewContents.
// Listen server + 2 clients on L_Prototype (NpcCount 0; each team has one remote client). Each client's replicated objective
// data is dumped every frame: its own UObjectiveTeamViewComponent's live sites (the only Objectives data a client receives;
// no other team view component exists in a client world) and FRoundPublicState, which has no objective field (reflection).
//   - at Active start: team 0's client holds only P1[0] as a live site, team 1's only P1[1]; no client holds Final, P2[*] or an
//     opponent site;
//   - team 0 completes both preliminaries (DebugCompleteStage through Round's DebugItem): only team 0's clients gain Final
//     (the host, on team 0 or 1, is checked as a client of its own team); team 1's client never holds Final;
//   - a late-joining client (reproduced: the controller's view back to a new controller's default, then the Roster's
//     RegisterTeamView -> OnParticipantViewRebound) sees the same set again.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthCompositionRoot.h"
#include "StealthGameMode.h"
#include "Tests/PIE/Objectives/ObjectivesPieFixture.h"
#include "UObject/UObjectIterator.h"
#include "View/StealthRoundPublicState.h"

NETWORK_TEST_CLASS(TeamViewContents, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static StealthObjectivesPie::FScopedPieSettings Settings;

	TWeakObjectPtr<AStealthPlayerController> ServerSide[2];
	TWeakObjectPtr<AStealthPlayerController> Machine[2];
	StealthObjectivesPie::FCourts Courts;
	int32 Samples = 0;
	int32 Violations = 0;
	int32 Team1HeldFinal = 0;
	int32 ExtraComponents = 0;

	BEFORE_ALL() { Settings.Apply(); }
	AFTER_ALL() { Settings.Restore(); }

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	/** Every frame: no client holds a court its team may not know; the other team never the final; one component per client. */
	void Sample(bool bTeam0HasFinal)
	{
		using namespace StealthObjectivesPie;
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
			ExtraComponents += Components == 1 ? 0 : 1;
			TSet<uint16> Allowed = { Courts.P1[Team].Value };
			if (Team == 0)
			{
				Allowed.Add(Courts.P2[0].Value);
				if (bTeam0HasFinal)
				{
					Allowed.Add(Courts.Final.Value);
				}
			}
			for (const uint16 Court : CourtsOf(ViewOf(Local)))
			{
				Violations += Allowed.Contains(Court) ? 0 : 1;
				Team1HeldFinal += Team == 1 && Court == Courts.Final.Value ? 1 : 0;
			}
		}
	}

	TEST_METHOD(EachClientHoldsOnlyItsOwnLiveSites)
	{
		using namespace StealthObjectivesPie;
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
				Courts = BoundCourts();
				return Courts.IsSet() && Machine[0].IsValid() && Machine[1].IsValid() && ViewOf(Machine[0].Get()).StageIndex == EObjectiveTeamStage::Stage1
					&& ViewOf(Machine[1].Get()).StageIndex == EObjectiveTeamStage::Stage1;
			})
			.Then(TEXT("assert the Active-start dump"), [this]()
			{
				Sample(false);
				for (int32 Team = 0; Team < 2; ++Team)
				{
					const TSet<uint16> Held = CourtsOf(ViewOf(Machine[Team].Get()));
					TestRunner->AddInfo(FString::Printf(TEXT("OC19 team %d client at Active: live %s (P1 %u, P2 %u, final %u)"), Team,
						*FString::JoinBy(Held, TEXT(","), [](uint16 C) { return FString::FromInt(C); }), Courts.P1[Team].Value, Courts.P2[Team].Value, Courts.Final.Value));
					ASSERT_THAT(IsTrue(Held.Num() == 1 && Held.Contains(Courts.P1[Team].Value), TEXT("a client holds only its own P1 at Active start")));
				}
				// FRoundPublicState carries no objective field (reflection over its members, nested structs included).
				TArray<FString> Fields;
				for (TFieldIterator<FProperty> It(FRoundPublicState::StaticStruct()); It; ++It)
				{
					Fields.Add(It->GetName());
					if (const FStructProperty* Nested = CastField<FStructProperty>(*It))
					{
						for (TFieldIterator<FProperty> Inner(Nested->Struct); Inner; ++Inner)
						{
							Fields.Add(It->GetName() + TEXT(".") + Inner->GetName());
						}
					}
				}
				const TArray<FString> Objective = Fields.FilterByPredicate([](const FString& F) { return F.Contains(TEXT("Objective")) || F.Contains(TEXT("Site")) || F.Contains(TEXT("Court")); });
				TestRunner->AddInfo(FString::Printf(TEXT("OC19 FRoundPublicState fields: %s"), *FString::Join(Fields, TEXT(", "))));
				ASSERT_THAT(IsTrue(Objective.Num() == 0, TEXT("FRoundPublicState holds no objective field")));
				ASSERT_THAT(IsTrue(Service()->DebugCompleteStage(0), TEXT("team 0's first DebugCompleteStage accepted")));
			})
			.Until(TEXT("team 0's client reaches Stage2"), [this]()
			{
				Sample(false);
				return ViewOf(Machine[0].Get()).StageIndex == EObjectiveTeamStage::Stage2;
			})
			.Then(TEXT("team 0 completes its second preliminary"), [this]()
			{
				ASSERT_THAT(IsTrue(Service()->DebugCompleteStage(0), TEXT("team 0's second DebugCompleteStage accepted")));
			})
			.Until(TEXT("team 0's client holds the final"), [this]()
			{
				Sample(true);
				return ViewOf(Machine[0].Get()).StageIndex == EObjectiveTeamStage::Final;
			})
			.Then(TEXT("assert only team 0 gained the final; reproduce a late join of team 0's client"), [this]()
			{
				Sample(true);
				const TSet<uint16> Team0 = CourtsOf(ViewOf(Machine[0].Get()));
				ASSERT_THAT(IsTrue(Team0.Num() == 1 && Team0.Contains(Courts.Final.Value), TEXT("team 0's client holds exactly the final")));
				ASSERT_THAT(IsTrue(!CourtsOf(ViewOf(Machine[1].Get())).Contains(Courts.Final.Value), TEXT("team 1's client does not hold the final")));
				// The host's own controller (a client of its own team on the listen server) holds its team's set too.
				for (AStealthPlayerController* Player : ServerPlayers())
				{
					if (Player->IsLocalController())
					{
						const uint8 HostTeam = TeamOf(Player);
						ASSERT_THAT(IsTrue(HostTeam < 2 && ViewOf(Player) == Service()->BuildTeamView(HostTeam), TEXT("the host holds its team's view")));
					}
				}
				ServerSide[0]->GetObjectiveTeamView()->ApplyView(FObjectiveTeamView());
			})
			.Until(TEXT("team 0's client shows the default view (a new controller)"), [this]()
			{
				return ViewOf(Machine[0].Get()).IsEmpty();
			})
			.Then(TEXT("the Roster rebinds the late joiner's view"), [this]()
			{
				URosterStore* Roster = URosterStore::Get(ServerWorld());
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld());
				ASSERT_THAT(IsTrue(Roster != nullptr && Root != nullptr, TEXT("the server's Roster and composition root")));
				Roster->RegisterTeamView(Root->GetParticipant(ServerSide[0].Get()), ServerSide[0]->GetTeamView());
			})
			.Until(TEXT("the late joiner holds the same set again"), [this]()
			{
				Sample(true);
				const TSet<uint16> Team0 = CourtsOf(ViewOf(Machine[0].Get()));
				return Team0.Num() == 1 && Team0.Contains(Courts.Final.Value);
			})
			.Then(TEXT("assert over the whole run"), [this]()
			{
				TestRunner->AddInfo(FString::Printf(TEXT("OC19 TeamViewContents samples=%d violations=%d team1-held-final=%d extra-components=%d"), Samples, Violations,
					Team1HeldFinal, ExtraComponents));
				ASSERT_THAT(IsTrue(Samples >= 4, TEXT("sampled")));
				ASSERT_THAT(IsTrue(Violations == 0, TEXT("no client ever held a court its team may not know")));
				ASSERT_THAT(IsTrue(Team1HeldFinal == 0, TEXT("team 1's client never held the final")));
				ASSERT_THAT(IsTrue(ExtraComponents == 0, TEXT("each client world held exactly its own Objectives view component")));
			});
	}
};

#endif
