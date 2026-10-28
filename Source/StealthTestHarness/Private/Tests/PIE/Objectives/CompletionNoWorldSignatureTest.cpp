// Objectives Chain story oc-020 AC2 (AC-OC-12 [PIE-2]): StealthGame.Objectives.PIE.CompletionNoWorldSignature.
// Listen server + 2 clients on L_Prototype (NpcCount 0). Team 0's remote client's pawn tends twice through the production
// hold (server-side submission with its own origin): a counted completion at its live P1[0], then a no-effect completion at the
// final (locked for it). On the opponent's client (team 1's remote client) every replicated property of every replicated actor
// and component is recorded per frame over the 2 s around each completion (the in-process stand-in for the wire trace: a value
// change of a replicated field is a received field; RPCs: no Objectives header declares one, so none can arrive):
//   - zero received fields whose owning class or struct is on the Objectives list (UObjectiveTeamViewComponent,
//     FObjectiveTeamView, or any Objectives / court / site name), in both cases; the opponent's own view component receives no
//     change in either window;
//   - the remaining received fields (pawn pose and movement, the engine's clock and ping fields) are the same set of field
//     names in both cases (values ignored).
// Court fixtures are not replicated at all (AStealthCourtSocket), so no court or fixture property can change.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "Tests/PIE/Objectives/ObjectivesPieFixture.h"

NETWORK_TEST_CLASS(CompletionNoWorldSignature, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static StealthObjectivesPie::FScopedPieSettings Settings;

	TWeakObjectPtr<AStealthPlayerController> Tender;
	TWeakObjectPtr<AStealthPlayerController> Opponent;
	StealthObjectivesPie::FCourts Courts;
	StealthObjectivesPie::FFieldRecorder Recorder;
	TSet<FString> Window[2];
	int32 OpponentChanges[2] = { 0, 0 };
	double DueTime = 0.0;
	double Mark = 0.0;
	uint32 Seq = 0x00400000u;

	BEFORE_ALL() { Settings.Apply(); }
	AFTER_ALL() { Settings.Restore(); }

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	/** Role 0: team 0's live P1 (counted); role 1: the final, locked for team 0 (no effect). Resolved when the step runs. */
	FCourtFixtureId CourtOf(int32 Role) const { return Role == 0 ? Courts.P1[0] : Courts.Final; }

	void Tend(int32 Case)
	{
		using namespace StealthObjectivesPie;
		Network
			.Then(*FString::Printf(TEXT("case %d: place the tender"), Case), [this, Case]()
			{
				PlaceAtCourt(Tender->GetPawn(), CourtOf(Case));
				Mark = ServerNow(ServerWorld());
			})
			.Until(*FString::Printf(TEXT("case %d: settle"), Case), [this]() { return ServerNow(ServerWorld()) >= Mark + 2.0; })
			.Then(*FString::Printf(TEXT("case %d: Begin"), Case), [this, Case]()
			{
				BeginTendAsOwner(Cast<AStealthPawn>(Tender->GetPawn()), CourtOf(Case), Seq);
				DueTime = ServerNow(ServerWorld()) + Service()->GetInteractHoldSec();
			})
			.Until(*FString::Printf(TEXT("case %d: to 1 s before the completion"), Case), [this]() { return ServerNow(ServerWorld()) >= DueTime - 1.0; })
			.Then(*FString::Printf(TEXT("case %d: open the window"), Case), [this, Case]()
			{
				Recorder.BeginWindow();
				OpponentChanges[Case] = Opponent->GetObjectiveTeamView()->GetClientChangeCount();
			})
			.Until(*FString::Printf(TEXT("case %d: record 2 s around the completion"), Case), [this]()
			{
				Recorder.Sample();
				return ServerNow(ServerWorld()) >= DueTime + 1.0;
			})
			.Then(*FString::Printf(TEXT("case %d: close the window"), Case), [this, Case]()
			{
				Window[Case] = Recorder.Changed;
				OpponentChanges[Case] = Opponent->GetObjectiveTeamView()->GetClientChangeCount() - OpponentChanges[Case];
			});
	}

	TEST_METHOD(CountedAndNoEffectCompletionsLookTheSameToTheOpponent)
	{
		using namespace StealthObjectivesPie;
		Network.Until(TEXT("Active, one remote client per team"), [this]()
		{
			if (!RoundActive() || Service() == nullptr)
			{
				return false;
			}
			for (AStealthPlayerController* Player : ServerPlayers())
			{
				if (Player->IsLocalController())
				{
					continue;
				}
				if (TeamOf(Player) == 0)
				{
					Tender = Player; // the server's copy: the tend is submitted with its own origin
				}
				else if (TeamOf(Player) == 1)
				{
					Opponent = OwnerMachineController(Player); // the opponent's own client world
				}
			}
			Courts = BoundCourts();
			if (Opponent.IsValid())
			{
				Recorder.World = Opponent->GetWorld();
			}
			return Courts.IsSet() && Tender.IsValid() && Opponent.IsValid();
		});
		Tend(0);
		Network.Then(TEXT("the counted completion landed"), [this]()
		{
			ASSERT_THAT(IsTrue(Service()->GetObjectiveTeamStage(0) == EObjectiveTeamStage::Stage2, TEXT("team 0's P1 counted")));
		});
		Tend(1);
		Network.Then(TEXT("assert"), [this]()
		{
			const TSet<FString> Volatile = { TEXT("StealthGameState.ReplicatedWorldTimeSecondsDouble"), TEXT("StealthGameState.ReplicatedWorldTimeSeconds"),
				TEXT("StealthPlayerState.CompressedPing"), TEXT("StealthPlayerState.PingInMilliseconds") };
			for (int32 Case = 0; Case < 2; ++Case)
			{
				TArray<FString> Fields = Window[Case].Array();
				Fields.Sort();
				TestRunner->AddInfo(FString::Printf(TEXT("OC20 %s window: %d field(s) received on the opponent: %s"), Case == 0 ? TEXT("counted") : TEXT("no-effect"), Fields.Num(),
					*FString::Join(Fields, TEXT(", "))));
				const TArray<FString> Objectives = Fields.FilterByPredicate([](const FString& F) { return StealthObjectivesPie::IsObjectivesField(F); });
				ASSERT_THAT(IsTrue(Objectives.Num() == 0, *FString::Printf(TEXT("%s: no Objectives-list field reached the opponent"), Case == 0 ? TEXT("counted") : TEXT("no-effect"))));
				ASSERT_THAT(IsTrue(OpponentChanges[Case] == 0, TEXT("the opponent's own view component received no change")));
			}
			TSet<FString> A = Window[0].Difference(Volatile);
			TSet<FString> B = Window[1].Difference(Volatile);
			const TSet<FString> OnlyA = A.Difference(B);
			const TSet<FString> OnlyB = B.Difference(A);
			TestRunner->AddInfo(FString::Printf(TEXT("OC20 field-name difference: counted-only {%s}, no-effect-only {%s}"), *FString::Join(OnlyA, TEXT(", ")), *FString::Join(OnlyB, TEXT(", "))));
			ASSERT_THAT(IsTrue(OnlyA.Num() == 0 && OnlyB.Num() == 0, TEXT("the same set of field names in both windows")));
		});
	}
};

#endif
