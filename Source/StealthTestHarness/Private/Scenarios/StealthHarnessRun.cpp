// T3 multi-process harness runner. See StealthHarnessRun.h.
#include "Scenarios/StealthHarnessRun.h"

#include "StealthTestHarnessModule.h"

#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "UObject/Package.h"

namespace StealthHarness
{
	static FRun* GRun = nullptr;

	static TMap<FString, FScenarioFactory>& ScenarioRegistry()
	{
		static TMap<FString, FScenarioFactory> Registry;
		return Registry;
	}

	FScenarioRegistrar::FScenarioRegistrar(const TCHAR* Name, FScenarioFactory Factory)
	{
		ScenarioRegistry().Add(FString(Name).ToLower(), Factory);
	}

	/** "Rtt" (story tdc-017): boot record, one net sample per second, exit after -StealthHarnessDuration= seconds (default 30). */
	class FRttScenario final : public IScenario
	{
	public:
		virtual bool TickServer(FRun& Run, UWorld* World, double ScenarioSeconds) override
		{
			return ScenarioSeconds >= FRun::IntParam(TEXT("Duration"), 30);
		}
	};

	static TUniquePtr<IScenario> MakeRttScenario()
	{
		return MakeUnique<FRttScenario>();
	}

	static FScenarioRegistrar GRegisterRtt(TEXT("Rtt"), &MakeRttScenario);

	static const TCHAR* ConditionName(int32 Condition)
	{
		static const TCHAR* Names[] = { TEXT("None"), TEXT("InitialOnly"), TEXT("OwnerOnly"), TEXT("SkipOwner"), TEXT("SimulatedOnly"),
			TEXT("AutonomousOnly"), TEXT("SimulatedOrPhysics"), TEXT("InitialOrOwner"), TEXT("Custom"), TEXT("ReplayOrOwner"), TEXT("ReplayOnly"),
			TEXT("SimulatedOnlyNoReplay"), TEXT("SimulatedOrPhysicsNoReplay"), TEXT("SkipReplay"), TEXT("Dynamic"), TEXT("Never"), TEXT("NetGroup") };
		return (Condition >= 0 && Condition < UE_ARRAY_COUNT(Names)) ? Names[Condition] : TEXT("?");
	}

	void FRun::StartIfRequested()
	{
		FString Scenario;
		if (GRun == nullptr && FParse::Value(FCommandLine::Get(), TEXT("-StealthHarnessScenario="), Scenario) && !Scenario.IsEmpty())
		{
			GRun = new FRun(Scenario);
		}
	}

	void FRun::Shutdown()
	{
		delete GRun;
		GRun = nullptr;
	}

	FRun* FRun::Get()
	{
		return GRun;
	}

	int32 FRun::IntParam(const TCHAR* Key, int32 Default)
	{
		int32 Value = Default;
		FParse::Value(FCommandLine::Get(), *FString::Printf(TEXT("-StealthHarness%s="), Key), Value);
		return Value;
	}

	FString FRun::StringParam(const TCHAR* Key, const FString& Default)
	{
		FString Value;
		return FParse::Value(FCommandLine::Get(), *FString::Printf(TEXT("-StealthHarness%s="), Key), Value) ? Value : Default;
	}

	FRun::FRun(const FString& InScenario)
		: ScenarioName(InScenario)
	{
		ProcessStart = FPlatformTime::Seconds();
		bServerProcess = StringParam(TEXT("Role"), TEXT("client")).Equals(TEXT("server"), ESearchCase::IgnoreCase);
		ClientIndex = IntParam(TEXT("Client"), 0);
		ExpectedClients = IntParam(TEXT("Clients"), 1);
		MaxSeconds = IntParam(TEXT("MaxSeconds"), 600);

		if (const FScenarioFactory* Factory = ScenarioRegistry().Find(ScenarioName.ToLower()))
		{
			Scenario = (*Factory)();
		}
		const bool bKnownScenario = Scenario.IsValid();
		if (!bKnownScenario)
		{
			Scenario = MakeUnique<FRttScenario>(); // unknown names fall back to "Rtt": boot, net samples, exit
		}

		const FString RunId = StringParam(TEXT("RunId"), FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		// -StealthHarnessOut=<path>: a ".json" value is this process's result file (records go beside it);
		// any other value is a folder shared by the processes of a run (records and server/client<N>-result.json
		// go there); absent: records go to Saved/Harness/<RunId> and no result file is written (story tdc-018).
		const FString OutParam = StringParam(TEXT("Out"), FString());
		const FString Dir = OutParam.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Harness"), RunId)
			: (OutParam.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase) ? FPaths::GetPath(OutParam) : OutParam);
		ResultPath = FStealthHarnessScenarioRunner::ResolveResultPath(OutParam, bServerProcess, ClientIndex);
		const FString FileName = bServerProcess ? FString(TEXT("server.jsonl")) : FString::Printf(TEXT("client%d.jsonl"), ClientIndex);
		IFileManager::Get().MakeDirectory(*Dir, true);
		File.Reset(IFileManager::Get().CreateFileWriter(*FPaths::Combine(Dir, FileName), FILEWRITE_AllowRead));

		// The replication-system assertion reads the game net driver of the running game world (story tdc-018).
		FString NetProfile;
		FParse::Value(FCommandLine::Get(), TEXT("PktEmulationProfile="), NetProfile);
		// Negative-control seam for the T3 runs: -StealthHarnessForceReplicationSystem=<legacy|iris> makes THIS process report that
		// system to the assertion instead of reading its driver (the engine switch -UseIrisReplication=1 only takes effect when the
		// net driver config allows Iris and the Iris plugin is on, which this project deliberately does not do: ADR-0001).
		const FString Forced = StringParam(TEXT("ForceReplicationSystem"), FString());
		if (!Forced.IsEmpty())
		{
			ReplicationProbe = MakeUnique<FStealthFixedReplicationProbe>(Forced.Equals(TEXT("iris"), ESearchCase::IgnoreCase) ? EHarnessReplicationSystem::Iris : EHarnessReplicationSystem::Legacy);
			UE_LOGF(LogStealthHarness, Warning, "Negative control: the replication-system probe is forced to report %ls", *Forced);
		}
		if (!ReplicationProbe)
		{
			ReplicationProbe = MakeUnique<FStealthNetDriverReplicationProbe>([]() -> const UNetDriver*
			{
				const UWorld* World = FindGameWorld();
				return World ? World->GetNetDriver() : nullptr;
			});
		}
		Runner = MakeUnique<FStealthHarnessScenarioRunner>(*ReplicationProbe, ScenarioName, LexToString(FApp::GetBuildConfiguration()), NetProfile);
		Runner->AddCapture(FPaths::Combine(Dir, FileName));

		Write(TEXT("start"), FString::Printf(TEXT("\"scenario\":%s,\"known\":%d,\"role\":%s,\"client\":%d,\"expectedClients\":%d,\"pid\":%u,\"cmdline\":%s"),
			*Q(ScenarioName), (int32)bKnownScenario, *Q(bServerProcess ? TEXT("server") : TEXT("client")), ClientIndex, ExpectedClients,
			FPlatformProcess::GetCurrentProcessId(), *Q(FCommandLine::Get())));

		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FRun::OnPostEngineInit);
		WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FRun::OnWorldInit);
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FRun::Tick));
		UE_LOGF(LogStealthHarness, Display, "Harness scenario %ls (%ls %d) writing to %ls", *ScenarioName, bServerProcess ? TEXT("server") : TEXT("client"), ClientIndex, *Dir);
	}

	FRun::~FRun()
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
		if (File.IsValid())
		{
			File->Flush();
			File->Close();
		}
	}

	void FRun::OnPostEngineInit()
	{
		// Every module is loaded now and no net driver exists yet.
		const int32 Trims = IntParam(TEXT("Trims"), -1);
		if (Trims >= 0)
		{
			if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("stealth.Spike.IdentityTrims")))
			{
				CVar->Set(Trims, ECVF_SetByCommandline);
			}
		}
#if DO_ENABLE_NET_TEST
		// Story tdc-017 load check: each named profile loads through LoadEmulationProfile (5.8.4 signature
		// `bool LoadEmulationProfile(const TCHAR* ProfileName)`); a missing name is the negative control.
		if (FParse::Param(FCommandLine::Get(), TEXT("StealthHarnessProfileCheck")))
		{
			for (const TCHAR* Profile : { TEXT("StealthLeak1"), TEXT("StealthLeak2"), TEXT("StealthClean"), TEXT("StealthNoSuchProfile") })
			{
				FPacketSimulationSettings Settings;
				Settings.PktLag = 999; // proves the load resets every field it does not set
				const bool bLoaded = Settings.LoadEmulationProfile(Profile);
				Write(TEXT("profileCheck"), FString::Printf(TEXT("\"profile\":%s,\"loaded\":%d,\"PktLag\":%d,\"PktLagVariance\":%d,\"PktLoss\":%d,\"PktIncomingLagMin\":%d,\"PktIncomingLoss\":%d,\"PktJitter\":%d"),
					*Q(Profile), (int32)bLoaded, Settings.PktLag, Settings.PktLagVariance, Settings.PktLoss, Settings.PktIncomingLagMin, Settings.PktIncomingLoss, Settings.PktJitter));
			}
		}
#endif
		const IConsoleVariable* TrimsVar = IConsoleManager::Get().FindConsoleVariable(TEXT("stealth.Spike.IdentityTrims"));
		Write(TEXT("config"), FString::Printf(TEXT("\"identityTrims\":%d"), TrimsVar ? TrimsVar->GetInt() : -1));
	}

	void FRun::OnWorldInit(UWorld* World, const UWorld::InitializationValues)
	{
		if (World == nullptr || !World->IsGameWorld())
		{
			return;
		}
		Write(TEXT("world"), FString::Printf(TEXT("\"map\":%s,\"netMode\":%d"), *Q(World->GetOutermost()->GetName()), (int32)World->GetNetMode()));
		World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([](AActor* Actor)
		{
			if (FRun* Run = FRun::Get(); Run && Actor && Actor->IsA<APawn>())
			{
				Run->DumpLayoutOnce(Actor->GetClass());
				Run->Write(TEXT("pawnSpawn"), FString::Printf(TEXT("\"name\":%s,\"cls\":%s,%s"),
					*Q(Actor->GetName()), *Q(Actor->GetClass()->GetName()), *ServerTimeFields(Actor->GetWorld())));
				// A replicated actor gets its NetGUID after the spawn returns: record it on the next tick.
				TWeakObjectPtr<AActor> Weak(Actor);
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
				{
					if (FRun* Later = FRun::Get(); Later && Weak.IsValid())
					{
						Later->Write(TEXT("pawnGuid"), FString::Printf(TEXT("\"name\":%s,\"guid\":%s"), *Q(Weak->GetName()), *Q(Guid(Weak->GetWorld(), Weak.Get()))));
					}
					return false;
				}));
			}
		}));
		World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateLambda([](AActor* Actor)
		{
			if (FRun* Run = FRun::Get(); Run && Actor && Actor->IsA<APawn>())
			{
				Run->Write(TEXT("pawnDestroy"), FString::Printf(TEXT("\"name\":%s,\"guid\":%s,%s"),
					*Q(Actor->GetName()), *Q(Guid(Actor->GetWorld(), Actor)), *ServerTimeFields(Actor->GetWorld())));
			}
		}));
	}

	UWorld* FRun::FindGameWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	void FRun::Write(const TCHAR* Type, const FString& Fields)
	{
		if (!File.IsValid())
		{
			return;
		}
		const FString Line = FString::Printf(TEXT("{\"t\":%.6f,\"type\":\"%s\"%s%s}\n"), FPlatformTime::Seconds(), Type, Fields.IsEmpty() ? TEXT("") : TEXT(","), *Fields);
		const FTCHARToUTF8 Utf8(*Line);
		File->Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Utf8.Length());
	}

	FString FRun::ServerTimeFields(const UWorld* World)
	{
		return World ? FString::Printf(TEXT("\"st\":%.6f,\"f\":%llu"), World->GetTimeSeconds(), (unsigned long long)GFrameCounter) : FString(TEXT("\"st\":-1,\"f\":0"));
	}

	FString FRun::Q(const FString& S)
	{
		FString Out = S.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
		Out.ReplaceInline(TEXT("\n"), TEXT(" "));
		Out.ReplaceInline(TEXT("\r"), TEXT(" "));
		return TEXT("\"") + Out + TEXT("\"");
	}

	FString FRun::Guid(const UWorld* World, const UObject* Object)
	{
		const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		if (!Object || !Driver || Driver->IsUsingIrisReplication() || !Driver->GuidCache.IsValid())
		{
			return TEXT("0");
		}
		return Driver->GuidCache->GetNetGUID(Object).ToString();
	}

	FString FRun::PlayerName(const APlayerController* PC)
	{
		if (!PC)
		{
			return TEXT("?");
		}
		if (PC->IsLocalController() && PC->GetNetMode() != NM_Client)
		{
			return TEXT("host");
		}
		return PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString(TEXT("?"));
	}

	APlayerController* FRun::FindPlayer(UWorld* World, const FString& Name)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get(); PC && PlayerName(PC) == Name)
			{
				return PC;
			}
		}
		return nullptr;
	}

	FString FRun::ConnectionName(const UNetConnection* Connection)
	{
		return Connection && Connection->PlayerController ? PlayerName(Connection->PlayerController) : FString(TEXT("?"));
	}

	void FRun::DumpLayoutOnce(const UClass* Class)
	{
		if (!Class || DumpedClasses.Contains(Class->GetFName()))
		{
			return;
		}
		DumpedClasses.Add(Class->GetFName());
		UClass* MutableClass = const_cast<UClass*>(Class);
		MutableClass->SetUpRuntimeReplicationData();
		TArray<FLifetimeProperty> Props;
		Class->GetDefaultObject()->GetLifetimeReplicatedProps(Props);
		TMap<int32, int32> ConditionByIndex;
		for (const FLifetimeProperty& Prop : Props)
		{
			ConditionByIndex.Add(Prop.RepIndex, (int32)Prop.Condition);
		}
		FString Entries;
		for (int32 Index = 0; Index < Class->ClassReps.Num(); ++Index)
		{
			const FRepRecord& Record = Class->ClassReps[Index];
			const int32* Condition = ConditionByIndex.Find(Index);
			Entries += FString::Printf(TEXT("%s{\"i\":%d,\"owner\":%s,\"prop\":%s,\"idx\":%d,\"cond\":%s}"), Index ? TEXT(",") : TEXT(""), Index,
				*Q(GetNameSafe(Record.Property ? Record.Property->GetOwnerStruct() : nullptr)), *Q(Record.Property ? Record.Property->GetName() : FString(TEXT("?"))),
				Record.Index, *Q(Condition ? ConditionName(*Condition) : TEXT("unregistered")));
		}
		Write(TEXT("layout"), FString::Printf(TEXT("\"cls\":%s,\"path\":%s,\"reps\":[%s]"), *Q(Class->GetName()), *Q(Class->GetPathName()), *Entries));
	}

	void FRun::WriteBoot(UWorld* World)
	{
		const UNetDriver* Driver = World->GetNetDriver();
		int32 PktLag = -1, PktLagVariance = -1, PktLoss = -1, PktIncomingLagMin = -1, PktIncomingLoss = -1;
#if DO_ENABLE_NET_TEST
		PktLag = Driver->PacketSimulationSettings.PktLag;
		PktLagVariance = Driver->PacketSimulationSettings.PktLagVariance;
		PktLoss = Driver->PacketSimulationSettings.PktLoss;
		PktIncomingLagMin = Driver->PacketSimulationSettings.PktIncomingLagMin;
		PktIncomingLoss = Driver->PacketSimulationSettings.PktIncomingLoss;
#endif
		FString Profile;
		FParse::Value(FCommandLine::Get(), TEXT("PktEmulationProfile="), Profile);
		const IConsoleVariable* MaxFpsVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
		const AGameModeBase* GameMode = World->GetAuthGameMode();
		const int32 MaxPlayers = GameMode && GameMode->GameSession ? GameMode->GameSession->MaxPlayers : -1;
		Write(TEXT("boot"), FString::Printf(
			TEXT("\"netMode\":%d,\"driver\":%s,\"iris\":%d,\"profile\":%s,\"PktLag\":%d,\"PktLagVariance\":%d,\"PktLoss\":%d,\"PktIncomingLagMin\":%d,\"PktIncomingLoss\":%d,")
			TEXT("\"netClientTicksPerSecond\":%.1f,\"tMaxFPS\":%.1f,\"maxPlayers\":%d,\"map\":%s"),
			(int32)World->GetNetMode(), *Q(Driver->GetClass()->GetName()), (int32)Driver->IsUsingIrisReplication(), *Q(Profile),
			PktLag, PktLagVariance, PktLoss, PktIncomingLagMin, PktIncomingLoss,
			GEngine ? GEngine->NetClientTicksPerSecond : -1.f, MaxFpsVar ? MaxFpsVar->GetFloat() : -1.f, MaxPlayers, *Q(World->GetOutermost()->GetName())));
		bBootWritten = true;
	}

	void FRun::WriteNetSample(UWorld* World)
	{
		const UNetDriver* Driver = World->GetNetDriver();
		const double Now = FPlatformTime::Seconds();
		const double Fps = LastNetSample > 0.0 ? FramesSinceSample / FMath::Max(Now - LastNetSample, 1e-6) : 0.0;
		FString Conns;
		if (const UNetConnection* Server = Driver->ServerConnection)
		{
			const APlayerController* PC = World->GetFirstPlayerController();
			const float ExactPing = PC && PC->PlayerState ? PC->PlayerState->ExactPing : -1.f;
			Conns = FString::Printf(TEXT("{\"conn\":\"server\",\"avgLagMs\":%.2f,\"exactPingMs\":%.2f,\"jitterMs\":%.2f,\"inLoss\":%.4f,\"outLoss\":%.4f}"),
				Server->AvgLag * 1000.f, ExactPing, Server->GetAverageJitterInMS(),
				Server->GetInLossPercentage().GetAvgLossPercentage(), Server->GetOutLossPercentage().GetAvgLossPercentage());
		}
		for (const UNetConnection* Connection : Driver->ClientConnections)
		{
			if (!Connection)
			{
				continue;
			}
			const APlayerController* PC = Connection->PlayerController;
			const float ExactPing = PC && PC->PlayerState ? PC->PlayerState->ExactPing : -1.f;
			Conns += FString::Printf(TEXT("%s{\"conn\":%s,\"avgLagMs\":%.2f,\"exactPingMs\":%.2f,\"jitterMs\":%.2f,\"inLoss\":%.4f,\"outLoss\":%.4f}"),
				Conns.IsEmpty() ? TEXT("") : TEXT(","), *Q(ConnectionName(Connection)), Connection->AvgLag * 1000.f, ExactPing, Connection->GetAverageJitterInMS(),
				Connection->GetInLossPercentage().GetAvgLossPercentage(), Connection->GetOutLossPercentage().GetAvgLossPercentage());
		}
		Write(TEXT("net"), FString::Printf(TEXT("\"fps\":%.1f,%s,\"conns\":[%s]"), Fps, *ServerTimeFields(World), *Conns));
		LastNetSample = Now;
		FramesSinceSample = 0;
	}

	void FRun::WritePawnTruth(UWorld* World)
	{
		// Server truth every 0.1 s from boot: which pawn (NetGUID) a player controls. Analysers join captures on it.
		FString Pawns;
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			const APawn* Pawn = *It;
			const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
			Pawns += FString::Printf(TEXT("%s{\"guid\":%s,\"name\":%s,\"kind\":%s,\"ctrl\":%s}"), Pawns.IsEmpty() ? TEXT("") : TEXT(","),
				*Q(Guid(World, Pawn)), *Q(Pawn->GetName()), *Q(PC ? PlayerName(PC) : FString(Pawn->GetController() ? TEXT("npc") : TEXT("none"))),
				*Q(GetNameSafe(Pawn->GetController())));
		}
		Write(TEXT("truth"), FString::Printf(TEXT("\"tag\":\"runner\",%s,\"pawns\":[%s]"), *ServerTimeFields(World), *Pawns));
	}

	void FRun::Finish(const TCHAR* Reason, bool bAbnormal)
	{
		if (bFinished)
		{
			return;
		}
		bFinished = true;
		if (Scenario.IsValid())
		{
			Scenario->OnFinish(*this);
		}
		if (bAbnormal)
		{
			Runner->MarkFailed(Reason);
		}
		Write(TEXT("end"), FString::Printf(TEXT("\"reason\":%s"), *Q(Reason)));
		// The host closes every client connection so the clients see the end at once (not a 60 s timeout).
		if (UWorld* World = bServerProcess ? FindGameWorld() : nullptr; World && World->GetNetDriver())
		{
			for (UNetConnection* Connection : TArray<UNetConnection*>(World->GetNetDriver()->ClientConnections))
			{
				if (Connection)
				{
					Connection->Close();
				}
			}
		}
		if (File.IsValid())
		{
			File->Flush();
		}
		UE_LOGF(LogStealthHarness, Display, "Harness scenario %ls finished (%ls); requesting exit.", *ScenarioName, Reason);
		// The result JSON: written once, here, with the verdict of the whole run (story tdc-018).
		const EStealthHarnessResultWrite Written = Runner->WriteResult(ResultPath);
		const bool bPassed = Runner->IsPassing() && Written != EStealthHarnessResultWrite::Failed;
		UE_LOGF(LogStealthHarness, Display, "Harness scenario %ls result: %ls", *ScenarioName, bPassed ? TEXT("passed") : TEXT("FAILED"));
		FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1, TEXT("StealthHarness"));
	}

	bool FRun::Tick(float DeltaSeconds)
	{
		++FramesSinceSample;
		const double Now = FPlatformTime::Seconds();
		if (bFinished)
		{
			return true;
		}
		if (Now - ProcessStart > MaxSeconds)
		{
			Finish(TEXT("max-seconds ceiling"), /*bAbnormal*/ true);
			return true;
		}
		if (File.IsValid() && Now - LastFlush > 1.0)
		{
			File->Flush();
			LastFlush = Now;
		}

		UWorld* World = FindGameWorld();
		UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		if (!World || !Driver)
		{
			if (!bServerProcess && bWasConnected)
			{
				Finish(TEXT("server connection lost"));
			}
			return true;
		}
		const ENetMode NetMode = World->GetNetMode();
		if (bServerProcess && NetMode != NM_ListenServer && NetMode != NM_DedicatedServer)
		{
			return true;
		}
		if (!bServerProcess)
		{
			if (NetMode != NM_Client || Driver->ServerConnection == nullptr || Driver->ServerConnection->GetConnectionState() != USOCK_Open)
			{
				if (bWasConnected)
				{
					Finish(TEXT("server connection lost"));
				}
				return true;
			}
			if (World->GetFirstPlayerController() == nullptr || World->GetGameState() == nullptr)
			{
				return true; // still joining
			}
			if (!bWasConnected)
			{
				bWasConnected = true;
				ClientConnectedAt = Now;
				// The login URL's ?Name= is replaced by the online identity's nickname: name the player C<n> explicitly.
				World->GetFirstPlayerController()->SetName(FString::Printf(TEXT("C%d"), ClientIndex));
				Write(TEXT("connected"), ServerTimeFields(World));
			}
		}
		if (!bBootWritten)
		{
			WriteBoot(World);
			// Story tdc-018: no leak or parity result is trusted under the wrong replication system.
			const bool bSystemOk = Runner->AssertReplicationSystem();
			Write(TEXT("replicationSystem"), FString::Printf(TEXT("\"system\":%s,\"expected\":%s,\"ok\":%d,\"negativeControlDetected\":%d"),
				*Q(StealthHarness::ToString(Runner->GetResult().ReplicationSystem)), *Q(StealthHarness::ToString(StealthHarness::PinnedReplicationSystem)),
				(int32)bSystemOk, (int32)Runner->GetResult().bNegativeControlDetected));
			if (!bSystemOk)
			{
				Finish(TEXT("replication system assertion failed"));
				return true;
			}
		}
		if (Now - LastNetSample >= 1.0)
		{
			WriteNetSample(World);
		}

		if (bServerProcess)
		{
			if (Now - LastTruth >= 0.1)
			{
				WritePawnTruth(World);
				LastTruth = Now;
			}
			if (AllJoinedAt < 0.0)
			{
				int32 Joined = 0;
				for (const UNetConnection* Connection : Driver->ClientConnections)
				{
					// Joined = logged in, begun play and renamed to C<n> by its harness (see the client branch below).
					Joined += (Connection && Connection->PlayerController && Connection->PlayerController->PlayerState && Connection->PlayerController->HasActorBegunPlay()
						&& Connection->PlayerController->PlayerState->GetPlayerName().StartsWith(TEXT("C"))) ? 1 : 0;
				}
				if (Joined < ExpectedClients)
				{
					return true;
				}
				AllJoinedAt = Now;
				Write(TEXT("allJoined"), FString::Printf(TEXT("\"clients\":%d,%s"), Joined, *ServerTimeFields(World)));
				Scenario->OnAllJoined(*this, World);
			}
			if (Scenario->TickServer(*this, World, Now - AllJoinedAt))
			{
				Finish(TEXT("scenario complete"));
			}
		}
		else
		{
			Scenario->TickClient(*this, World, Now - ClientConnectedAt);
		}
		return true;
	}

}
