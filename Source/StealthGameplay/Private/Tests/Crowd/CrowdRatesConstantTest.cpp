// Story cnb-013 (production/epics/crowd-npc-behaviour/story-013-identity-and-command-path-gate.md), AC-CNB-37: crowd rates
// never adapt during a round (GDD CNB-R7; ADR-0016 section 9).
//   RatesConstant  a 10-minute round of the headless scene in which scripted "players" run past the crowd every 2 s
//                  (far above 3 x their F2 r_P) and hide (concealed): the effective draw-rate table (every tell's
//                  compensated clock rate, the base long-pause share, the decision and steering rates, the hash of the
//                  frozen Crowd snapshot) logged every 60 s never changes.
//   NoWriteAfterReady  the Director refuses a Crowd tuning write while its round is Active (counted), and its core holds
//                  the snapshot as a const copy (no mutator exists).
// The story names a harness file (CrowdRatesConstantTest in StealthTestHarness); with no bot-player behaviour layer yet the
// players are scripted pawns of the headless scene (story deviation).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Server/CrowdDirector.h"
#include "Server/CrowdTellSampler.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningHasher.h"
#include "Server/StealthTuningVersion.h"
#include "Tests/Crowd/CrowdSimWorld.h"

#include <type_traits>

namespace CrowdRatesConstantTestData
{
	FString RateTable(StealthCrowdSim::FWorld& World)
	{
		const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
		FString Table;
		for (int32 Tell = 0; Tell < static_cast<int32>(ECrowdTell::Count); ++Tell)
		{
			Table += FString::Printf(TEXT("%.12f,"), FCrowdTellSampler::CompensatedRatePerMin(Tuning, static_cast<ECrowdTell>(Tell)));
		}
		FStealthServerTuning Snapshot;
		Snapshot.Crowd_Server = Tuning;
		Table += FString::Printf(TEXT("long %.3f decide %.3f steer %.3f yield %.3f hash %llx"), Tuning.CrowdLongPausePct, Tuning.CrowdDecisionPerSec, Tuning.CrowdSteerPerSec, Tuning.CrowdYieldPct,
			FTuningHasher::Hash(Snapshot, TuningSchemaVersion, false));
		return Table;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdRatesConstantTest, "StealthGame.Crowd.Identity.RatesConstant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdRatesConstantTest::RunTest(const FString& Parameters)
{
	using namespace CrowdRatesConstantTestData;
	using namespace StealthCrowdSim;
	FConfig Config;
	FWorld World(Config);
	World.SpawnNpcs();
	TArray<int32> Players;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Players.Add(World.AddScripted(FVector(-1500.0 + 300.0 * Index, 0.0, 0.0), 90.0f));
	}
	World.StartRound();
	const FString First = RateTable(World);
	int32 Changes = 0;
	int32 Tables = 1;
	for (int32 Second = 1; Second <= 600; ++Second)
	{
		// Bot players: run (a salient run start) every 2 s, the odd ones hidden half the time.
		if (Second % 2 == 0)
		{
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				FScripted& Player = World.GetScripted(Players[Index]);
				Player.Body.MoveIntent = FVector2D(0.0, 1.0);
				Player.Body.Gait = EGait::Run;
				Player.Body.bPerceivable = !(Index % 2 == 1 && (Second / 30) % 2 == 1);
				Player.Body.Control.Yaw = (Second * 37 + Index * 90) % 360;
				World.EmitSalient(Player.Body);
			}
		}
		World.RunUntil(static_cast<double>(Second));
		if (Second % 60 == 0)
		{
			++Tables;
			Changes += RateTable(World) != First ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("RATES %d tables over 10 min, %d changed; table: %s"), Tables, Changes, *First));
	TestEqual(TEXT("the effective rate table never changes during the round (AC-CNB-37)"), Changes, 0);
	TestTrue(TEXT("the frozen snapshot is a const copy in the core (no Crowd write path)"), std::is_const_v<std::remove_reference_t<decltype(World.GetCore().GetTuning())>>);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdNoTuningWriteAfterReadyTest, "StealthGame.Crowd.Identity.NoTuningWriteAfterReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdNoTuningWriteAfterReadyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CrowdTuningWriteWorld"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	UCrowdDirector* Director = UCrowdDirector::Get(World);
	if (TestNotNull(TEXT("Director"), Director))
	{
		FCrowdTuning_Server Changed;
		Changed.CrowdRunIntervalSec = 500.0f;
		TestTrue(TEXT("before the round a tuning set is accepted"), Director->SetTuningForRound(Changed));
		TArray<FVector> Corners;
		Director->RequestPath(FCrowdPathRequest(), Corners); // creates the round's core
		struct FNoStarter final : public ICrowdAgentStarter { virtual void StartAgent(uint32, const FCrowdFirstActivity&, double) override {} } Starter;
		Director->GetCore()->StartRound(0.0, true, Starter);
		FCrowdTuning_Server Later;
		Later.CrowdRunIntervalSec = 100.0f;
		TestFalse(TEXT("during the round a Crowd tuning write is refused"), Director->SetTuningForRound(Later));
		TestEqual(TEXT("and counted"), Director->GetRefusedTuningWrites(), 1);
		TestEqual(TEXT("the round keeps its frozen value"), Director->GetCrowdTuning().CrowdRunIntervalSec, 500.0f);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
