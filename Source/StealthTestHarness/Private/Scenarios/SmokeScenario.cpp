// Story tdc-020 (production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md):
// the multi-process "Smoke" scenario of the T3 harness (ADR-0008 sections 1 and 5). It does nothing
// but prove the topology: a listen host and its client(s) boot into the fixture map, every process
// asserts and records its replication system (FStealthHarnessScenarioRunner, story tdc-018), and the
// host ends the run once every expected client has joined and a fixed number of FRAMES has passed
// (bounded by frames, never by wall-clock time: ADR-0008 section 4). Launchers: the Gauntlet node
// Build/Scripts/StealthMultiProcessSmoke.cs, or tools/ci/mp/run-mp.ps1 -Scenario Smoke.

#include "Scenarios/StealthHarnessRun.h"

namespace StealthHarness
{
	/** "Smoke": ends `-StealthHarnessSettleFrames=` (default 60) server frames after all clients joined. */
	class FSmokeScenario final : public IScenario
	{
	public:
		virtual void OnAllJoined(FRun& Run, UWorld* World) override
		{
			JoinedFrame = GFrameCounter;
			Run.Write(TEXT("smoke"), FString::Printf(TEXT("\"phase\":\"allJoined\",\"expectedClients\":%d"), FRun::IntParam(TEXT("Clients"), 1)));
		}

		virtual bool TickServer(FRun& Run, UWorld* World, double ScenarioSeconds) override
		{
			return GFrameCounter - JoinedFrame >= static_cast<uint64>(FMath::Max(1, FRun::IntParam(TEXT("SettleFrames"), 60)));
		}

	private:
		uint64 JoinedFrame = 0;
	};

	static TUniquePtr<IScenario> MakeSmokeScenario()
	{
		return MakeUnique<FSmokeScenario>();
	}

	static FScenarioRegistrar GRegisterSmoke(TEXT("Smoke"), &MakeSmokeScenario);
}
