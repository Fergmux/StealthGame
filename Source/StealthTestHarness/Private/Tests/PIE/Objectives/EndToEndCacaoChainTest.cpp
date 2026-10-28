// Objectives Chain story oc-021 AC1 / AC2 (AC-OC-14 [Integration/BLOCKING, PIE-2]): StealthGame.Objectives.PIE.EndToEndCacaoChain.
// Listen server + 1 client on L_Prototype (1v1, NpcCount 0), the real UObjectivesService, Round queue, Interact hold stand-in
// and team-view carrier. Every tend is a REAL hold to D (each player's pawn placed at its live court, InteractBegin submitted
// with its controller's own origin, the StealthBot path): team 0 P1, team 1 P1, team 0 P2; team 1 begins its P2 and, while it
// runs, team 0 begins the final; team 1's P2 completes during team 0's final hold (team 1 finishing its preliminaries after team
// 0 started the final), team 1 then tends the final too, and team 0's final completes first:
//   - FRoundResult {ObjectiveWin, winner team 0}; team 1's final completion arrives after the latch (no effect);
//   - the server Round Event Log holds exactly 4 ObjectiveStage and 1 FinalCompleted entries, in key order;
//   - SP-1 on the wire: team 1's client never held the final's court before team 1 completed its own preliminaries
//     (interpretation recorded in the story: the AC's "never held the final id" cannot hold together with "4 ObjectiveStage"
//     once team 1 counts its P2 before the latch; the leak it guards is an early final).
// The screenshots of AC2 (oc-e2e-*.png) are the story's run-and-observe.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Core/Round/StealthRoundMatchCore.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "Tests/PIE/Objectives/ObjectivesPieFixture.h"

NETWORK_TEST_CLASS(EndToEndCacaoChain, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static StealthObjectivesPie::FScopedPieSettings Settings;

	TWeakObjectPtr<AStealthPlayerController> Player[2];
	TWeakObjectPtr<AStealthPlayerController> Team1Machine;
	StealthObjectivesPie::FCourts Courts;
	uint32 Seq = 0x00500000u;
	double Mark = 0.0;
	double Due = 0.0;
	int32 EarlyFinalSightings = 0;
	bool bTeam1PrelimsDone = false;

	BEFORE_ALL() { Settings.Apply(); }
	AFTER_ALL() { Settings.Restore(); }

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(1).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	/** Every frame: team 1's client holds the final only once team 1's own preliminaries are complete (SP-1). */
	void Watch()
	{
		using namespace StealthObjectivesPie;
		bTeam1PrelimsDone |= Service() != nullptr && Service()->GetCompletedPreliminaryStages(1) >= 2;
		if (Team1Machine.IsValid() && !bTeam1PrelimsDone && CourtsOf(ViewOf(Team1Machine.Get())).Contains(Courts.Final.Value))
		{
			++EarlyFinalSightings;
		}
	}

	AStealthPawn* PawnOf(int32 Team) const { return Player[Team].IsValid() ? Cast<AStealthPawn>(Player[Team]->GetPawn()) : nullptr; }

	/** A real hold: place Team's pawn at Role's court (0 P1, 1 P2, 2 final), settle, Begin. */
	void Begin(int32 Team, int32 Role)
	{
		using namespace StealthObjectivesPie;
		const FCourtFixtureId Court = Role == 0 ? Courts.P1[Team] : (Role == 1 ? Courts.P2[Team] : Courts.Final);
		PlaceAtCourt(PawnOf(Team), Court, Team == 0 ? 140.0 : -140.0);
		BeginTendAsOwner(PawnOf(Team), Court, Seq);
	}

	void TendAndWait(int32 Team, int32 Role, EObjectiveTeamStage Expect)
	{
		using namespace StealthObjectivesPie;
		Network
			.Then(*FString::Printf(TEXT("team %d tends role %d"), Team, Role), [this, Team, Role]() { Begin(Team, Role); })
			.Until(*FString::Printf(TEXT("team %d reaches stage %d"), Team, static_cast<int32>(Expect)), [this, Team, Expect]()
			{
				Watch();
				return Service()->GetObjectiveTeamStage(Team) == Expect;
			});
	}

	TEST_METHOD(BothTeamsTendTheirPreliminariesAndTeamZeroWinsTheFinal)
	{
		using namespace StealthObjectivesPie;
		Network.Until(TEXT("Active, 1v1"), [this]()
		{
			if (!RoundActive() || Service() == nullptr)
			{
				return false;
			}
			for (AStealthPlayerController* Each : ServerPlayers())
			{
				const uint8 Team = TeamOf(Each);
				if (Team < 2)
				{
					Player[Team] = Each;
					if (Team == 1)
					{
						Team1Machine = OwnerMachineController(Each);
					}
				}
			}
			Courts = BoundCourts();
			return Courts.IsSet() && Player[0].IsValid() && Player[1].IsValid() && Team1Machine.IsValid();
		});
		TendAndWait(0, 0, EObjectiveTeamStage::Stage2);
		TendAndWait(1, 0, EObjectiveTeamStage::Stage2);
		TendAndWait(0, 1, EObjectiveTeamStage::Final);
		Network
			.Then(TEXT("team 1 begins its P2"), [this]() { Begin(1, 1); Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("1 s into team 1's hold"), [this]() { Watch(); return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("team 0 begins the final (team 1's P2 completes during it)"), [this]()
			{
				Begin(0, 2);
				Due = ServerNow(ServerWorld()) + Service()->GetInteractHoldSec();
			})
			.Until(TEXT("team 1's P2 counts (its final unlocks)"), [this]() { Watch(); return Service()->GetObjectiveTeamStage(1) == EObjectiveTeamStage::Final; })
			.Then(TEXT("team 1 tends the final too"), [this]() { Begin(1, 2); })
			.Until(TEXT("the round ends"), [this]()
			{
				Watch();
				const AStealthGameState* State = ServerWorld() != nullptr ? ServerWorld()->GetGameState<AStealthGameState>() : nullptr;
				return State != nullptr && State->GetRoundPublicState().Phase == ERoundPhase::Ended;
			})
			.Then(TEXT("mark"), [this]() { Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("past team 1's final hold"), [this]() { Watch(); return ServerNow(ServerWorld()) >= Mark + 4.0; })
			.Then(TEXT("assert"), [this]()
			{
				const AStealthGameState* State = ServerWorld()->GetGameState<AStealthGameState>();
				const FRoundResult& Result = State->GetRoundPublicState().LastResult;
				TestRunner->AddInfo(FString::Printf(TEXT("OC21 result: winner %d, public reason %d, steps %d / %d"), Result.WinningTeam, Result.PublicReason,
					Result.TeamSummary[0].StepsCompleted, Result.TeamSummary[1].StepsCompleted));
				ASSERT_THAT(IsTrue(Result.WinningTeam == 0 && Result.PublicReason == 1, TEXT("FRoundResult {ObjectiveWin, winner team 0}")));
				const URoundMatchStore* Round = URoundMatchStore::Get(ServerWorld());
				const FRoundMatchCore* Core = Round != nullptr ? Round->GetMatchCore() : nullptr;
				ASSERT_THAT(IsTrue(Core != nullptr, TEXT("the server's Round core")));
				const TArray<FRoundLogEntry>& Log = Core->GetLogArchive().GetCurrentEntries();
				TArray<FString> Order;
				FQueueKey Previous;
				bool bKeyOrder = true;
				int32 Stages = 0;
				int32 Finals = 0;
				for (const FRoundLogEntry& Entry : Log)
				{
					if (Entry.Type != ERoundLogType::ObjectiveStage && Entry.Type != ERoundLogType::FinalCompleted)
					{
						continue;
					}
					Stages += Entry.Type == ERoundLogType::ObjectiveStage ? 1 : 0;
					Finals += Entry.Type == ERoundLogType::FinalCompleted ? 1 : 0;
					Order.Add(FString::Printf(TEXT("%s(team %u)"), Entry.Type == ERoundLogType::ObjectiveStage ? TEXT("ObjectiveStage") : TEXT("FinalCompleted"), Entry.Data[0]));
					bKeyOrder &= !Previous.IsSet() || Previous < Entry.Key;
					Previous = Entry.Key;
				}
				TestRunner->AddInfo(FString::Printf(TEXT("OC21 log: %s; early final sightings on team 1's client %d"), *FString::Join(Order, TEXT(", ")), EarlyFinalSightings));
				ASSERT_THAT(IsTrue(Stages == 4 && Finals == 1, TEXT("exactly 4 ObjectiveStage and 1 FinalCompleted")));
				ASSERT_THAT(IsTrue(bKeyOrder, TEXT("in key order")));
				ASSERT_THAT(IsTrue(EarlyFinalSightings == 0, TEXT("team 1's client never held the final before its own preliminaries were complete")));
			});
	}
};

#endif
