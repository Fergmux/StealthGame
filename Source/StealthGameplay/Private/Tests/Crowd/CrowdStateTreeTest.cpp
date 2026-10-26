// Story cnb-012 (production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md): the crowd
// StateTree schema, assets, decision grid and the per-frame due-time list (ADR-0016 section 3; engine facts E5, E6, E12,
// E14, E15; GDD States and Transitions, AC-CNB-33).
//   AssetsBootCheck  ST_CrowdFull and ST_CrowdRestricted load, are compiled on UCrowdStateTreeSchema with
//                    ScheduledTickPolicy Allowed (a stale or foreign tree fails the check), and the cvar is on; the
//                    state order gives the interrupt priority Halted > Reacting > the rest (Routing, Pausing, Halted only
//                    in the restricted tree).
//   GridWithEvents   50 crowd controllers in a game world with their trees on the 4 Hz phase grid, events sent to 20 % of
//                    the pawns each second for 20 s: every tree tick is on the pawn's grid or the one off-grid tick after
//                    an event (then back on the grid), the components tick in TG_PrePhysics after the Director and before
//                    the controller, and no frame holds more than ceil(N / (FPS / f)) + 2 decisions.
//   DueTimeAccuracy  a pause end acts in the controller tick in which it falls due (error at most one frame) at
//                    CrowdDecisionPerSec 4 and 3 (the headless scene).
//   DueTimeList      ordering, replace-by-kind and cancel of FCrowdDueTimeList.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/CrowdAssetKeys.h"
#include "Server/CrowdDirector.h"
#include "Server/CrowdDueTimeList.h"
#include "Server/CrowdStateTreeComponent.h"
#include "Server/CrowdStateTreeSchema.h"
#include "Server/CrowdStateTreeTypes.h"
#include "Server/StealthCrowdController.h"
#include "StateTree.h"
#include "Tests/Crowd/CrowdSimWorld.h"
#include "View/Core/StealthPawn.h"

namespace CrowdStateTreeTestData
{
	class FHooks final : public IStealthPawnFactoryHooks
	{
	public:
		virtual void ApplyAppearance(AStealthPawn* Pawn, int32 AppearanceId) override {}
		virtual ERosterResult BindPawn(FParticipantId Participant, AStealthPawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch) override { return ERosterResult::NoOp_NotBound; }
		virtual AController* CreateAiController(AStealthPawn* Pawn) override { return Pawn->GetWorld()->SpawnActor<AStealthCrowdController>(); }
	};

	TArray<FName> ChildNames(const UStateTree* Tree)
	{
		TArray<FName> Names;
		const TConstArrayView<FCompactStateTreeState> States = Tree->GetStates();
		if (States.Num() > 0)
		{
			for (uint16 Index = States[0].ChildrenBegin; Index < States[0].ChildrenEnd; Index = States[Index].ChildrenEnd)
			{
				Names.Add(States[Index].Name);
			}
		}
		return Names;
	}

	/** Records the pause end (due) and the next leg start of each NPC. */
	struct FDueRecorder final : public ICrowdAgentObserver
	{
		TMap<uint32, double> PauseDue;
		TArray<double> Lateness;
		virtual void OnPauseStarted(uint32 Ordinal, double Now, bool bLong, double LengthSec, bool, const FVector&) override { PauseDue.Add(Ordinal, Now + LengthSec); }
		virtual void OnWatchdog(uint32 Ordinal, double, double) override { PauseDue.Remove(Ordinal); }
		virtual void OnConverseDecision(uint32 Ordinal, double, uint64, bool, bool bDecided) override { if (bDecided) { PauseDue.Remove(Ordinal); } }
		virtual void OnLegStarted(uint32 Ordinal, double Now, float, double) override
		{
			if (const double* Due = PauseDue.Find(Ordinal))
			{
				Lateness.Add(Now - *Due);
				PauseDue.Remove(Ordinal);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStateTreeAssetsBootCheckTest, "StealthGame.Crowd.StateTree.AssetsBootCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStateTreeAssetsBootCheckTest::RunTest(const FString& Parameters)
{
	using namespace CrowdStateTreeTestData;
	const FCrowdAssetKeys Keys;
	const UStateTree* Full = Cast<UStateTree>(Keys.CrowdFullStateTreeAsset.TryLoad());
	const UStateTree* Restricted = Cast<UStateTree>(Keys.CrowdRestrictedStateTreeAsset.TryLoad());
	if (!TestNotNull(TEXT("ST_CrowdFull loads"), Full) || !TestNotNull(TEXT("ST_CrowdRestricted loads"), Restricted))
	{
		return false;
	}
	TestTrue(TEXT("ST_CrowdFull is compiled on the crowd schema with scheduled ticks allowed"), UCrowdStateTreeSchema::IsCrowdTree(Full));
	TestTrue(TEXT("ST_CrowdRestricted too"), UCrowdStateTreeSchema::IsCrowdTree(Restricted));
	TestTrue(TEXT("the schema sets ScheduledTickPolicy Allowed itself"), GetDefault<UCrowdStateTreeSchema>()->AllowsScheduledTick());
	TestTrue(TEXT("StateTree.Component.ScheduledTickEnabled is on"), UCrowdDirector::IsScheduledTickEnabled());
	// A stale or foreign tree fails the boot check (negative control: an uncompiled tree).
	const UStateTree* Stale = NewObject<UStateTree>();
	TestFalse(TEXT("an uncompiled tree fails the check"), UCrowdStateTreeSchema::IsCrowdTree(Stale));

	const TArray<FName> FullOrder = ChildNames(Full);
	AddInfo(FString::Printf(TEXT("STATETREE full: %s"), *FString::JoinBy(FullOrder, TEXT(" "), [](FName Name) { return Name.ToString(); })));
	if (TestTrue(TEXT("the full tree has the 11 GDD states"), FullOrder.Num() == 11))
	{
		TestEqual(TEXT("Halted first (highest priority)"), FullOrder[0], FName(TEXT("Halted")));
		TestEqual(TEXT("then Reacting"), FullOrder[1], FName(TEXT("Reacting")));
	}
	const TArray<FName> RestrictedOrder = ChildNames(Restricted);
	TestTrue(TEXT("the restricted tree is Halted, Routing, Pausing"), RestrictedOrder == TArray<FName>({ FName(TEXT("Halted")), FName(TEXT("Routing")), FName(TEXT("Pausing")) }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStateTreeGridWithEventsTest, "StealthGame.Crowd.StateTree.GridWithEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStateTreeGridWithEventsTest::RunTest(const FString& Parameters)
{
	using namespace CrowdStateTreeTestData;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CrowdStateTreeGridWorld"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay(); // no game mode: dispatch BeginPlay so actor and component ticks register
	const uint64 CachedFrameCounter = GFrameCounter;
	// As FTestWorldWrapper::DestroyTestWorld: a world that began play ends play before it is destroyed, and the frame
	// counter the manual ticks advanced is restored (a begun world destroyed without EndPlay crashed a later map load).
	const auto Teardown = [World, CachedFrameCounter]()
	{
		World->BeginTearingDown();
		World->EndPlay(EEndPlayReason::Quit);
		GFrameCounter = CachedFrameCounter;
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	FHooks Hooks;
	UStealthPawnFactory* Factory = UStealthPawnFactory::Get(World);
	Factory->SetHooks(&Hooks, World);
	Factory->SetDefaultPawnClass(AStealthPawn::StaticClass());
	UCrowdDirector* Director = UCrowdDirector::Get(World);
	const FCrowdAssetKeys Keys;
	UStateTree* Tree = Cast<UStateTree>(Keys.CrowdFullStateTreeAsset.TryLoad());

	constexpr int32 Npcs = 50;
	TArray<AStealthCrowdController*> Controllers;
	for (int32 Index = 0; Index < Npcs; ++Index)
	{
		FStealthSpawnRequest Request;
		Request.Transform = FTransform(FVector(300.0 * (Index % 10), 300.0 * (Index / 10), AStealthPawn::SampleCapsuleHalfHeightCm + 2.0));
		Request.ControlKind = EStealthPawnControlKind::Ai;
		if (AStealthPawn* Pawn = Factory->SpawnSharedPawn(Request).Pawn)
		{
			Controllers.Add(Cast<AStealthCrowdController>(Pawn->GetController()));
		}
	}
	bool bOrder = Controllers.Num() == Npcs && Tree != nullptr;
	for (AStealthCrowdController* Controller : Controllers)
	{
		bOrder &= Controller != nullptr && Controller->VerifyTickOrder();
	}
	TestTrue(TEXT("50 crowd controllers with their trees; Director -> tree -> controller prerequisites (TG_PrePhysics)"), bOrder);
	if (!bOrder)
	{
		Teardown();
		return false;
	}
	// Start every pawn as the Director's StartRound does (the bare world has no navmesh, so Director::StartRound would
	// refuse with NavNotReady): phases stratified by the core, one grid per pawn.
	FCrowdDirectorCore* Core = Director->GetCore();
	const double Start = World->GetTimeSeconds();
	struct FStarter final : public ICrowdAgentStarter
	{
		UCrowdDirector* Director = nullptr;
		virtual void StartAgent(uint32 Ordinal, const FCrowdFirstActivity& First, double Now) override { Director->StartAgent(Ordinal, First, Now); }
	} Starter;
	Starter.Director = Director;
	Core->StartRound(Start, /*bNavReady*/ true, Starter);
	const double Period = Core->GetDecisionPeriod();

	int32 OffGridNotAfterEvent = 0;
	int32 TreeTicks = 0;
	int32 Events = 0;
	TArray<int64> LastTicks;
	TArray<bool> EventPending;
	LastTicks.SetNumZeroed(Controllers.Num());
	EventPending.SetNumZeroed(Controllers.Num());
	int32 MaxDecisions = 0;
	const int32 Frames = 20 * 60;
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		// Events on 20 % of the pawns each second: 10 a second spread over the frames.
		if (Frame % 6 == 0)
		{
			const int32 Target = (Frame / 6 * 7) % Controllers.Num();
			Controllers[Target]->GetCrowdStateTree()->SendCrowdEvent(TAG_Crowd_Stimulus_Salient);
			EventPending[Target] = true;
			++Events;
		}
		++GFrameCounter; // one engine frame per step: the tick manager queues a tick function once per frame number
		World->Tick(LEVELTICK_All, 1.0f / 60.0f);
		MaxDecisions = FMath::Max(MaxDecisions, Director->GetDecisionStats().Frame == Director->GetDirectorFrame() ? Director->GetDecisionStats().ThisFrame : 0);
		for (int32 Index = 0; Index < Controllers.Num(); ++Index)
		{
			const UCrowdStateTreeComponent* Component = Controllers[Index]->GetCrowdStateTree();
			if (Component->GetTreeTickCount() != LastTicks[Index])
			{
				LastTicks[Index] = Component->GetTreeTickCount();
				++TreeTicks;
				if (!Component->WasLastTickOnGrid())
				{
					if (EventPending[Index] || Component->WasLastTickWake())
					{
						EventPending[Index] = false; // the one off-grid tick of the event
					}
					else
					{
						++OffGridNotAfterEvent;
					}
				}
				else
				{
					EventPending[Index] = false;
				}
			}
		}
	}
	{
		int32 Running = 0, TickEnabled = 0;
		int64 Skipped = 0, Wakes = 0;
		for (AStealthCrowdController* Controller : Controllers)
		{
			const UCrowdStateTreeComponent* Component = Controller->GetCrowdStateTree();
			Running += Component->IsRunning() ? 1 : 0;
			TickEnabled += Component->IsComponentTickEnabled() ? 1 : 0;
			Skipped += Component->GetSkippedOffGridTicks();
			Wakes += Component->GetWakeTickCount();
		}
		AddInfo(FString::Printf(TEXT("GRIDDIAG running %d, tick enabled %d, skipped off-grid %lld, wake ticks %lld, interval %.4f, now %.3f, start %.3f, tree %s"),
			Running, TickEnabled, Skipped, Wakes, Controllers[0]->GetCrowdStateTree()->GetComponentTickInterval(), World->GetTimeSeconds(), Start, Tree != nullptr ? *Tree->GetName() : TEXT("none")));
	}
	const int32 Bound = Core->DecisionBoundPerFrame(60.0);
	AddInfo(FString::Printf(TEXT("GRID %d tree ticks over 20 s for %d pawns (%.2f Hz each), %d events, off-grid ticks not explained by an event %d, max decisions in a frame %d (bound %d), %lld fallback decisions"),
		TreeTicks, Npcs, TreeTicks / 20.0 / Npcs, Events, OffGridNotAfterEvent, MaxDecisions, Bound, Controllers[0]->GetFallbackDecisions()));
	TestTrue(TEXT("the trees run on the grid (about 4 Hz each)"), TreeTicks >= Npcs * 20 * 4 * 0.9);
	TestEqual(TEXT("every off-grid tick follows an event, and the pawn returns to its grid"), OffGridNotAfterEvent, 0);
	TestTrue(TEXT("no frame holds more decisions than the bound, events included"), MaxDecisions <= Bound);
	TestEqual(TEXT("no fallback decision (the tree governs)"), Controllers[0]->GetFallbackDecisions(), int64(0));

	Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStateTreeDueTimeAccuracyTest, "StealthGame.Crowd.StateTree.DueTimeAccuracy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStateTreeDueTimeAccuracyTest::RunTest(const FString& Parameters)
{
	using namespace CrowdStateTreeTestData;
	using namespace StealthCrowdSim;
	for (const float Rate : { 4.0f, 3.0f })
	{
		FConfig Config;
		Config.NpcCount = 30;
		Config.Tuning.CrowdDecisionPerSec = Rate;
		FWorld World(Config);
		FDueRecorder Recorder;
		World.SetObserver(&Recorder);
		World.SpawnNpcs();
		World.StartRound();
		World.RunUntil(180.0);
		double Worst = 0.0;
		double Earliest = 0.0;
		for (const double Late : Recorder.Lateness)
		{
			Worst = FMath::Max(Worst, Late);
			Earliest = FMath::Min(Earliest, Late);
		}
		AddInfo(FString::Printf(TEXT("DUETIME %.0f Hz: %d pause ends, latest %.4f s, earliest %.4f s"), Rate, Recorder.Lateness.Num(), Worst, Earliest));
		TestTrue(FString::Printf(TEXT("pause ends measured at %.0f Hz"), Rate), Recorder.Lateness.Num() > 100);
		TestTrue(FString::Printf(TEXT("a pause end acts within one frame of its due time at %.0f Hz"), Rate), Worst <= Config.StepSec + 1e-6 && Earliest >= -1e-6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDueTimeListTest, "StealthGame.Crowd.StateTree.DueTimeList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDueTimeListTest::RunTest(const FString& Parameters)
{
	FCrowdDueTimeList List;
	List.Schedule(ECrowdDueKind::GlanceEnd, 2.0);
	List.Schedule(ECrowdDueKind::PauseEnd, 1.0);
	List.Schedule(ECrowdDueKind::ReactionStart, 1.0, 7);
	List.Schedule(ECrowdDueKind::PauseEnd, 3.0); // replaces the earlier pause end
	FCrowdDueEntry Entry;
	TestFalse(TEXT("nothing is due at 0.5"), List.PopDue(0.5, Entry));
	TestTrue(TEXT("the reaction is due at 1.0"), List.PopDue(1.0, Entry) && Entry.Kind == ECrowdDueKind::ReactionStart && Entry.Payload == 7);
	TestFalse(TEXT("the replaced pause end is gone"), List.PopDue(1.5, Entry));
	TestTrue(TEXT("the glance end at 2.0"), List.PopDue(2.5, Entry) && Entry.Kind == ECrowdDueKind::GlanceEnd);
	List.Cancel(ECrowdDueKind::PauseEnd);
	TestEqual(TEXT("cancel removes the kind"), List.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
