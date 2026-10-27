// Spike story cnb-032 (production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md): the
// "CrowdCost" scenario of the T3 harness (ADR-0008; ADR-0016 section 10; GDD F5, AC-CNB-26). The listen host process
// (never in-process PIE: ADR-0022 section 6) starts the round once every bot client process has joined, waits for the
// crowd, then measures -StealthHarnessDuration= seconds with a scripted strike every -StealthHarnessStrikeEvery=
// seconds through the bot player path (the prototype strike announces OnActionStarted(Attack), so R8 runs). Every frame
// it reads the crowd cost meter (the six F5 scopes, Crowd.Director ... Crowd.Dispatch, summed game-thread time) and the
// frame's game-thread time; it writes a per-second summary and a final record with mean, p99 and worst frame per scope.
// Launch: tools/ci/mp/run-mp.ps1 -Scenario CrowdCost -Clients 10 -Duration 300 -MaxFps 60 -NpcCount 50.

#include "Scenarios/StealthHarnessRun.h"

#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Server/CrowdCostMeter.h"
#include "Server/CrowdDirector.h"
#include "StealthGameMode.h"

namespace StealthHarness
{
	class FCrowdCostScenario final : public IScenario
	{
	public:
		virtual void OnAllJoined(FRun& Run, UWorld* World) override
		{
			if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
			{
				GameMode->StartRound();
			}
			StartedAt = World->GetTimeSeconds();
			FString Cpu = FPlatformMisc::GetCPUBrand().TrimStartAndEnd();
			Run.Write(TEXT("host"), FString::Printf(TEXT("\"cpu\":%s,\"cores\":%d,\"logical\":%d,\"gpu\":%s,\"reference\":false,%s"),
				*FRun::Q(Cpu), FPlatformMisc::NumberOfCores(), FPlatformMisc::NumberOfCoresIncludingHyperthreads(), *FRun::Q(FPlatformMisc::GetPrimaryGPUBrand()),
				*FRun::ServerTimeFields(World)));
		}

		virtual bool TickServer(FRun& Run, UWorld* World, double ScenarioSeconds) override
		{
#if !UE_BUILD_SHIPPING
			FCrowdCostMeter& Meter = FCrowdCostMeter::Get();
			double Scopes[static_cast<int32>(ECrowdCostScope::Count)];
			const double CrowdMs = Meter.TakeFrame(Scopes);
			const UCrowdDirector* Director = UCrowdDirector::Get(World);
			const double Now = World->GetTimeSeconds();
			if (MeasureFrom < 0.0)
			{
				// Warm-up: the crowd is active and the Active-entry hitch has passed.
				if (Director != nullptr && Director->GetCore() != nullptr && Director->GetCore()->IsActive() && Director->NumControllers() >= 10 && Now - StartedAt > 15.0)
				{
					MeasureFrom = Now;
					NextStrike = Now + StrikeEvery();
					Meter.bEnabled = true;
					Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"measure\",\"controllers\":%d,%s"), Director->NumControllers(), *FRun::ServerTimeFields(World)));
				}
				return Now - StartedAt > 180.0; // a crowd that never starts ends the run (the record says so)
			}
			if (Frames > 0 || CrowdMs > 0.0)
			{
				FrameCrowd.Add(CrowdMs);
				FrameGame.Add(FApp::GetDeltaTime() * 1000.0);
				for (int32 Index = 0; Index < static_cast<int32>(ECrowdCostScope::Count); ++Index)
				{
					ScopeSum[Index] += Scopes[Index];
					ScopeMax[Index] = FMath::Max(ScopeMax[Index], Scopes[Index]);
				}
				++Frames;
			}
			if (Now >= NextStrike)
			{
				NextStrike += StrikeEvery();
				if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
				{
					// A strike burst through the bot player path (round-robin over the players).
					GameMode->DebugBotAction(StrikeIndex++ % FMath::Max(FRun::IntParam(TEXT("Clients"), 1) + 1, 1), TEXT("strike_npc"));
					++Strikes;
				}
			}
			if (Now - LastSummary >= 1.0)
			{
				LastSummary = Now;
				Run.Write(TEXT("second"), FString::Printf(TEXT("\"t\":%.1f,\"frames\":%d,\"crowdMsLast\":%.4f,\"controllers\":%d"), Now - MeasureFrom, Frames, CrowdMs,
					Director != nullptr ? Director->NumControllers() : 0));
			}
			if (Now - MeasureFrom >= FRun::IntParam(TEXT("Duration"), 300))
			{
				WriteResult(Run, World, Director);
				Meter.bEnabled = false;
				return true;
			}
#endif
			return false;
		}

	private:
		static double StrikeEvery() { return FMath::Max(1, FRun::IntParam(TEXT("StrikeEvery"), 20)); }

		static double Percentile(TArray<double> Values, double P)
		{
			if (Values.Num() == 0)
			{
				return 0.0;
			}
			Values.Sort();
			return Values[FMath::Clamp(FMath::CeilToInt32(P * Values.Num()) - 1, 0, Values.Num() - 1)];
		}

		static double Mean(const TArray<double>& Values)
		{
			double Sum = 0.0;
			for (const double Value : Values)
			{
				Sum += Value;
			}
			return Values.Num() > 0 ? Sum / Values.Num() : 0.0;
		}

		void WriteResult(FRun& Run, UWorld* World, const UCrowdDirector* Director)
		{
			static const TCHAR* Names[] = { TEXT("Director"), TEXT("StateTree"), TEXT("Follower"), TEXT("Perception"), TEXT("NavQuery"), TEXT("Dispatch") };
			FString Breakdown;
			for (int32 Index = 0; Index < static_cast<int32>(ECrowdCostScope::Count); ++Index)
			{
				Breakdown += FString::Printf(TEXT("%s\"%s\":{\"meanMs\":%.4f,\"maxMs\":%.4f}"), Index > 0 ? TEXT(",") : TEXT(""), Names[Index], Frames > 0 ? ScopeSum[Index] / Frames : 0.0, ScopeMax[Index]);
			}
			int32 Pawns = 0;
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				++Pawns;
			}
			Run.Write(TEXT("result"), FString::Printf(TEXT("\"frames\":%d,\"strikes\":%d,\"pawns\":%d,\"controllers\":%d,\"crowdMeanMs\":%.4f,\"crowdP99Ms\":%.4f,\"crowdMaxMs\":%.4f,\"frameMeanMs\":%.3f,\"frameP99Ms\":%.3f,\"budgetP99Ms\":1.5,\"budgetMeanMs\":0.8,\"scopes\":{%s},%s"),
				Frames, Strikes, Pawns, Director != nullptr ? Director->NumControllers() : 0, Mean(FrameCrowd), Percentile(FrameCrowd, 0.99), Percentile(FrameCrowd, 1.0),
				Mean(FrameGame), Percentile(FrameGame, 0.99), *Breakdown, *FRun::ServerTimeFields(World)));
		}

		double StartedAt = 0.0;
		double MeasureFrom = -1.0;
		double NextStrike = 0.0;
		double LastSummary = 0.0;
		int32 Frames = 0;
		int32 Strikes = 0;
		int32 StrikeIndex = 1;
		TArray<double> FrameCrowd;
		TArray<double> FrameGame;
		double ScopeSum[static_cast<int32>(ECrowdCostScope::Count)] = {};
		double ScopeMax[static_cast<int32>(ECrowdCostScope::Count)] = {};
	};

	static TUniquePtr<IScenario> MakeCrowdCostScenario()
	{
		return MakeUnique<FCrowdCostScenario>();
	}

	static FScenarioRegistrar GRegisterCrowdCost(TEXT("CrowdCost"), &MakeCrowdCostScenario);
}
