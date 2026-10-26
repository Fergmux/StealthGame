// The crowd's PIE tests on the production crowd in L_Prototype (listen server + clients; the composition root spawns the
// round's NPCs through the pawn factory with AStealthCrowdController, D53 integration):
//   StealthGame.Crowd.PIE.LookPitchReachesObserver   story cnb-006: a crowd Look of pitch -15 reaches the pawn's replicated
//       view pitch on an observer client (RemoteViewPitch, through the engine's compression; the SCL HeadLook filter,
//       story SCL-021, does not exist yet, so the replicated pitch is read directly).
//   StealthGame.Crowd.PIE.EngineVerifications        spike cnb-001 (ADR-0016 verifications 2, 3, 6, 7): no path following
//       and no ensure under the visual logger for 60 s of routed motion; every crowd tree back on its grid after events;
//       FindPathAsync results at least 2 frames after the start (p99 recorded); identical default components on an
//       AI-driven, a player-driven and a just-handed-over pawn, UGameplayTasksComponent present and non-replicated;
//       StateTree.Component.ScheduledTickEnabled on.
//   StealthGame.Crowd.PIE.CommandPath                story cnb-013 (AC-CNB-02, AC-CNB-11, runtime half): over the run every
//       crowd motion arrives through the Move/Look entry points (sink counters), no RequestDirectMove / RequestPathMove
//       backstop fires, no crowd controller has a path-following component, no crowd discrete command or Attack is sent
//       and no stale-epoch drop happens. Duration: Stealth.Crowd.CommandPathTestSec (45 s per commit; 600 s for the
//       evidence run).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "EngineUtils.h"
#include "GameplayTasksComponent.h"
#include "HAL/IConsoleManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "Server/CrowdDirector.h"
#include "Server/CrowdStateTreeComponent.h"
#include "Server/CrowdStateTreeTypes.h"
#include "Server/StealthCrowdController.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"
#include "VisualLogger/VisualLogger.h"

namespace CrowdPIETestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("600");
	TAutoConsoleVariable<float> CVarCommandPathSec(TEXT("Stealth.Crowd.CommandPathTestSec"), 45.0f, TEXT("Duration of StealthGame.Crowd.PIE.CommandPath (seconds)."));

	UWorld* ServerWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_ListenServer)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	UWorld* ClientWorld()
	{
		const FWorldContext* Best = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Client && (!Best || Context.PIEInstance < Best->PIEInstance))
			{
				Best = &Context;
			}
		}
		return Best ? Best->World() : nullptr;
	}

	bool RoundActive()
	{
		const UWorld* Client = ClientWorld();
		const AStealthGameState* State = Client ? Client->GetGameState<AStealthGameState>() : nullptr;
		return ServerWorld() != nullptr && State != nullptr && State->GetRoundPublicState().Phase == ERoundPhase::Active;
	}

	TArray<AStealthCrowdController*> CrowdControllers(UWorld* World)
	{
		TArray<AStealthCrowdController*> Out;
		if (World)
		{
			for (TActorIterator<AStealthCrowdController> It(World); It; ++It)
			{
				if (It->GetPawn() != nullptr)
				{
					Out.Add(*It);
				}
			}
		}
		return Out;
	}

	TArray<FString> Inventory(const AActor& Actor)
	{
		TArray<FString> Rows;
		for (const UActorComponent* Component : Actor.GetComponents())
		{
			Rows.Add(FString::Printf(TEXT("%s/%s/%d"), *Component->GetClass()->GetName(), *Component->GetName(), Component->GetIsReplicated() ? 1 : 0));
		}
		Rows.Sort();
		return Rows;
	}

	/** Shared timeout raise for the long crowd runs. */
	struct FTimeout
	{
		FString Previous;
		void Raise()
		{
			if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
			{
				Previous = Timeout->GetString();
				Timeout->Set(NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
			}
		}
		void Restore()
		{
			if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !Previous.IsEmpty())
			{
				Timeout->Set(*Previous, ECVF_SetByCode);
			}
		}
	};
}

NETWORK_TEST_CLASS(LookPitchReachesObserver, "StealthGame.Crowd.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static CrowdPIETestData::FTimeout Timeout;
	TWeakObjectPtr<AStealthCrowdController> Subject;
	uint64 Mark = 0;

	BEFORE_ALL() { Timeout.Raise(); }
	AFTER_ALL() { Timeout.Restore(); }

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(1).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(CrowdPIETestData::FixtureMap);
	}

	TEST_METHOD(CrowdLookPitchReplicates)
	{
		using namespace CrowdPIETestData;
		Network
			.Until(TEXT("the round is Active with a crowd"), []() { return RoundActive() && CrowdControllers(ServerWorld()).Num() > 0; })
			.Then(TEXT("look down -15 on one NPC"), [this]()
			{
				TArray<AStealthCrowdController*> Controllers = CrowdControllers(ServerWorld());
				Subject = Controllers[0];
				Subject->GetAgent().SetLookOverride(FRotator(-15.0, 40.0, 0.0));
				Mark = GFrameCounter;
			})
			.Until(TEXT("replication settles"), [this]() { return GFrameCounter >= Mark + 90; })
			.Then(TEXT("read the observer's view of the pitch"), [this]()
			{
				ASSERT_THAT(IsTrue(Subject.IsValid(), TEXT("the subject controller")));
				const APawn* OnServer = Subject->GetPawn();
				ASSERT_THAT(IsTrue(OnServer != nullptr, TEXT("the subject pawn")));
				const float ServerPitch = static_cast<float>(FRotator::NormalizeAxis(Subject->GetControlRotation().Pitch));
				// The same pawn on the observer client: the nearest Shared Pawn to the server location.
				const AStealthPawn* OnClient = nullptr;
				double Best = 1e18;
				for (TActorIterator<AStealthPawn> It(CrowdPIETestData::ClientWorld()); It; ++It)
				{
					const double D = FVector::DistSquared(It->GetActorLocation(), OnServer->GetActorLocation());
					if (D < Best)
					{
						Best = D;
						OnClient = *It;
					}
				}
				ASSERT_THAT(IsTrue(OnClient != nullptr, TEXT("the subject on the observer client")));
				const float ObservedPitch = static_cast<float>(FRotator::NormalizeAxis(OnClient->GetBaseAimRotation().Pitch));
				TestRunner->AddInfo(FString::Printf(TEXT("LOOKPITCH server control pitch %.2f, observer view pitch %.2f (match distance %.1f cm)"), ServerPitch, ObservedPitch, FMath::Sqrt(Best)));
				ASSERT_THAT(IsNear(-15.0f, ServerPitch, 0.01f, TEXT("the crowd controller keeps the Look pitch")));
				ASSERT_THAT(IsNear(-15.0f, ObservedPitch, 1.5f, TEXT("the observer sees the pitch (RemoteViewPitch, 8-bit compression)")));
				Subject->GetAgent().SetLookOverride(TOptional<FRotator>());
			});
	}
};

NETWORK_TEST_CLASS(EngineVerifications, "StealthGame.Crowd.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static CrowdPIETestData::FTimeout Timeout;
	uint64 Mark = 0;
	TArray<TWeakObjectPtr<AStealthCrowdController>> Tracked;
	TArray<int64> LastTicks;
	TArray<bool> EventPending;
	int32 OffGridNotAfterEvent = 0;
	int32 TreeTicks = 0;
	int32 WakeTicks = 0;
	int32 Events = 0;
	double MarkTime = 0.0;

	BEFORE_ALL() { Timeout.Raise(); }
	AFTER_ALL() { Timeout.Restore(); FVisualLogger::Get().SetIsRecording(false); }

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(1).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(CrowdPIETestData::FixtureMap);
	}

	TEST_METHOD(CrowdEngineFactsHold)
	{
		using namespace CrowdPIETestData;
		Network
			.Until(TEXT("the round is Active with a crowd"), []() { return RoundActive() && CrowdControllers(ServerWorld()).Num() >= 10; })
			.Then(TEXT("component parity and the scheduled-tick cvar"), [this]()
			{
				TestRunner->TestTrue(TEXT("StateTree.Component.ScheduledTickEnabled is on"), UCrowdDirector::IsScheduledTickEnabled());
				UWorld* Server = ServerWorld();
				const TArray<AStealthCrowdController*> Controllers = CrowdControllers(Server);
				const AStealthPawn* Npc = Cast<AStealthPawn>(Controllers[0]->GetPawn());
				// The player-driven pawn: the client's pawn on the server.
				AStealthPawn* PlayerPawn = nullptr;
				APlayerController* PlayerPC = nullptr;
				for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
				{
					if (!It->Get()->IsLocalController() && Cast<AStealthPawn>(It->Get()->GetPawn()))
					{
						PlayerPC = It->Get();
						PlayerPawn = Cast<AStealthPawn>(PlayerPC->GetPawn());
					}
				}
				ASSERT_THAT(IsTrue(Npc != nullptr, TEXT("an AI-driven pawn")));
				ASSERT_THAT(IsTrue(PlayerPawn != nullptr, TEXT("a player-driven pawn")));
				const TArray<FString> NpcRows = Inventory(*Npc);
				const TArray<FString> PlayerRows = Inventory(*PlayerPawn);
				// The just-handed-over pawn: the player pawn taken over by a crowd controller, inventoried in its first frame.
				PlayerPC->UnPossess();
				AStealthCrowdController* Takeover = Server->SpawnActor<AStealthCrowdController>();
				Takeover->Possess(PlayerPawn);
				const TArray<FString> HandedOverRows = Inventory(*PlayerPawn);
				TestRunner->AddInfo(FString::Printf(TEXT("ENGINEVERIFY inventory AI %d, player %d, handed over %d components"), NpcRows.Num(), PlayerRows.Num(), HandedOverRows.Num()));
				TestRunner->TestTrue(TEXT("AI-driven and player-driven pawns have identical components"), NpcRows == PlayerRows);
				TestRunner->TestTrue(TEXT("the just-handed-over pawn too"), HandedOverRows == PlayerRows);
				for (const AStealthPawn* Pawn : { Npc, static_cast<const AStealthPawn*>(PlayerPawn) })
				{
					const UGameplayTasksComponent* Tasks = Pawn->FindComponentByClass<UGameplayTasksComponent>();
					TestRunner->TestTrue(TEXT("UGameplayTasksComponent present"), Tasks != nullptr);
					TestRunner->TestTrue(TEXT("and not replicated"), Tasks != nullptr && !Tasks->GetIsReplicated());
				}
				Takeover->UnPossess();
				PlayerPC->Possess(PlayerPawn);
				Takeover->Destroy();
			})
			.Then(TEXT("visual logger on; track every tree"), [this]()
			{
				FVisualLogger::Get().SetIsRecording(true);
				for (AStealthCrowdController* Controller : CrowdControllers(ServerWorld()))
				{
					Tracked.Add(Controller);
					LastTicks.Add(Controller->GetCrowdStateTree()->GetTreeTickCount());
					EventPending.Add(false);
				}
				Mark = GFrameCounter;
				MarkTime = ServerWorld()->GetTimeSeconds();
			})
			.Until(TEXT("60 s of routed motion with events on 20 % of the pawns each second"), [this]()
			{
				const uint64 Frame = GFrameCounter - Mark;
				if (Frame % 6 == 0 && Tracked.Num() > 0)
				{
					const int32 Target = static_cast<int32>((Frame / 6 * 7) % Tracked.Num());
					if (AStealthCrowdController* Controller = Tracked[Target].Get())
					{
						Controller->GetCrowdStateTree()->SendCrowdEvent(TAG_Crowd_Stimulus_Salient);
						EventPending[Target] = true;
						++Events;
					}
				}
				for (int32 Index = 0; Index < Tracked.Num(); ++Index)
				{
					const AStealthCrowdController* Controller = Tracked[Index].Get();
					if (Controller == nullptr)
					{
						continue;
					}
					const UCrowdStateTreeComponent* Component = Controller->GetCrowdStateTree();
					if (Component->GetTreeTickCount() != LastTicks[Index])
					{
						LastTicks[Index] = Component->GetTreeTickCount();
						++TreeTicks;
						WakeTicks += Component->WasLastTickWake() ? 1 : 0;
						if (!Component->WasLastTickOnGrid() && !EventPending[Index] && !Component->WasLastTickWake())
						{
							++OffGridNotAfterEvent;
						}
						EventPending[Index] = false;
					}
				}
				return GFrameCounter >= Mark + 60 * 60;
			})
			.Then(TEXT("report"), [this]()
			{
				FVisualLogger::Get().SetIsRecording(false);
				const UCrowdDirector* Director = UCrowdDirector::Get(ServerWorld());
				ASSERT_THAT(IsTrue(Director != nullptr, TEXT("Director")));
				int32 PathFollowing = 0;
				for (AStealthCrowdController* Controller : CrowdControllers(ServerWorld()))
				{
					PathFollowing += Controller->FindComponentByClass<UPathFollowingComponent>() != nullptr ? 1 : 0;
				}
				TArray<int32> Latency(Director->GetPathLatencyFrames().GetData(), Director->GetPathLatencyFrames().Num());
				Latency.Sort();
				const int32 MinLatency = Latency.Num() > 0 ? Latency[0] : -1;
				const int32 P99 = Latency.Num() > 0 ? Latency[FMath::Min(Latency.Num() - 1, FMath::FloorToInt32(0.99 * Latency.Num()))] : -1;
				TestRunner->AddInfo(FString::Printf(TEXT("ENGINEVERIFY %d crowd controllers, path-following components %d; tree ticks %d, events %d, off-grid ticks not after an event %d; max decisions in a frame %d (bound %d); path results %d, latency min %d frames, p99 %d frames, max path starts in a frame %d"),
					CrowdControllers(ServerWorld()).Num(), PathFollowing, TreeTicks, Events, OffGridNotAfterEvent, Director->GetDecisionStats().MaxPerFrame,
					Director->GetCore() ? Director->GetCore()->DecisionBoundPerFrame(60.0) : -1, Latency.Num(), MinLatency, P99, Director->GetMaxPathStartsInAFrame()));
				TestRunner->AddInfo(FString::Printf(TEXT("ENGINEVERIFY %d wake or event ticks over %.1f s of world time"), WakeTicks, ServerWorld()->GetTimeSeconds() - MarkTime));
				TestRunner->TestEqual(TEXT("no crowd controller has a path-following component (verification 2)"), PathFollowing, 0);
				TestRunner->TestEqual(TEXT("every off-grid tree tick follows an event (verification 3)"), OffGridNotAfterEvent, 0);
				TestRunner->TestTrue(TEXT("path results were delivered"), Latency.Num() > 0);
				TestRunner->TestTrue(TEXT("every FindPathAsync result arrives at least 2 frames after its start (verification 6)"), MinLatency >= 2);
				TestRunner->TestTrue(TEXT("at most 2 path starts per frame"), Director->GetMaxPathStartsInAFrame() <= 2);
			});
	}
};

NETWORK_TEST_CLASS(CommandPath, "StealthGame.Crowd.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static CrowdPIETestData::FTimeout Timeout;
	uint64 Mark = 0;
	TUniquePtr<FStealthRecordingFaultSink> Faults;

	BEFORE_ALL() { Timeout.Raise(); }
	AFTER_ALL() { Timeout.Restore(); }

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(1).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(CrowdPIETestData::FixtureMap);
	}

	AFTER_EACH()
	{
		for (TActorIterator<AStealthPawn> It(CrowdPIETestData::ServerWorld()); It; ++It)
		{
			if (UStealthMovementComponent* Movement = It->GetStealthMovement())
			{
				Movement->SetFaultSink(nullptr);
			}
		}
		Faults.Reset();
	}

	TEST_METHOD(CrowdMotionOnlyThroughTheCommandInterface)
	{
		using namespace CrowdPIETestData;
		Network
			.Until(TEXT("the round is Active with a crowd"), []() { return RoundActive() && CrowdControllers(ServerWorld()).Num() >= 10; })
			.Then(TEXT("instrument the movement backstops"), [this]()
			{
				Faults = MakeUnique<FStealthRecordingFaultSink>();
				for (AStealthCrowdController* Controller : CrowdControllers(ServerWorld()))
				{
					if (UStealthMovementComponent* Movement = Cast<AStealthPawn>(Controller->GetPawn())->GetStealthMovement())
					{
						Movement->SetFaultSink(Faults.Get());
					}
				}
				Mark = GFrameCounter;
			})
			.Until(TEXT("the run"), [this]() { return GFrameCounter >= Mark + static_cast<uint64>(60.0f * CVarCommandPathSec.GetValueOnGameThread()); })
			.Then(TEXT("assert the command path"), [this]()
			{
				int64 Passed = 0, Discrete = 0, Stale = 0, PathFollowing = 0;
				const TArray<AStealthCrowdController*> Controllers = CrowdControllers(ServerWorld());
				for (const AStealthCrowdController* Controller : Controllers)
				{
					Passed += Controller->GetSinkCounters().Passed;
					Discrete += Controller->GetSinkCounters().DiscretePassed;
					Stale += Controller->GetSinkCounters().StaleEpochDrop;
					PathFollowing += Controller->FindComponentByClass<UPathFollowingComponent>() != nullptr ? 1 : 0;
				}
				const int32 Backstops = Faults.IsValid() ? Faults->GetRecords().Num() : 0;
				TestRunner->AddInfo(FString::Printf(TEXT("COMMANDPATH %.0f s, %d crowd controllers: %lld Move/Look calls through the sinks, %lld discrete, %lld stale-epoch drops, %d movement backstop faults, %lld path-following components"),
					CVarCommandPathSec.GetValueOnGameThread(), Controllers.Num(), Passed, Discrete, Stale, Backstops, PathFollowing));
				ASSERT_THAT(IsTrue(Passed > 1000, TEXT("the crowd moved through the Move/Look entry points")));
				ASSERT_THAT(AreEqual(0, Backstops, TEXT("0 RequestDirectMove / RequestPathMove backstop hits (AC-CNB-02)")));
				ASSERT_THAT(AreEqual(int64(0), PathFollowing, TEXT("0 path-following components")));
				ASSERT_THAT(AreEqual(int64(0), Discrete, TEXT("0 crowd discrete commands in this catalogue (no Attack can exist, AC-CNB-11)")));
				ASSERT_THAT(AreEqual(int64(0), Stale, TEXT("no stale-epoch drop without a hand-over")));
			});
	}
};

#endif
