#pragma once

// T3 multi-process harness runner (ADR-0008 section 1, T3): one listen host and N client processes
// (`UnrealEditor.exe <uproject> -game`) each load this module; when the command line names a scenario
// (`-StealthHarnessScenario=<Name>`), the runner drives it and writes one JSON-lines record file per
// process. Launcher: tools/ci/mp/run-mp.ps1. Analysers: tools/ci/mp/*.py.
//
// First used by the IVP spikes S1 (story-001) and S9 (story-004) and the TDC net-profile RTT
// measurement (story-017). Gauntlet wiring is story tdc-020; until then the PowerShell launcher runs it.
//
// Time: every record carries `t`, FPlatformTime::Seconds() (QueryPerformanceCounter based, so it is
// comparable across processes on one machine). Server records also carry server world time `st` and
// GFrameCounter `f`. Wall-clock time is never a pass/fail predicate (ADR-0008 section 4); the analysers
// report measurements, the reports judge.

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "StealthHarnessScenarioRunner.h"

class AActor;
class APawn;
class APlayerController;
class FArchive;
class UNetConnection;

namespace StealthHarness
{
	class FRun;

	/** One named scenario. Every hook runs on the game thread. */
	class IScenario
	{
	public:
		virtual ~IScenario() = default;

		/** Called once on the server when the expected number of clients has logged in. */
		virtual void OnAllJoined(FRun& Run, UWorld* World) {}

		/** Server (listen host) frame tick after OnAllJoined. Returns true when the scenario is done. */
		virtual bool TickServer(FRun& Run, UWorld* World, double ScenarioSeconds) { return false; }

		/** Client frame tick while connected; ClientSeconds counts from the first connected frame. */
		virtual void TickClient(FRun& Run, UWorld* World, double ClientSeconds) {}

		/**
		 * Client: true while the scenario expects the server connection to go away (a rejoin, a hard travel); the run
		 * then keeps going instead of ending with "server connection lost".
		 */
		virtual bool ClientToleratesDisconnect(FRun& Run) const { return false; }

		/** Client tick while not connected (only after a first connection, and only while ClientToleratesDisconnect). World may be null. */
		virtual void TickClientOffline(FRun& Run, UWorld* World, double ClientSeconds) {}

		/** Client: a (new) server connection is ready: the first join, a rejoin, or the reconnect of a hard travel. */
		virtual void OnClientConnected(FRun& Run, UWorld* World) {}

		/** Called once when the run finishes for any reason, before the exit request: release world hooks here. */
		virtual void OnFinish(FRun& Run) {}
	};

	/** Process-wide runner; exists only when the command line names a scenario. */
	class FRun
	{
	public:
		/** Creates the runner if `-StealthHarnessScenario=` is on the command line. Module startup only. */
		static void StartIfRequested();

		/** Flushes and destroys the runner. Module shutdown only. */
		static void Shutdown();

		/** The runner, or null when no scenario was requested. */
		static FRun* Get();

		/** Writes one record: {"t":..,"type":Type,<Fields>}. Fields is a JSON fragment without braces (may be empty). */
		void Write(const TCHAR* Type, const FString& Fields);

		/** Server world fields `"st":..,"f":..` for World (empty fragment when World is null). */
		static FString ServerTimeFields(const UWorld* World);

		/** JSON string literal for S (quotes and backslashes escaped). */
		static FString Q(const FString& S);

		/** NetGUID of Object in World's game net driver as a string ("0" when not mapped). */
		static FString Guid(const UWorld* World, const UObject* Object);

		/** Name the client used to log in (`C<n>`, from `?Name=` in the launcher URL); "host" for the listen host's own player. */
		static FString PlayerName(const APlayerController* PC);

		/** Server: the player controller whose PlayerState name equals Name, or null. */
		static APlayerController* FindPlayer(UWorld* World, const FString& Name);

		/** Server: the client name (`C<n>`) that owns Connection, or "?". */
		static FString ConnectionName(const UNetConnection* Connection);

		/** Command-line integer `-StealthHarness<Key>=`, or Default. */
		static int32 IntParam(const TCHAR* Key, int32 Default);

		/** Command-line string `-StealthHarness<Key>=`, or Default. */
		static FString StringParam(const TCHAR* Key, const FString& Default);

		/** Records the class replication layout (ClassReps index -> property, condition) once per class. */
		void DumpLayoutOnce(const UClass* Class);

		/** The process's result evaluator (story tdc-018): replication-system assertion, faults[], captures[]. Null before construction finishes. */
		FStealthHarnessScenarioRunner& GetRunner() { return *Runner; }

		const FString& GetScenarioName() const { return ScenarioName; }
		bool IsServerProcess() const { return bServerProcess; }
		int32 GetClientIndex() const { return ClientIndex; }

	private:
		FRun(const FString& InScenario);
		~FRun();

		bool Tick(float DeltaSeconds);
		void OnPostEngineInit();
		void OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
		void WriteBoot(UWorld* World);
		void WriteNetSample(UWorld* World);
		void WritePawnTruth(UWorld* World);
		/** Ends the run: bAbnormal marks it failed (a ceiling hit), then the result JSON is written and the process exits with 0 (passed) or 1. */
		void Finish(const TCHAR* Reason, bool bAbnormal = false);
		static UWorld* FindGameWorld();

		FString ScenarioName;
		TUniquePtr<IScenario> Scenario;
		TUniquePtr<FArchive> File;
		FTSTicker::FDelegateHandle TickHandle;
		FDelegateHandle PostEngineInitHandle;
		FDelegateHandle WorldInitHandle;
		TSet<FName> DumpedClasses;
		TUniquePtr<IStealthReplicationSystemProbe> ReplicationProbe;
		TUniquePtr<FStealthHarnessScenarioRunner> Runner;
		FString ResultPath;

		bool bServerProcess = false;
		int32 ClientIndex = 0;
		int32 ExpectedClients = 1;
		double ProcessStart = 0.0;
		double MaxSeconds = 600.0;
		double LastFlush = 0.0;
		double LastNetSample = 0.0;
		double LastTruth = 0.0;
		double AllJoinedAt = -1.0;
		double ClientConnectedAt = -1.0;
		/** The server connection the client last saw ready (a new one means a rejoin or a hard-travel reconnect). */
		TWeakObjectPtr<UNetConnection> LastServerConnection;
		int32 ClientConnections = 0;
		/** Since when a tolerated disconnect has lasted (-1 while connected). */
		double OfflineSince = -1.0;
		bool bBootWritten = false;
		bool bFinished = false;
		bool bWasConnected = false;
		uint64 FramesSinceSample = 0;
	};

	/** Creates one scenario instance. */
	using FScenarioFactory = TUniquePtr<IScenario> (*)();

	/**
	 * Registers a scenario under a name (case-insensitive) at static-initialisation time, so each scenario
	 * lives in its own translation unit and the runner never names one. Usage, at namespace scope:
	 *   static StealthHarness::FScenarioRegistrar GRegisterS1(TEXT("S1"), &MakeS1Scenario);
	 */
	struct FScenarioRegistrar
	{
		FScenarioRegistrar(const TCHAR* Name, FScenarioFactory Factory);
	};
}
