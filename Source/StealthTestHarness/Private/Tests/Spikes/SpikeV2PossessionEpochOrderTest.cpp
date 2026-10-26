// Story iif-009 (production/epics/input-interaction-framework/story-009-spike-v2-possession-epoch-order.md):
// ADR-0011 verification item V2. A THROWAWAY probe harness, not a gate test (named under StealthSpike., outside the
// per-commit ^StealthGame. filter); run on demand:
//   UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthSpike.Input.PIE.SpikeV2; Quit" -unattended -nullrhi
// Listen server + 1 client on the fixture map. AStealthPlayerController records every possession event in a
// development-only trace (server OnPossess epoch; owner ClientSetPossessionEpoch receipt; owner ClientRestart with the
// OwnerPossessionEpoch at that moment). Per path, the run checks on the owner that ClientSetPossessionEpoch(N) arrived
// before every ClientRestart of the possession and that OwnerPossessionEpoch == N at each ClientRestart.
// Paths (the Round R17.1 and ADR-0010 hand-over passes do not exist yet, so they are simulated by direct
// UnPossess/Possess calls, as the story allows; FScopedPossessionParity does not exist yet either):
//   Takeover        the player possesses a crowd pawn (its AI controller releases it and adopts the player's old body)
//   SamePawn        Unresponsive hand-over: a temporary AI controller holds the player's pawn for a few frames, then the
//                   same player re-possesses the same pawn (the epoch must still advance)
//   Retry           a takeover immediately followed by the server's ClientRetryClientRestart for the same pawn
//   NullPawn        a takeover of a pawn culled for the connection (IVP S9 relevancy hook): ClientRestart resolves to a
//                   null pawn on the owner, which asks ServerCheckClientPossessionReliable; the cull is then lifted
//   Rejoin          a new controller's first possession: the client's initial join (run -1) and rejoins through the
//                   ST-001 hook (fewer runs: each reconnect reloads the client map)
// Each path runs without emulation and under PktLag 50 ms per direction with 1 % loss (both drivers). Results go to
// Saved/Spikes/V2/v2-results.json and V2RESULT log lines; the report is in production/qa/spikes/.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "AIController.h"
#include "CQTest.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Spikes/StealthSpikeHooks.h"
#include "Session/StealthConnectionEventHook.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "View/Core/StealthPawn.h"

namespace SpikeV2
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	constexpr int32 RunsPerPath = 20;
	constexpr int32 RejoinRuns = 5;
	/** Frame ceiling of one run (a timeout is recorded, never a pass). */
	constexpr uint64 RunCeilingFrames = 900;
	constexpr uint64 SamePawnHoldFrames = 6;
	constexpr uint64 CullHoldFrames = 30;
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("1800");

	enum class EPath : uint8 { Takeover, SamePawn, Retry, NullPawn, Rejoin };
	const TCHAR* PathName(EPath Path)
	{
		switch (Path)
		{
		case EPath::Takeover: return TEXT("Takeover");
		case EPath::SamePawn: return TEXT("SamePawnAfterUnresponsive");
		case EPath::Retry: return TEXT("ClientRetryClientRestart");
		case EPath::NullPawn: return TEXT("NullPawnResend");
		default: return TEXT("Rejoin");
		}
	}

	struct FRunResult
	{
		FString Path;
		FString Profile;
		int32 Run = 0;
		uint16 ServerEpoch = 0;
		int32 EpochReceipts = 0;
		int32 Restarts = 0;
		int32 NullRestarts = 0;
		/** Frames: first epoch receipt, first restart, final (non-null) restart (0 = none). */
		uint64 EpochFrame = 0;
		uint64 FirstRestartFrame = 0;
		uint64 FinalRestartFrame = 0;
		/** OwnerPossessionEpoch at every restart of the run. */
		TArray<uint16> EpochAtRestarts;
		bool bOrdered = false;
		bool bTimedOut = false;
		/** Owner restarts for an earlier possession seen in this window, and whether each carried that possession's epoch. */
		int32 LateResends = 0;
		bool bLateResendsOk = true;
		/** A server OnPossess happened in the run (false: the path could not be exercised, e.g. no pawn after a rejoin). */
		bool bExercised = false;
		bool bPass = false;
	};
}

NETWORK_TEST_CLASS(SpikeV2PossessionEpochOrder, "StealthSpike.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;

	TUniquePtr<FStealthConnectionEventHook> Hook;
	FName ClientHandle;
	TArray<SpikeV2::FRunResult> Results;
	int32 TraceMark = 0;
	uint64 RunStartFrame = 0;
	uint16 ExpectedEpoch = 0;
	TWeakObjectPtr<APawn> ExpectedPawn;
	TWeakObjectPtr<AAIController> TempAi;
	TWeakObjectPtr<AStealthPawn> Culled;
	bool bRejoinExpected = false;
	/** Server epoch of each possession, by pawn GUID (to check late resends). */
	TMap<uint64, uint16> PossessionEpochByGuid;
	/** The listen host's own controller (its possessions are not the client's). */
	FString HostControllerPath;
	bool bEmulated = false;
	/** CQTest keeps the description pointer: the label strings must outlive the command queue. */
	TArray<FString> Labels;
	const TCHAR* L(FString Label) { return *Labels.Add_GetRef(MoveTemp(Label)); }

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(SpikeV2::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			if (!PreviousNetworkTimeout.IsEmpty())
			{
				Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
			}
		}
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(1)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(SpikeV2::FixtureMap);
	}

	AFTER_EACH()
	{
		StealthSpikeHooks::SetRelevancyOverride(StealthSpikeHooks::FRelevancyOverride());
		Hook.Reset();
	}

	static UWorld* ServerWorld()
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

	/** The server-side controller of the remote client. */
	static AStealthPlayerController* RemoteController(UWorld* Server)
	{
		for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
		{
			AStealthPlayerController* PC = Cast<AStealthPlayerController>(It->Get());
			if (PC && !PC->IsLocalController())
			{
				return PC;
			}
		}
		return nullptr;
	}

	/** An AI-driven crowd pawn other than Exclude. */
	static AStealthPawn* CrowdPawn(UWorld* Server, const APawn* Exclude)
	{
		for (TActorIterator<AStealthPawn> It(Server); It; ++It)
		{
			if (*It != Exclude && It->GetController() && It->GetController()->IsA<AAIController>())
			{
				return *It;
			}
		}
		return nullptr;
	}

	void ApplyEmulation(bool bOn)
	{
		FPacketSimulationSettings Settings;
		if (bOn)
		{
			Settings.PktLag = 50;
			Settings.PktLoss = 1;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetDriver())
			{
				Context.World()->GetNetDriver()->SetPacketSimulationSettings(Settings);
			}
		}
	}

	/** Server: starts one run of Path. */
	void StartRun(SpikeV2::EPath Path)
	{
		using namespace SpikeV2;
		UWorld* Server = ServerWorld();
		AStealthPlayerController* PC = Server ? RemoteController(Server) : nullptr;
		TraceMark = AStealthPlayerController::GetPossessionTrace().Num();
		RunStartFrame = GFrameCounter;
		ExpectedPawn = nullptr;
		if (PC == nullptr)
		{
			return;
		}
		APawn* Old = PC->GetPawn();
		switch (Path)
		{
		case EPath::Takeover:
		case EPath::Retry:
		case EPath::NullPawn:
		{
			AStealthPawn* Target = Path == EPath::NullPawn && Culled.IsValid() ? Culled.Get() : CrowdPawn(Server, Old);
			if (Target == nullptr)
			{
				return;
			}
			AController* Ai = Target->GetController();
			Ai->UnPossess();
			if (Path == EPath::NullPawn)
			{
				// The cull is already in place (PrepareNullPawn); possess while the owner cannot resolve the pawn.
			}
			PC->Possess(Target);
			if (Old)
			{
				Ai->Possess(Old);
			}
			if (Path == EPath::Retry)
			{
				PC->ClientRetryClientRestart(Target);
			}
			ExpectedPawn = Target;
			break;
		}
		case EPath::SamePawn:
		{
			// The second half (re-possession) runs in FinishSamePawn after SamePawnHoldFrames.
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AAIController* Ai = Server->SpawnActor<AAIController>(AAIController::StaticClass(), Params);
			PC->UnPossess();
			if (Ai && Old)
			{
				Ai->Possess(Old);
			}
			TempAi = Ai;
			ExpectedPawn = Old;
			break;
		}
		case EPath::Rejoin:
			break;
		}
		ExpectedEpoch = PC->GetServerPossessionEpoch();
	}

	/** Server, SamePawn: the same player re-possesses the same pawn. */
	void FinishSamePawn()
	{
		UWorld* Server = ServerWorld();
		AStealthPlayerController* PC = Server ? RemoteController(Server) : nullptr;
		APawn* Pawn = ExpectedPawn.Get();
		if (AAIController* Ai = TempAi.Get())
		{
			Ai->UnPossess();
			Ai->Destroy();
		}
		if (PC && Pawn)
		{
			PC->Possess(Pawn);
			ExpectedEpoch = PC->GetServerPossessionEpoch();
		}
	}

	/** True once the owner has run a ClientRestart for the expected pawn (any pawn for Rejoin), or the ceiling passed. */
	bool RunDone(SpikeV2::EPath Path) const
	{
		const TArray<FStealthPossessionTraceEvent>& Trace = AStealthPlayerController::GetPossessionTrace();
		for (int32 Index = TraceMark; Index < Trace.Num(); ++Index)
		{
			const FStealthPossessionTraceEvent& E = Trace[Index];
			// Client actor names differ from the server's, so any non-null owner restart after the mark ends the run.
			if (E.bClientWorld && E.Kind == FStealthPossessionTraceEvent::EKind::OwnerRestart && E.Pawn != TEXT("None"))
			{
				return true;
			}
		}
		return GFrameCounter > RunStartFrame + SpikeV2::RunCeilingFrames;
	}

	/** Evaluates the trace since the run's mark. */
	void Evaluate(SpikeV2::EPath Path, int32 Run)
	{
		using namespace SpikeV2;
		FRunResult R;
		R.Path = PathName(Path);
		R.Profile = bEmulated ? TEXT("PktLag50_Loss1") : TEXT("none");
		R.Run = Run;
		const TArray<FStealthPossessionTraceEvent>& Trace = AStealthPlayerController::GetPossessionTrace();
		// The run's possession: the remote controller's server OnPossess in this window (pawn GUID and epoch). Every owner
		// restart is matched to ITS possession by pawn GUID (identical on both sides): a late resend of an earlier
		// possession, arriving inside this window, is checked against that earlier possession's epoch, not this one's.
		uint16 ServerEpoch = 0;
		uint64 RunGuid = 0;
		for (int32 Index = TraceMark; Index < Trace.Num(); ++Index)
		{
			const FStealthPossessionTraceEvent& E = Trace[Index];
			if (!E.bClientWorld && E.Kind == FStealthPossessionTraceEvent::EKind::ServerOnPossess && E.Controller != HostControllerPath)
			{
				ServerEpoch = E.Epoch;
				RunGuid = E.PawnGuid;
				R.bExercised = true;
			}
		}
		bool bSawRunEpoch = false;
		bool bOrdered = true;
		bool bFinal = false;
		for (int32 Index = TraceMark; Index < Trace.Num(); ++Index)
		{
			const FStealthPossessionTraceEvent& E = Trace[Index];
			if (!E.bClientWorld)
			{
				continue;
			}
			if (E.Kind == FStealthPossessionTraceEvent::EKind::OwnerEpoch)
			{
				++R.EpochReceipts;
				R.EpochFrame = R.EpochFrame ? R.EpochFrame : E.Frame;
				bSawRunEpoch |= E.Epoch == ServerEpoch;
			}
			else if (E.Kind == FStealthPossessionTraceEvent::EKind::OwnerRestart)
			{
				if (RunGuid != 0 && E.PawnGuid != 0 && E.PawnGuid != RunGuid)
				{
					// A late (re)send for an earlier possession: its epoch must be that possession's.
					const uint16* Earlier = PossessionEpochByGuid.Find(E.PawnGuid);
					++R.LateResends;
					R.bLateResendsOk &= Earlier && *Earlier == E.Epoch;
					continue;
				}
				++R.Restarts;
				R.NullRestarts += E.Pawn == TEXT("None") ? 1 : 0;
				R.FirstRestartFrame = R.FirstRestartFrame ? R.FirstRestartFrame : E.Frame;
				R.EpochAtRestarts.Add(E.Epoch);
				bOrdered &= bSawRunEpoch;
				if (E.Pawn != TEXT("None"))
				{
					bFinal = true;
					R.FinalRestartFrame = E.Frame;
				}
			}
		}
		if (RunGuid != 0)
		{
			PossessionEpochByGuid.Add(RunGuid, ServerEpoch);
		}
		// For a remote client the server event is on the server's copy of the controller; take the latest server epoch.
		R.ServerEpoch = ServerEpoch;
		R.bOrdered = bOrdered && R.Restarts > 0;
		R.bTimedOut = !bFinal;
		bool bEpochMatches = R.EpochAtRestarts.Num() > 0;
		for (uint16 Epoch : R.EpochAtRestarts)
		{
			bEpochMatches &= Epoch == R.ServerEpoch;
		}
		R.bPass = R.bOrdered && bEpochMatches && bFinal && R.bLateResendsOk;
		TestRunner->AddInfo(FString::Printf(TEXT("V2RESULT %s | %s | run %d | server epoch %u | receipts %d | restarts %d (null %d) | epoch@restart [%s] | late resends %d (ok %d) | ordered %d | %s"),
			*R.Path, *R.Profile, R.Run, R.ServerEpoch, R.EpochReceipts, R.Restarts, R.NullRestarts,
			*FString::JoinBy(R.EpochAtRestarts, TEXT(","), [](uint16 E) { return FString::FromInt(E); }), R.LateResends, R.bLateResendsOk ? 1 : 0, R.bOrdered ? 1 : 0,
			R.bPass ? TEXT("PASS") : (!R.bExercised ? TEXT("NOT EXERCISED") : (R.bTimedOut ? TEXT("FAIL(timeout)") : TEXT("FAIL")))));
		Results.Add(MoveTemp(R));
	}

	void QueuePath(SpikeV2::EPath Path, int32 Runs)
	{
		using namespace SpikeV2;
		for (int32 Run = 0; Run < Runs; ++Run)
		{
			const FString Label = FString::Printf(TEXT("%s run %d"), PathName(Path), Run);
			if (Path == EPath::NullPawn)
			{
				Network.Then(L(Label + TEXT(": cull a crowd pawn for the connection")), [this]()
				{
					UWorld* Server = ServerWorld();
					AStealthPlayerController* PC = Server ? RemoteController(Server) : nullptr;
					TWeakObjectPtr<AStealthPawn> Target = Server ? CrowdPawn(Server, PC ? PC->GetPawn() : nullptr) : nullptr;
					Culled = Target;
					StealthSpikeHooks::SetRelevancyOverride([Target](const AActor* Subject, const AActor*, const AActor*) -> TOptional<bool>
					{
						return Subject == Target.Get() ? TOptional<bool>(false) : TOptional<bool>();
					});
					RunStartFrame = GFrameCounter;
				})
				.Until(L(Label + TEXT(": the cull closes the channel")), [this]()
				{
					// The server keeps a no-longer-relevant actor's channel for RelevantTimeout (5 s), so wait for the close.
					UWorld* Server = ServerWorld();
					AStealthPlayerController* PC = Server ? RemoteController(Server) : nullptr;
					UNetConnection* Connection = PC ? PC->GetNetConnection() : nullptr;
					AStealthPawn* Target = Culled.Get();
					return (Connection && Target && Connection->FindActorChannelRef(TWeakObjectPtr<AActor>(Target)) == nullptr) || GFrameCounter > RunStartFrame + SpikeV2::RunCeilingFrames;
				});
			}
			if (Path == EPath::Rejoin)
			{
				Network.Then(L(Label + TEXT(": rejoin")), [this]()
				{
					TraceMark = AStealthPlayerController::GetPossessionTrace().Num();
					RunStartFrame = GFrameCounter;
					FString Error;
					const bool bStarted = FStealthConnectionEventHook::Rejoin(ClientHandle, FStealthConnectionEventHook::IdentityKeyForSlot(1), TEXT("V2"), FString(), &Error);
					TestRunner->AddInfo(FString::Printf(TEXT("V2 rejoin started %d (%s)"), bStarted ? 1 : 0, *Error));
					// No expected-error declaration: in this PIE the rejoin reconnects without a BroadcastNetworkFailure.
				});
			}
			else
			{
				Network.Then(L(Label + TEXT(": possession change")), [this, Path]() { StartRun(Path); });
			}
			if (Path == EPath::SamePawn)
			{
				Network.Until(L(Label + TEXT(": hold under AI")), [this]() { return GFrameCounter > RunStartFrame + SamePawnHoldFrames; })
					.Then(L(Label + TEXT(": re-possess the same pawn")), [this]() { FinishSamePawn(); });
			}
			if (Path == EPath::NullPawn)
			{
				Network.Until(L(Label + TEXT(": null restart window")), [this]() { return GFrameCounter > RunStartFrame + CullHoldFrames; })
					.Then(L(Label + TEXT(": lift the cull")), [this]() { StealthSpikeHooks::SetRelevancyOverride(StealthSpikeHooks::FRelevancyOverride()); });
			}
			Network.Until(L(Label + TEXT(": owner restart")), [this, Path]() { return RunDone(Path); })
				.Then(L(Label + TEXT(": evaluate")), [this, Path, Run]()
				{
					Evaluate(Path, Run);
					if (Path == EPath::Rejoin && bEmulated)
					{
						ApplyEmulation(true); // the rejoined client has a new net driver
					}
				});
		}
	}

	void QueueProfile(bool bOn)
	{
		using namespace SpikeV2;
		Network.Then(bOn ? TEXT("emulation on") : TEXT("emulation off"), [this, bOn]() { bEmulated = bOn; ApplyEmulation(bOn); });
		QueuePath(EPath::Takeover, RunsPerPath);
		QueuePath(EPath::SamePawn, RunsPerPath);
		QueuePath(EPath::Retry, RunsPerPath);
		QueuePath(EPath::NullPawn, RunsPerPath);
		QueuePath(EPath::Rejoin, RejoinRuns);
	}

	TEST_METHOD(EpochPrecedesClientRestartOnEveryPath)
	{
		using namespace SpikeV2;
		Network
			.UntilClients(TEXT("the client possesses its pawn"), [](FBasePIENetworkComponentState& Client)
			{
				const APlayerController* PC = Client.World ? Client.World->GetFirstPlayerController() : nullptr;
				return PC && PC->GetPawn() != nullptr;
			})
			.UntilServer(TEXT("the crowd has spawned (after the nav rebuild)"), [](FBasePIENetworkComponentState& Server)
			{
				int32 Ai = 0;
				for (TActorIterator<AStealthPawn> It(Server.World); It; ++It)
				{
					Ai += It->GetController() && It->GetController()->IsA<AAIController>() ? 1 : 0;
				}
				return Ai >= 4;
			})
			.ThenServer(TEXT("hook and client handle"), [this](FBasePIENetworkComponentState& Server)
			{
				Hook = MakeUnique<FStealthConnectionEventHook>(Server.World);
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Client)
					{
						ClientHandle = Context.ContextHandle;
					}
				}
				for (FConstPlayerControllerIterator It = Server.World->GetPlayerControllerIterator(); It; ++It)
				{
					if (It->Get() && It->Get()->IsLocalController())
					{
						HostControllerPath = It->Get()->GetPathName();
					}
				}
				// The client's initial join is a first possession of a fresh controller (the rejoin shape): evaluate it.
				TraceMark = 0;
				Evaluate(SpikeV2::EPath::Rejoin, -1);
				AStealthPlayerController::GetPossessionTrace().Reset();
			});
		QueueProfile(false);
		QueueProfile(true);
		Network.Then(TEXT("report"), [this]()
		{
			ApplyEmulation(false);
			TMap<FString, TPair<int32, int32>> PerPath; // passes, runs
			FString Json = FString::Printf(TEXT("{\n  \"engine\": \"%s\",\n  \"runs\": [\n"), *FEngineVersion::Current().ToString());
			for (int32 Index = 0; Index < Results.Num(); ++Index)
			{
				const SpikeV2::FRunResult& R = Results[Index];
				TPair<int32, int32>& Tally = PerPath.FindOrAdd(R.Path + TEXT(" / ") + R.Profile);
				Tally.Key += R.bPass ? 1 : 0;
				++Tally.Value;
				Json += FString::Printf(TEXT("    {\"path\": \"%s\", \"profile\": \"%s\", \"run\": %d, \"serverEpoch\": %u, \"epochReceipts\": %d, \"restarts\": %d, \"nullRestarts\": %d, \"epochFrame\": %llu, \"firstRestartFrame\": %llu, \"finalRestartFrame\": %llu, \"epochAtRestarts\": [%s], \"ordered\": %s, \"timedOut\": %s, \"exercised\": %s, \"pass\": %s}%s\n"),
					*R.Path, *R.Profile, R.Run, R.ServerEpoch, R.EpochReceipts, R.Restarts, R.NullRestarts, R.EpochFrame, R.FirstRestartFrame, R.FinalRestartFrame,
					*FString::JoinBy(R.EpochAtRestarts, TEXT(","), [](uint16 E) { return FString::FromInt(E); }), R.bOrdered ? TEXT("true") : TEXT("false"),
					R.bTimedOut ? TEXT("true") : TEXT("false"), R.bExercised ? TEXT("true") : TEXT("false"), R.bPass ? TEXT("true") : TEXT("false"), Index + 1 < Results.Num() ? TEXT(",") : TEXT(""));
			}
			Json += TEXT("  ],\n  \"summary\": {\n");
			int32 Written = 0;
			for (const TPair<FString, TPair<int32, int32>>& Pair : PerPath)
			{
				Json += FString::Printf(TEXT("    \"%s\": \"%d/%d\"%s\n"), *Pair.Key, Pair.Value.Key, Pair.Value.Value, ++Written < PerPath.Num() ? TEXT(",") : TEXT(""));
				TestRunner->AddInfo(FString::Printf(TEXT("V2SUMMARY %s %d/%d"), *Pair.Key, Pair.Value.Key, Pair.Value.Value));
			}
			Json += TEXT("  }\n}\n");
			FFileHelper::SaveStringToFile(Json, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Spikes/V2/v2-results.json")));
			ASSERT_THAT(IsTrue(Results.Num() > 0, TEXT("the harness recorded runs")));
		});
	}
};

#endif
