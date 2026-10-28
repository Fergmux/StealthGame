// Objectives Chain story oc-019 AC3 (AC-OC-33 [PIE-2]): StealthGame.Objectives.PIE.InformantsShareTeamView.
// Listen server + 2 clients on L_Prototype (NpcCount 0; teams {host, client X} and {client Y}). Client Y's pawn assassinates
// client X's (the production strike path), so X's team has one Active member (the host) and one Informant (X, respawned by the
// harness-only crowd-less respawn). A rival release for X's team (the service's test seam: one release due 0.2 s from now,
// scheduled and resolved through the real queue and resolver) raises that team's RivalPrelimCount:
//   - the host and X receive the same count, stage card and live-site set from the same team-level write (X's client gets it
//     as exactly one replicated change); client Y receives none of it (its change count and view are unchanged);
//   - X rejoining inside the grace (reproduced: its controller's view back to a new controller's default, then the Roster's
//     RegisterTeamView -> OnParticipantViewRebound) holds a team view equal to the host's.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthCompositionRoot.h"
#include "Server/StealthPenaltyHarnessSeams.h"
#include "StealthGameMode.h"
#include "Tests/PIE/Objectives/ObjectivesPieFixture.h"

NETWORK_TEST_CLASS(InformantsShareTeamView, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static StealthObjectivesPie::FScopedPieSettings Settings;

	TWeakObjectPtr<AStealthPlayerController> Host;
	TWeakObjectPtr<AStealthPlayerController> X;
	TWeakObjectPtr<AStealthPlayerController> XMachine;
	TWeakObjectPtr<AStealthPlayerController> Y;
	TWeakObjectPtr<AStealthPlayerController> YMachine;
	uint8 PairTeam = 0;
	int32 XChangesBefore = 0;
	int32 YChangesBefore = 0;
	FObjectiveTeamView YViewBefore;
	uint32 Seq = 0x00200000u;
	double Mark = 0.0;

	BEFORE_ALL()
	{
		Settings.Apply();
		StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(true);
	}

	AFTER_ALL()
	{
		StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(false);
		Settings.Restore();
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	TEST_METHOD(ActiveAndInformantGetTheSameViewInOneUpdate)
	{
		using namespace StealthObjectivesPie;
		Network
			.Until(TEXT("Active and the players split 2v1 with the host in the pair"), [this]()
			{
				AStealthPlayerController* Solo = nullptr;
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				if (!RoundActive() || Service() == nullptr || !SplitTeams(Solo, Pair))
				{
					return false;
				}
				AStealthPlayerController* HostSide = Pair[0]->IsLocalController() ? Pair[0] : (Pair[1]->IsLocalController() ? Pair[1] : nullptr);
				if (HostSide == nullptr)
				{
					return false; // the host is the solo player in this split: wait (never happens with the 2v1 lobby rule; guarded)
				}
				Host = HostSide;
				X = HostSide == Pair[0] ? Pair[1] : Pair[0];
				Y = Solo;
				XMachine = OwnerMachineController(X.Get());
				YMachine = OwnerMachineController(Y.Get());
				PairTeam = TeamOf(Host.Get());
				return XMachine.IsValid() && YMachine.IsValid() && ViewOf(XMachine.Get()).StageIndex == EObjectiveTeamStage::Stage1;
			})
			.Then(TEXT("Y strikes X"), [this]()
			{
				AStealthPawn* XPawn = Cast<AStealthPawn>(X->GetPawn());
				AStealthPawn* YPawn = Cast<AStealthPawn>(Y->GetPawn());
				ASSERT_THAT(IsTrue(XPawn != nullptr && YPawn != nullptr, TEXT("both pawns")));
				MoveClearOfBushes(XPawn);
				PlaceStrikerFacing(YPawn, XPawn, 150.0);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.6; })
			.Then(TEXT("strike"), [this]() { StrikeAt(Cast<AStealthPawn>(Y->GetPawn()), X->GetPawn(), Seq); })
			.Until(TEXT("X is an informant with a pawn again (respawned)"), [this]()
			{
				const AStealthPlayerState* State = X.IsValid() ? X->GetPlayerState<AStealthPlayerState>() : nullptr;
				const AStealthPawn* Pawn = X.IsValid() ? Cast<AStealthPawn>(X->GetPawn()) : nullptr;
				return State != nullptr && State->IsInformant() && Pawn != nullptr && !Pawn->HasDeathWrite() && RoundActive();
			})
			.Then(TEXT("a rival release for the pair's team"), [this]()
			{
				const AStealthPlayerState* HostState = Host->GetPlayerState<AStealthPlayerState>();
				ASSERT_THAT(IsTrue(HostState != nullptr && !HostState->IsInformant(), TEXT("the host is the team's Active member")));
				XChangesBefore = XMachine->GetObjectiveTeamView()->GetClientChangeCount();
				YChangesBefore = YMachine->GetObjectiveTeamView()->GetClientChangeCount();
				YViewBefore = ViewOf(YMachine.Get());
				ASSERT_THAT(IsTrue(ViewOf(Host.Get()).RivalPrelimCount == 0, TEXT("the count starts at 0")));
				ASSERT_THAT(IsTrue(Service()->ScheduleRivalReleaseNowForTest(PairTeam, 0.2), TEXT("the release is scheduled")));
			})
			.Until(TEXT("both the host and X hold count 1"), [this]()
			{
				return ViewOf(Host.Get()).RivalPrelimCount == 1 && ViewOf(XMachine.Get()).RivalPrelimCount == 1;
			})
			.Then(TEXT("mark"), [this]() { Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("1 s for any stray update"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("assert the shared view and Y's silence"), [this]()
			{
				const FObjectiveTeamView& HostView = ViewOf(Host.Get());
				const FObjectiveTeamView& XView = ViewOf(XMachine.Get());
				TestRunner->AddInfo(FString::Printf(TEXT("OC19 informant share: host count %d stage %d sites %d; X count %d stage %d sites %d; X changes +%d; Y changes +%d"),
					HostView.RivalPrelimCount, static_cast<int32>(HostView.StageIndex), HostView.LiveSites.Num(), XView.RivalPrelimCount, static_cast<int32>(XView.StageIndex),
					XView.LiveSites.Num(), XMachine->GetObjectiveTeamView()->GetClientChangeCount() - XChangesBefore, YMachine->GetObjectiveTeamView()->GetClientChangeCount() - YChangesBefore));
				ASSERT_THAT(IsTrue(XView == HostView, TEXT("the informant's view equals the Active member's: count, stage card and live sites")));
				ASSERT_THAT(IsTrue(XView == Service()->BuildTeamView(PairTeam), TEXT("and both equal the team's server-built view")));
				ASSERT_THAT(IsTrue(XMachine->GetObjectiveTeamView()->GetClientChangeCount() - XChangesBefore == 1, TEXT("X received it as one update")));
				ASSERT_THAT(IsTrue(YMachine->GetObjectiveTeamView()->GetClientChangeCount() == YChangesBefore, TEXT("the other team's client received nothing")));
				ASSERT_THAT(IsTrue(ViewOf(YMachine.Get()) == YViewBefore, TEXT("the other team's view is unchanged")));
				X->GetObjectiveTeamView()->ApplyView(FObjectiveTeamView()); // the rejoin: a new controller's default
			})
			.Until(TEXT("X shows the default view"), [this]() { return ViewOf(XMachine.Get()).IsEmpty(); })
			.Then(TEXT("the Roster rebinds X inside the grace"), [this]()
			{
				URosterStore* Roster = URosterStore::Get(ServerWorld());
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld());
				ASSERT_THAT(IsTrue(Roster != nullptr && Root != nullptr, TEXT("the server's Roster and composition root")));
				Roster->RegisterTeamView(Root->GetParticipant(X.Get()), X->GetTeamView());
			})
			.Until(TEXT("X's view equals the host's again"), [this]() { return ViewOf(XMachine.Get()) == ViewOf(Host.Get()) && !ViewOf(XMachine.Get()).IsEmpty(); })
			.Then(TEXT("assert the rejoin"), [this]()
			{
				ASSERT_THAT(IsTrue(ViewOf(XMachine.Get()).RivalPrelimCount == 1, TEXT("the rejoined informant keeps the team's released count")));
			});
	}
};

#endif
