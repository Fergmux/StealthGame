// Story cnb-022 (production/epics/crowd-npc-behaviour/story-022-bush-herring.md): the bush red herring (T5) (GDD CNB-R7
// bush bullet, R16 T5, AC-CNB-15; ADR-0017; IVP F5). Headless NPC-only scenes with a bush every 15 m (the sim's
// Concealment stand-in: any number of pawns may share a bush, concealed pawns leave NPC perception and steering).
//   RateAndStays        (a) the configured rate 60 / CrowdBushIntervalSec >= r_N,req from IVP F5 (a deterministic config
//                       check); (b) over >= 120 simulated minutes the accepted per-NPC entry rate lies in the 99 % Poisson
//                       interval of the configured rate; every entry is a Hide(Enter) through the one call site after a
//                       dry run; stays show no upper truncation (empirical p99 within 15 % of the log-normal's); no NPC
//                       re-enters within BushReenterCooldownSec.
//   OccupancyBlind      the bushes chosen for a fixed seed are identical with the bushes empty and with three concealed
//                       pawns standing in them.
//   NearestWithinTenM   the bush entered is the nearest one with an open side within 10 m of the current leg; a bush
//                       12 m off the leg is never chosen.
// The advisory NPC-versus-player dwell comparison (Policy F7) is reported by playtest telemetry, not gated here.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "HAL/PlatformTime.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdBushHerringTestData
{
	using namespace StealthCrowdSim;
	using namespace StealthCrowdHerringTest;

	inline FConfig BushConfig(uint64 Seed)
	{
		FConfig Config;
		Config.HalfSizeCm = 3000.0;
		Config.SubSeed = Seed;
		Config.EnabledTellMask = 1u << static_cast<uint32>(ECrowdTell::Bush);
		return Config;
	}

	inline void AddBushGrid(FWorld& World, double HalfCm)
	{
		for (double X = -HalfCm * 0.8; X <= HalfCm * 0.8; X += 1500.0)
		{
			for (double Y = -HalfCm * 0.8; Y <= HalfCm * 0.8; Y += 1500.0)
			{
				World.AddBush(FVector(X + 250.0, Y + 250.0, 0.0));
			}
		}
	}

	/** IVP F5's required NPC rate: P_hide = n_P r_P / (n_P r_P + n_N r_N) <= k b with b = n_P / (n_P + n_N). */
	inline double RequiredRatePerMin(double RP, double NP, double NN, double K)
	{
		const double KB = K * NP / (NP + NN);
		return RP * NP * (1.0 - KB) / (KB * NN);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdBushRateTest, "StealthGame.Crowd.Bush.RateAndStays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdBushRateTest::RunTest(const FString& Parameters)
{
	using namespace CrowdBushHerringTestData;
	const double WallStart = FPlatformTime::Seconds();
	FConfig Config = BushConfig(0xB0500022ull);
	FWorld World(Config);
	AddBushGrid(World, Config.HalfSizeCm);
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	const double Minutes = 120.0;
	World.RunUntil(Minutes * 60.0);
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	// (a) the deterministic config check (r_P 0.3, n_P = TeamSize 5, n_N = NpcCount 50, k 1.5).
	const double Required = RequiredRatePerMin(0.3, 5.0, Tuning.NpcCount, 1.5);
	const double Configured = 60.0 / Tuning.CrowdBushIntervalSec;
	// (b) the measured rate.
	int32 Entries = 0;
	TArray<double> Stays;
	TMap<uint32, double> EnteredAt, LastExit;
	int32 Reentries = 0;
	for (const FBushEvent& Event : Recorder.Bush)
	{
		if (Event.bEnter)
		{
			++Entries;
			if (const double* Exit = LastExit.Find(Event.Ordinal); Exit != nullptr && Event.Time - *Exit < World.GetActionTimings().BushReenterCooldownSec)
			{
				++Reentries;
			}
			EnteredAt.Add(Event.Ordinal, Event.Time);
		}
		else if (const double* In = EnteredAt.Find(Event.Ordinal))
		{
			Stays.Add(Event.Time - *In);
			LastExit.Add(Event.Ordinal, Event.Time);
			EnteredAt.Remove(Event.Ordinal);
		}
	}
	double Low = 0, High = 0;
	const double NpcMinutes = Minutes * World.NumNpcs();
	const bool bRate = StealthCrowdStats::InPoisson99(Entries, NpcMinutes * Configured, &Low, &High);
	const double P99 = StealthCrowdStats::Quantile(Stays, 0.99);
	const double Theory99 = Tuning.CrowdBushDwellMedianSec * FMath::Exp(Tuning.CrowdBushDwellSigma * 2.3263478740);
	int32 HideEnters = 0, WithoutDryRun = 0;
	const TArray<FCommandRecord>& Log = World.GetCommandLog();
	for (int32 Index = 0; Index < Log.Num(); ++Index)
	{
		if (Log[Index].Kind != FCommandRecord::EKind::HideEnter)
		{
			continue;
		}
		++HideEnters;
		bool bDryRun = false;
		for (int32 Back = Index - 1; Back >= 0 && Log[Back].Time >= Log[Index].Time - 1e-9; --Back)
		{
			bDryRun |= Log[Back].Kind == FCommandRecord::EKind::DryRun && Log[Back].Ordinal == Log[Index].Ordinal && Log[Back].Arg == static_cast<int32>(EActionType::HideEnter);
		}
		WithoutDryRun += bDryRun ? 0 : 1;
	}
	AddInfo(FString::Printf(TEXT("BUSH %.0f NPC-min (%.1f s wall): configured %.4f /min >= required %.4f; entries %d (accepted %lld, dropped %lld; 99 %% %.1f-%.1f), Hide(Enter) %d, without a dry run %d, stays %d p99 %.1f s vs log-normal %.1f s, re-entries inside the cooldown %d"),
		NpcMinutes, FPlatformTime::Seconds() - WallStart, Configured, Required, Entries, World.GetTells().GetCounters().Accepted[static_cast<int32>(ECrowdTell::Bush)],
		World.GetTells().GetCounters().Dropped[static_cast<int32>(ECrowdTell::Bush)], Low, High, HideEnters, WithoutDryRun, Stays.Num(), P99, Theory99, Reentries));
	TestTrue(TEXT("(a) the configured accepted rate meets IVP F5's r_N,req (0.19 at r_P 0.3, 50 NPCs)"), Configured >= Required - 1e-9 && Required > 0.185 && Required < 0.195);
	TestTrue(TEXT("(b) the per-NPC entry rate is in the 99 % Poisson interval of the configured rate"), bRate);
	TestEqual(TEXT("every entry is a Hide(Enter) through the one call site"), HideEnters, Entries);
	TestEqual(TEXT("every Hide(Enter) follows a dry run"), WithoutDryRun, 0);
	TestTrue(TEXT("stays show no upper truncation (p99 within 15 % of the log-normal's)"), Stays.Num() > 500 && FMath::Abs(P99 - Theory99) <= 0.15 * Theory99);
	TestEqual(TEXT("no NPC re-enters within BushReenterCooldownSec"), Reentries, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdBushOccupancyBlindTest, "StealthGame.Crowd.Bush.OccupancyBlind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdBushOccupancyBlindTest::RunTest(const FString& Parameters)
{
	using namespace CrowdBushHerringTestData;
	auto Run = [](bool bOccupied, TArray<FBushEvent>& OutEnters)
	{
		FConfig Config = BushConfig(0xB0500B11ull);
		FWorld World(Config);
		AddBushGrid(World, Config.HalfSizeCm);
		FRecorder Recorder;
		World.SetObserver(&Recorder);
		// The NPCs first, so their ordinals and keys are the same whether or not the occupants exist.
		World.SpawnNpcs();
		if (bOccupied)
		{
			// Three concealed pawns (any kind) standing in three bushes: Concealment hides them from NPC perception and steering.
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FScripted& Hidden = World.GetScripted(World.AddScripted(World.GetBushes()[Index].Center, 0.0f));
				Hidden.Body.bPerceivable = false;
				Hidden.Body.bObstacle = false;
			}
		}
		World.StartRound();
		World.RunUntil(20.0 * 60.0);
		for (const FBushEvent& Event : Recorder.Bush)
		{
			if (Event.bEnter)
			{
				OutEnters.Add(Event);
			}
		}
	};
	TArray<FBushEvent> Empty, Occupied;
	Run(false, Empty);
	Run(true, Occupied);
	int32 Mismatch = FMath::Abs(Empty.Num() - Occupied.Num());
	for (int32 Index = 0; Index < FMath::Min(Empty.Num(), Occupied.Num()); ++Index)
	{
		Mismatch += (Empty[Index].Ordinal != Occupied[Index].Ordinal || Empty[Index].BushId != Occupied[Index].BushId || Empty[Index].Time != Occupied[Index].Time) ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("BUSHBLIND %d entries with the bushes empty, %d with three occupied, mismatches %d"), Empty.Num(), Occupied.Num(), Mismatch));
	TestTrue(TEXT("bush entries happen"), Empty.Num() > 50);
	TestEqual(TEXT("the bushes chosen are identical with the bushes empty and occupied"), Mismatch, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdBushNearestTest, "StealthGame.Crowd.Bush.NearestWithinTenM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdBushNearestTest::RunTest(const FString& Parameters)
{
	using namespace CrowdBushHerringTestData;
	FConfig Config = BushConfig(0xB0500C33ull);
	Config.NpcCount = 1;
	Config.Tuning.CrowdBushIntervalSec = 20.0f; // accelerated
	FWorld World(Config);
	// A straight back-and-forth leg along X: bush 1 at 5 m off the leg, bush 2 at 3 m off but farther away, bush 3 12 m off.
	World.AddBush(FVector(800.0, 500.0, 0.0));
	World.AddBush(FVector(2400.0, 300.0, 0.0));
	World.AddBush(FVector(500.0, 1200.0, 0.0));
	World.SetRouteFixture([](uint32 Ordinal, const FVector& From, FVector& OutGoal)
	{
		OutGoal = From.X < 1500.0 ? FVector(3000.0, 0.0, 0.0) : FVector(0.0, 0.0, 0.0);
		return true;
	});
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcAt(FVector::ZeroVector, 0.0f);
	World.StartRound();
	World.RunUntil(15.0 * 60.0);
	int32 Enters = 0, FarBush = 0;
	for (const FBushEvent& Event : Recorder.Bush)
	{
		if (Event.bEnter)
		{
			++Enters;
			FarBush += Event.BushId == World.GetBushes()[2].BushId ? 1 : 0;
		}
	}
	// Every choice was the nearest of the bushes within 10 m of the leg at the decision (the sim's FindBush is the rule;
	// here the outcome: never the one 12 m off the leg, and both near bushes are used from their own side of the leg).
	TSet<uint16> Used;
	for (const FBushEvent& Event : Recorder.Bush)
	{
		if (Event.bEnter)
		{
			Used.Add(Event.BushId);
		}
	}
	AddInfo(FString::Printf(TEXT("BUSHNEAREST %d entries, bushes used %d, the bush 12 m off the leg chosen %d times"), Enters, Used.Num(), FarBush));
	TestTrue(TEXT("bush entries happen on the leg"), Enters >= 5);
	TestEqual(TEXT("a bush more than 10 m off the leg is never chosen"), FarBush, 0);
	TestTrue(TEXT("the nearest near-leg bush is used (both near bushes, depending on where the pawn is)"), Used.Num() == 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
