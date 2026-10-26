// Story cnb-016 (production/epics/crowd-npc-behaviour/story-016-gaze-catalogue.md): the gaze catalogue G1-G7 (GDD CNB-R6,
// R7 head-hold clause, AC-CNB-16; SCL R8.8 T13 definition; decision D14 shared idle gaze).
//   GlanceHolds   10 min of the reference scene: every G2 glance holds 0.4-2.5 s unless it is a G6 hold of 2.5-6 s; no G2
//                 target is concealed (two concealed pawns stand in the crowd) or out of line of sight. The head-rate
//                 clause (HeadLook <= LookMaxRateDegPerSec) is the SCL head filter's (story SCL-021, not built yet): it
//                 is not asserted here.
//   LongHoldRate  600 NPC-minutes: the G6 rate is within the 99 % Poisson interval of 0.10 per NPC-minute; G1 idle scans
//                 are SCL's shared layer (the crowd adds no NPC-only scan, so none is counted).
//   GoodsPitch    a long pause at a goods context pitches the look to -10 .. -20 degrees for 1-4 s (G3).
//   SalientGlance >= 400 salient stimuli (a runner within 15 m with line of sight): the glance share in the 99 %
//                 binomial interval of 0.5, and the reaction delay measured from the stimulus time lies in [0.3, 1.2] s
//                 and passes KS at p > 0.01 against U(0.3, 1.2).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdGazeTestData
{
	struct FGlanceRecorder final : public ICrowdAgentObserver
	{
		TSet<uint64> Concealed;
		int32 PasserBy = 0, LongHolds = 0, BadHolds = 0, ConcealedTargets = 0, Goods = 0, BadGoods = 0, Salient = 0;
		TArray<double> Delays;
		int32 Scheduled = 0;
		virtual void OnGlance(uint32 Ordinal, double Now, ECrowdGlanceKind Kind, uint64 TargetKey, double HoldSec) override
		{
			const FCrowdTuning_Server Tuning;
			switch (Kind)
			{
			case ECrowdGlanceKind::PasserBy:
				++PasserBy;
				BadHolds += HoldSec < Tuning.CrowdGlanceHoldMinSec || HoldSec > Tuning.CrowdGlanceHoldMaxSec ? 1 : 0;
				break;
			case ECrowdGlanceKind::LongHold:
				++LongHolds;
				BadHolds += HoldSec < Tuning.CrowdHeadHoldMinSec || HoldSec > Tuning.CrowdHeadHoldMaxSec ? 1 : 0;
				break;
			case ECrowdGlanceKind::Goods:
				++Goods;
				BadGoods += HoldSec < Tuning.CrowdGoodsHoldMinSec || HoldSec > Tuning.CrowdGoodsHoldMaxSec ? 1 : 0;
				break;
			case ECrowdGlanceKind::Salient:
				++Salient;
				break;
			default:
				break;
			}
			ConcealedTargets += Concealed.Contains(TargetKey) ? 1 : 0;
		}
		virtual void OnSalientGlanceScheduled(uint32 Ordinal, double StimulusTime, double StartTime) override
		{
			++Scheduled;
			Delays.Add(StartTime - StimulusTime);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdGazeGlanceHoldsTest, "StealthGame.Crowd.Gaze.GlanceHoldsAndLongHoldRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdGazeGlanceHoldsTest::RunTest(const FString& Parameters)
{
	using namespace CrowdGazeTestData;
	using namespace StealthCrowdSim;
	FConfig Config;
	Config.HalfSizeCm = 2500.0;
	FWorld World(Config);
	FGlanceRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	// Two concealed walkers in the crowd (a bush crossing): never a glance target.
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FScripted& Hidden = World.GetScripted(World.AddScripted(FVector(200.0 * Index, 300.0, 0.0), 0.0f));
		Hidden.Body.bPerceivable = false;
		Hidden.Body.bObstacle = false;
		Hidden.Body.MoveIntent = FVector2D(0.0, 0.8);
		Recorder.Concealed.Add(Hidden.Body.Key);
	}
	World.StartRound();
	const double Minutes = 12.0; // 50 NPCs x 12 min = 600 NPC-minutes
	World.RunUntil(Minutes * 60.0);
	const double NpcMinutes = Minutes * World.NumNpcs();
	const double Expected = NpcMinutes * 60.0 / Config.Tuning.CrowdHeadHoldIntervalSec;
	double Low = 0.0, High = 0.0;
	const bool bRate = StealthCrowdStats::InPoisson99(Recorder.LongHolds, Expected, &Low, &High);
	AddInfo(FString::Printf(TEXT("GAZE %d G2 glances, %d G6 holds over %.0f NPC-min (expected %.1f, 99 %% %.1f-%.1f), bad holds %d, concealed targets %d, HeadHold tells accepted %lld performed %lld"),
		Recorder.PasserBy, Recorder.LongHolds, NpcMinutes, Expected, Low, High, Recorder.BadHolds, Recorder.ConcealedTargets,
		World.GetTellSampler().GetCounters().Accepted[static_cast<int32>(ECrowdTell::HeadHold)], World.GetTellSampler().GetCounters().Performed[static_cast<int32>(ECrowdTell::HeadHold)]));
	TestTrue(TEXT("G2 glances happen"), Recorder.PasserBy > 500);
	TestEqual(TEXT("every G2 holds 0.4-2.5 s and every G6 2.5-6 s (AC-CNB-16a)"), Recorder.BadHolds, 0);
	TestEqual(TEXT("no glance targets a concealed pawn"), Recorder.ConcealedTargets, 0);
	TestTrue(TEXT("the G6 rate is in the 99 % Poisson interval of 0.10 per NPC-minute (AC-CNB-16b)"), bRate);
	TestTrue(TEXT("every G2 target was line-of-sight checked"), World.RaysAsked >= Recorder.PasserBy + Recorder.LongHolds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdGazeGoodsPitchTest, "StealthGame.Crowd.Gaze.GoodsPitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdGazeGoodsPitchTest::RunTest(const FString& Parameters)
{
	using namespace CrowdGazeTestData;
	using namespace StealthCrowdSim;
	FConfig Config;
	Config.NpcCount = 1;
	FWorld World(Config);
	World.AddContext({ FVector(150.0, 0.0, 0.0), true });
	FGlanceRecorder Recorder;
	World.SetObserver(&Recorder);
	FNpc& Npc = World.SpawnNpcAt(FVector::ZeroVector, 0.0f);
	FCrowdFirstActivity Long;
	Long.bPause = true;
	Long.PauseSec = 12.0;
	World.OverrideFirstActivity(Npc.Body.Ordinal, Long);
	World.StartRound();
	float MinPitch = 0.0f;
	for (int32 Frame = 0; Frame < 60 * 10; ++Frame)
	{
		World.Step();
		MinPitch = FMath::Min(MinPitch, static_cast<float>(FRotator::NormalizeAxis(Npc.Body.Control.Pitch)));
	}
	AddInfo(FString::Printf(TEXT("GOODS %d goods looks, lowest pitch %.1f deg"), Recorder.Goods, MinPitch));
	TestEqual(TEXT("one goods look"), Recorder.Goods, 1);
	TestEqual(TEXT("held 1-4 s"), Recorder.BadGoods, 0);
	TestTrue(TEXT("the look pitches to -10 .. -20 degrees (G3)"), MinPitch <= -10.0f + 0.1f && MinPitch >= -20.0f - 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdGazeSalientGlanceTest, "StealthGame.Crowd.Gaze.SalientGlance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdGazeSalientGlanceTest::RunTest(const FString& Parameters)
{
	using namespace CrowdGazeTestData;
	using namespace StealthCrowdSim;
	FConfig Config;
	FWorld World(Config);
	FGlanceRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	const int32 Runner = World.AddScripted(FVector::ZeroVector, 0.0f);
	World.StartRound();
	World.RunUntil(5.0);
	int32 Stimuli = 0;
	// A runner crosses the square every 3 s (each NPC's 2 s refractory then lets the next run count again).
	for (int32 Run = 0; Run < 100; ++Run)
	{
		FScripted& Body = World.GetScripted(Runner);
		Body.Body.Location = FVector(-2000.0 + 70.0 * (Run % 40), -1500.0 + 50.0 * (Run % 60), 0.0);
		Stimuli += World.EmitSalient(Body.Body);
		World.RunUntil(World.GetNow() + 3.0);
	}
	World.RunUntil(World.GetNow() + 3.0);
	double Low = 0.0, High = 0.0;
	const bool bShare = StealthCrowdStats::InBinomial99(Recorder.Scheduled, Stimuli, Config.Tuning.CrowdSalientGlancePct / 100.0, &Low, &High);
	double D = 0.0;
	const double P = StealthCrowdStats::KsPValue(Recorder.Delays, [&Config](double X) { return StealthCrowdStats::UniformCdf(X, Config.Tuning.CrowdReactDelayMinSec, Config.Tuning.CrowdReactDelayMaxSec); }, &D);
	double MinDelay = 1e9, MaxDelay = 0.0;
	for (const double Delay : Recorder.Delays)
	{
		MinDelay = FMath::Min(MinDelay, Delay);
		MaxDelay = FMath::Max(MaxDelay, Delay);
	}
	AddInfo(FString::Printf(TEXT("SALIENT %d stimuli with line of sight within 15 m, %d glances (%.3f; 99 %% %.3f-%.3f), delays %.3f-%.3f s (KS D %.4f p %.4f), %d performed"),
		Stimuli, Recorder.Scheduled, Stimuli > 0 ? static_cast<double>(Recorder.Scheduled) / Stimuli : 0.0, Low, High, MinDelay, MaxDelay, D, P, Recorder.Salient));
	TestTrue(TEXT(">= 400 eligible salient stimuli"), Stimuli >= 400);
	TestTrue(TEXT("the glance share lies in the 99 % binomial interval of 0.5 (G4)"), bShare);
	TestTrue(TEXT("delays from the stimulus time lie in [0.3, 1.2] s"), MinDelay >= Config.Tuning.CrowdReactDelayMinSec - 1e-9 && MaxDelay <= Config.Tuning.CrowdReactDelayMaxSec + 1e-9);
	TestTrue(TEXT("and pass KS against U(0.3, 1.2) at p > 0.01"), P > 0.01);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
