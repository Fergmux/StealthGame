// Story cnb-021 (production/epics/crowd-npc-behaviour/story-021-brace-and-startle-herring.md): the brace and startle red
// herring (T4) (GDD CNB-R7 brace bullet, R16 T4, F2). Headless accelerated scenes with scripted runners crossing the
// crowd (human-like pawns: the crowd cannot tell them from NPCs).
//   DueWindowAndTriggers a brace happens only after the T4 clock made it due (never from a runner alone): every brace
//                        follows an accepted Brace tell of the same NPC, at a flinch trigger within the 30 s window or as
//                        an ambient startle at its end; triggered braces had a runner within 2 m or a landing within 3 m.
//   CapShareAndCooldown  >= 400 braces: the share held to the Prepare cap (DefenceDurationSec, read from its owner) in the
//                        99 % binomial interval of CrowdBraceCapSharePct, the rest released at U(1.0, 3.8) s; every
//                        PrepareBegin follows a Prepare dry run and is matched by a PrepareEnd; no NPC braces twice inside
//                        the 60 s cooldown.
// The accepted-rate gate at the default 3,000 s interval (0.020 per NPC-minute) needs 600 simulated minutes: it is part
// of StealthGame.Crowd.TellCalibration.TellBaselineNonZero (story cnb-023).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "HAL/PlatformTime.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdBraceTestData
{
	using namespace StealthCrowdSim;
	using namespace StealthCrowdHerringTest;

	/** Scripted runners bouncing across the plane at run speed (they hold the Run gait like a human holding sprint). */
	inline void AddRunners(FWorld& World, int32 Count, double HalfCm)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Y = -HalfCm * 0.8 + Index * (1.6 * HalfCm / FMath::Max(Count - 1, 1));
			FScripted& Runner = World.GetScripted(World.AddScripted(FVector(-HalfCm * 0.9, Y, 0.0), 0.0f));
			Runner.Body.MoveIntent = FVector2D(0.0, 1.0);
			Runner.Body.Gait = EGait::Run;
		}
	}

	/** Turns runners at the plane's edges (their camera yaw flips; the move intent stays forward). */
	inline void SteerRunners(FWorld& World, int32 Count, double HalfCm)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FBody& Body = World.GetScripted(Index).Body;
			if (Body.Location.X > HalfCm * 0.9 && FMath::Abs(Body.Control.Yaw) < 90.0)
			{
				Body.Control.Yaw = 180.0;
			}
			else if (Body.Location.X < -HalfCm * 0.9 && FMath::Abs(Body.Control.Yaw) > 90.0)
			{
				Body.Control.Yaw = 0.0;
			}
		}
	}

	struct FAcceptRecorder : public FRecorder
	{
		TMap<uint32, TArray<double>> Accepted;
		virtual void OnTellAccepted(uint32 Ordinal, double Now, ECrowdTell Tell) override
		{
			if (Tell == ECrowdTell::Brace)
			{
				Accepted.FindOrAdd(Ordinal).Add(Now);
			}
		}
	};

	inline FConfig BraceConfig(uint64 Seed, float IntervalSec)
	{
		FConfig Config;
		Config.HalfSizeCm = 2500.0;
		Config.SubSeed = Seed;
		Config.EnabledTellMask = 1u << static_cast<uint32>(ECrowdTell::Brace);
		Config.Tuning.CrowdBraceIntervalSec = IntervalSec; // accelerated (the window and trigger rules do not depend on it)
		Config.FirstNpcOrdinal = 16;
		return Config;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdBraceDueWindowTest, "StealthGame.Crowd.Brace.DueWindowAndTriggers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdBraceDueWindowTest::RunTest(const FString& Parameters)
{
	using namespace CrowdBraceTestData;
	const double WallStart = FPlatformTime::Seconds();
	FConfig Config = BraceConfig(0xB4ACE021ull, 200.0f);
	FWorld World(Config);
	const int32 Runners = 12;
	AddRunners(World, Runners, Config.HalfSizeCm);
	FAcceptRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	// Runner passes within 2 m of an NPC with no due brace: none of them may produce a brace.
	int32 UndueRunnerPasses = 0;
	const double Minutes = 20.0;
	while (World.GetNow() < Minutes * 60.0)
	{
		SteerRunners(World, Runners, Config.HalfSizeCm);
		World.Step();
		if (World.GetFrame() % 15 == 0)
		{
			for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
			{
				FNpc& Npc = World.GetNpc(Index);
				for (int32 R = 0; R < Runners; ++R)
				{
					UndueRunnerPasses += !Npc.Agent.IsBraceDue() && FVector::Dist2D(World.GetScripted(R).Body.Location, Npc.Body.Location) <= CrowdHerringConstants::FlinchRunnerRadiusCm ? 1 : 0;
				}
			}
		}
	}
	int32 Unexplained = 0, Triggered = 0, Ambient = 0, TriggeredLate = 0, AmbientEarly = 0;
	TMap<uint32, double> PreviousBrace;
	for (const FBrace& Brace : Recorder.Braces)
	{
		// The window opened at the latest acceptance before the brace that was not dropped in between (a due brace that
		// met the shared cooldown is dropped and the next acceptance opens a new window).
		const TArray<double>* Times = Recorder.Accepted.Find(Brace.Ordinal);
		const double* Prev = PreviousBrace.Find(Brace.Ordinal);
		double Due = -1.0;
		for (const double T : Times != nullptr ? *Times : TArray<double>())
		{
			if (T > Brace.Time + 1e-9 || (Prev != nullptr && T <= *Prev + 1e-9))
			{
				continue;
			}
			const bool bDropped = Recorder.Dropped.ContainsByPredicate([&](const FTellEvent& D)
				{ return D.Ordinal == Brace.Ordinal && D.Tell == ECrowdTell::Brace && D.Time >= T - 1e-9 && D.Time <= Brace.Time + 1e-9; });
			if (!bDropped)
			{
				Due = FMath::Max(Due, T);
			}
		}
		PreviousBrace.Add(Brace.Ordinal, Brace.Time);
		if (Due < 0.0)
		{
			++Unexplained;
			continue;
		}
		const double Delay = Brace.Time - Due;
		if (Brace.bTriggered)
		{
			++Triggered;
			TriggeredLate += Delay > CrowdHerringConstants::BraceDueWindowSec + 1e-6 ? 1 : 0;
		}
		else
		{
			++Ambient;
			AmbientEarly += Delay < CrowdHerringConstants::BraceDueWindowSec - 1e-6 ? 1 : 0;
		}
	}
	int32 AcceptedTotal = 0;
	for (const TPair<uint32, TArray<double>>& Pair : Recorder.Accepted)
	{
		AcceptedTotal += Pair.Value.Num();
	}
	AddInfo(FString::Printf(TEXT("BRACEDUE %.0f min x %d NPCs, %d runners (%.1f s wall): accepted %d, braces %d (triggered %d, ambient %d), without a due clock %d, triggered after the window %d, ambient before the window ended %d, runner passes of undue NPCs %d"),
		Minutes, World.NumNpcs(), Runners, FPlatformTime::Seconds() - WallStart, AcceptedTotal, Recorder.Braces.Num(), Triggered, Ambient, Unexplained, TriggeredLate, AmbientEarly, UndueRunnerPasses));
	TestTrue(TEXT("braces happen"), Recorder.Braces.Num() > 100);
	TestEqual(TEXT("a trigger alone never causes a brace (every brace follows an accepted T4 tell)"), Unexplained, 0);
	TestTrue(TEXT("runners passed NPCs that were not due (and caused no brace)"), UndueRunnerPasses > 100);
	TestTrue(TEXT("flinch triggers perform due braces"), Triggered > 0);
	TestEqual(TEXT("a triggered brace happens within the 30 s window"), TriggeredLate, 0);
	TestEqual(TEXT("an ambient startle happens only when the window has ended"), AmbientEarly, 0);
	TestTrue(TEXT("no more braces than accepted tells"), Recorder.Braces.Num() <= AcceptedTotal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdBraceCapShareTest, "StealthGame.Crowd.Brace.CapShareAndCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdBraceCapShareTest::RunTest(const FString& Parameters)
{
	using namespace CrowdBraceTestData;
	const double WallStart = FPlatformTime::Seconds();
	FConfig Config = BraceConfig(0xB4ACE022ull, 150.0f);
	Config.FirstNpcOrdinal = 0;
	FWorld World(Config);
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(40.0 * 60.0);
	const FCrowdActionTimings Timings = World.GetActionTimings();
	int32 Capped = 0, BadCap = 0, BadRelease = 0;
	for (const FBrace& Brace : Recorder.Braces)
	{
		Capped += Brace.bCapped ? 1 : 0;
		BadCap += Brace.bCapped && Brace.HoldSec != Timings.PrepareCapSec ? 1 : 0;
		BadRelease += !Brace.bCapped && (Brace.HoldSec < CrowdHerringConstants::BraceReleaseMinSec || Brace.HoldSec > CrowdHerringConstants::BraceReleaseMaxSec) ? 1 : 0;
	}
	int32 Begins = 0, Ends = 0, WithoutDryRun = 0, InsideCooldown = 0;
	TMap<uint32, double> LastBegin;
	const TArray<FCommandRecord>& Log = World.GetCommandLog();
	for (int32 Index = 0; Index < Log.Num(); ++Index)
	{
		const FCommandRecord& Record = Log[Index];
		if (Record.Kind == FCommandRecord::EKind::PrepareEnd)
		{
			++Ends;
		}
		if (Record.Kind != FCommandRecord::EKind::PrepareBegin)
		{
			continue;
		}
		++Begins;
		bool bDryRun = false;
		for (int32 Back = Index - 1; Back >= 0 && Log[Back].Time >= Record.Time - 1e-9; --Back)
		{
			bDryRun |= Log[Back].Kind == FCommandRecord::EKind::DryRun && Log[Back].Ordinal == Record.Ordinal && Log[Back].Arg == static_cast<int32>(EActionType::Prepare) && Log[Back].bAccepted;
		}
		WithoutDryRun += bDryRun ? 0 : 1;
		if (const double* Last = LastBegin.Find(Record.Ordinal); Last != nullptr && Record.Time - *Last < Timings.PrepareCooldownSec)
		{
			++InsideCooldown;
		}
		LastBegin.Add(Record.Ordinal, Record.Time);
	}
	double Low = 0, High = 0;
	const bool bShare = StealthCrowdStats::InBinomial99(Capped, Recorder.Braces.Num(), World.GetCrowdTuning().CrowdBraceCapSharePct / 100.0, &Low, &High);
	AddInfo(FString::Printf(TEXT("BRACECAP %d braces (%.1f s wall): held to the %.1f s cap %d (share %.4f, 99 %% %.4f-%.4f), bad cap holds %d, releases outside 1.0-3.8 s %d; PrepareBegin %d, PrepareEnd %d, without a dry run %d, inside the cooldown %d"),
		Recorder.Braces.Num(), FPlatformTime::Seconds() - WallStart, Timings.PrepareCapSec, Capped, Recorder.Braces.Num() > 0 ? static_cast<double>(Capped) / Recorder.Braces.Num() : 0.0, Low, High,
		BadCap, BadRelease, Begins, Ends, WithoutDryRun, InsideCooldown));
	TestTrue(TEXT("at least 400 braces"), Recorder.Braces.Num() >= 400);
	TestTrue(TEXT("the cap share is in the 99 % binomial interval of CrowdBraceCapSharePct"), bShare);
	TestEqual(TEXT("capped braces hold exactly the Prepare cap"), BadCap, 0);
	TestEqual(TEXT("the rest are released at U(1.0, 3.8) s"), BadRelease, 0);
	TestEqual(TEXT("every brace is a PrepareBegin through the sink"), Begins, Recorder.Braces.Num());
	TestTrue(TEXT("every PrepareBegin is ended (the last may still be held at the run's end)"), Ends >= Begins - World.NumNpcs() && Ends <= Begins);
	TestEqual(TEXT("every PrepareBegin follows a Prepare dry run"), WithoutDryRun, 0);
	TestEqual(TEXT("no NPC braces inside the 60 s cooldown"), InsideCooldown, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
