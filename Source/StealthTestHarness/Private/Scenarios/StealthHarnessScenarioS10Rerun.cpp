// Scenario "S10R": the IVP S10 re-run (story production/epics/information-visibility-policy/
// story-056-s10-fix-implementation-and-rerun.md) on the S9/S10 cull rig, with the fix of ADR-0003 amendment
// "S10 resolution 2026-10-25" implemented (S10-R2..R6):
//   - S10-R2 per-connection re-open guard: every rig close whose channel is Closing right after Close() adds a
//     pending-close entry for (subject, connection); the relevancy hook returns false while a non-stale entry exists;
//     the entry clears at the in-sequence close acknowledgement, read every server frame in the cull tick
//     (TG_PostUpdateWork) from the closed channel's slot (Channels[ChIndex] != ClosedChannel, or same object no
//     longer Closing). No forced update at release.
//   - S10-R3 bounded hold: ReopenAckTimeoutMs (default 350, harness setting) marks the entry stale (no longer holds;
//     still watched until its acknowledgement), flags the pair unverified; the flag clears 2 x ReopenAckTimeoutMs
//     after that acknowledgement or at the pawn's next close to that connection.
//   - S10-R4 lost-pawn repair: the client counts channel opens per live pawn instance (AStealthCharacter::
//     OnActorChannelOpen spike hook); a network destroy with >= 2 opens queues a probe; the next client frame reports
//     NMT_ActorChannelFailure(LastOpenChIndex) only if that channel is still an open, not-Closing actor channel for the
//     same NetGUID and the server connection is open. Server: AStealthPlayerController::NotifyActorChannelFailure
//     (spike hook) only queues; the cull tick re-checks the slot, relevance and the unverified flag, then closes the
//     channel and adds a pending-close entry (fresh channel); valid reports over ChannelRepairMaxPerSec per connection
//     are deferred (FIFO, one per pawn), invalid ones are counted as UnexpectedChannelFailure.
//   - S10-R5 warm-up on every client AStealthCharacter (every creation path, every pawn kind, the owner's own pawn
//     too): hidden at PostNetInit (nothing renders between the spawn and PostNetInit, which run in one ProcessBunch),
//     presentation applied with the harness estimator once synced, the mesh in AlwaysTickPoseAndRefreshBones with
//     external tick rate 1, shown in the first OnBoneTransformsFinalized after the apply; failsafe ReopenWarmupMaxFrames.
//   - S10-R6 presentation times from the ADR-0002 section 9 harness estimator (UStealthSpikeClockComponent).
// Probes independent of the fix: a client census at every server commit (no lost pawns), the head-bone height of the
// component-space transforms at the first visible frame (OnWorldPostActorTick, after every tick group), screenshots on
// the render client for a silhouette check, per-re-open timestamps (un-cull, close sent, in-sequence acknowledgement,
// open sent with every send's packet id, open receipt with its packet id, spawn, apply, warm, first visible frame).
// Subjects: AI-possessed crowd pawns and player-possessed crowd pawns (possessed by server-side PlayerControllers with
// PlayerStates and no connection, like the listen host's own player), cycling through one rule.
// Controls: -StealthHarnessReopenGuard=0 (guard off), -StealthHarnessWarmup=0 (warm-up off),
// -StealthHarnessReopenAckTimeoutMs=200 -StealthHarnessInjectCloseDrops=2 (fault injection: the packet carrying a
// case-C close and its first resend are dropped on that connection by the per-connection loss emulation).
// THROWAWAY spike code (ADR-0003: the production mechanism is stories 023-025). Analyser: tools/ci/mp/analyze_s10r.py.
#include "Scenarios/StealthHarnessRun.h"

#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/Spikes/StealthSpikeHooks.h"
#include "Spikes/Visibility/StealthSpikeActionStateComponent.h"
#include "Spikes/Visibility/StealthSpikeClockComponent.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthNpcController.h"
#include "StealthTestHarnessModule.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "Engine/Channel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Net/DataBunch.h"
#include "Net/DataChannel.h"
#include "Templates/Atomic.h"
#include "UnrealClient.h"

namespace StealthHarness
{
	namespace S10R
	{
		constexpr double PawnNetUpdateHz = 15.0;
		constexpr double ConcealCullDelaySec = 0.250;
		constexpr double HoldCulledSec = 1.2;
		constexpr double HoldVisibleSec = 1.5;
		constexpr int32 RaceFrames = 2;
		constexpr double CrowdWaitCeiling = 40.0;
		constexpr double ObservationCeilingSec = 10.0;
		constexpr int32 RefVisibleFrame = 10;

		/** The retired prototype kill's body life (its BodyLifetime default): a killed subject's body is removed after it. */
		constexpr float BodyLifeSec = 10.0f;

		/**
		 * Kills Pawn by the production death path (story asn-027): FDeathState{Now} through AStealthPawn::ApplyDeathWrite
		 * (the forced update, the freeze and the death-write re-open), then what the harness's crowd needs: the controller
		 * released (an NPC's destroyed) and the body removed after BodyLifeSec.
		 */
		inline void KillByDeathWrite(AStealthCharacter& Pawn, double Now)
		{
			FDeathState Death;
			Death.DeathServerTime = Now;
			Pawn.ApplyDeathWrite(Death);
			if (AController* Controller = Pawn.GetController())
			{
				const bool bIsPlayer = Controller->IsPlayerController();
				Controller->UnPossess();
				if (!bIsPlayer)
				{
					Controller->Destroy();
				}
			}
			Pawn.SetLifeSpan(BodyLifeSec);
		}
	}

	/** Counts ensures and error/warning log lines by category (as S10). */
	class FS10RFaultCounter final : public FOutputDevice
	{
	public:
		TMap<FName, int32> Errors;
		TMap<FName, int32> Warnings;
		TAtomic<int32> Ensures{ 0 };
		TAtomic<int32> FailedToFindCreateActor{ 0 };
		mutable FCriticalSection Mutex;

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			FScopeLock Lock(&Mutex);
			if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Fatal)
			{
				Errors.FindOrAdd(Category)++;
			}
			else if (Verbosity == ELogVerbosity::Warning)
			{
				Warnings.FindOrAdd(Category)++;
				if (V && FCString::Strstr(V, TEXT("failed to find/create ACTOR")))
				{
					++FailedToFindCreateActor;
				}
			}
		}

		virtual bool CanBeUsedOnAnyThread() const override { return true; }

		FString Describe() const
		{
			FScopeLock Lock(&Mutex);
			FString Out = FString::Printf(TEXT("\"ensures\":%d,\"failedToFindCreateActor\":%d,\"errors\":{"), Ensures.Load(), FailedToFindCreateActor.Load());
			int32 Index = 0;
			for (const TPair<FName, int32>& Pair : Errors)
			{
				Out += FString::Printf(TEXT("%s%s:%d"), Index++ ? TEXT(",") : TEXT(""), *FRun::Q(Pair.Key.ToString()), Pair.Value);
			}
			Out += TEXT("},\"warnings\":{");
			Index = 0;
			for (const TPair<FName, int32>& Pair : Warnings)
			{
				Out += FString::Printf(TEXT("%s%s:%d"), Index++ ? TEXT(",") : TEXT(""), *FRun::Q(Pair.Key.ToString()), Pair.Value);
			}
			return Out + TEXT("}");
		}
	};

	enum class ES10RState : uint8 { Visible, CullPending, Culled };
	enum class ES10RCase : uint8 { A, B, C };
	enum class ES10RKind : uint8 { Ai, Player };

	/** The kind as a JSON string literal. */
	static const TCHAR* KindName(ES10RKind Kind) { return Kind == ES10RKind::Ai ? TEXT("\"ai\"") : TEXT("\"player\""); }

	/** S10-R2 pending-close entry (the ADR's FPendingChannelClose plus harness bookkeeping). */
	struct FS10RPendingClose
	{
		TWeakObjectPtr<UNetConnection> Connection;
		const UActorChannel* ClosedChannel = nullptr; // compared, dereferenced only while it is still in its slot
		int32 ChIndex = INDEX_NONE;
		double CloseSentServerTime = 0.0;
		double CloseSentT = 0.0;
		bool bStale = false;
		bool bRepair = false;
		int32 Cycle = 0;
		int32 Case = 0;
		/** Packet ids the close bunch went out in (first send, then resends). */
		TArray<int32> ClosePackets;
		/** Fault injection: packets carrying this close still to drop. */
		int32 DropsLeft = 0;
	};

	/** Per (subject, connection) state. */
	struct FS10RPair
	{
		TArray<FS10RPendingClose> Entries;
		bool bUnverified = false;
		double UnverifiedClearAt = -1.0;
		/** The open channel last seen for the subject on this connection (null after a close). */
		const UActorChannel* OpenChannel = nullptr;
		int32 OpenChIndex = INDEX_NONE;
		/** Packet ids the current channel's open bunch went out in; tracking stops at OpenAcked. */
		TArray<int32> OpenPackets;
		bool bOpenTracking = false;
		/** QPC time a re-open was first found held for this pair (-1: not held). */
		double HeldSinceT = -1.0;
		bool bNextOpenIsRepair = false;
		bool bRepairQueued = false;
	};

	struct FS10RSubject
	{
		TWeakObjectPtr<AStealthCharacter> Pawn;
		TWeakObjectPtr<APlayerController> PlayerController; // player-kind subjects only
		ES10RKind Kind = ES10RKind::Ai;
		FString Guid;
		FVector HoldAt = FVector::ZeroVector;
		ES10RState State = ES10RState::Visible;
		double NextCommitAt = 0.0;
		double CullDeadline = 0.0;
		double CulledAt = 0.0;
		uint64 ClosedFrame = 0;
		int32 Cycle = 0;
		ES10RCase Case = ES10RCase::A;
		bool bInjectThisCycle = false;
		TMap<UNetConnection*, FS10RPair> Pairs;
	};

	/** A queued lost-pawn report (server, S10-R4). */
	struct FS10RRepairReport
	{
		TWeakObjectPtr<AActor> Pawn;
		TWeakObjectPtr<UNetConnection> Connection;
		const UActorChannel* Channel = nullptr;
		int32 ChIndex = INDEX_NONE;
		double ReceivedT = 0.0;
	};

	/** Client-side per pawn-instance warm-up and probe state. */
	struct FS10RClientPawn
	{
		TWeakObjectPtr<AStealthCharacter> Pawn;
		FString Name;
		FString Guid;
		bool bSubject = false;
		bool bPostNetInit = false;
		bool bApplied = false;
		bool bWarm = false;
		bool bShown = false;
		bool bFirstVisibleDone = false;
		bool bRefDone = false;
		bool bClockWaitRecorded = false;
		uint64 PostNetInitFrame = 0;
		uint64 AppliedFrame = 0;
		int32 VisibleFrames = 0;
		int32 ChannelOpenCount = 0;
		int32 LastOpenChIndex = INDEX_NONE;
		FNetworkGUID OpenNetGUID;
		FDelegateHandle FinalizeHandle;
		EVisibilityBasedAnimTickOption PreviousTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
		int32 CaptureStage = -1; // render client: frames captured after the first visible frame
	};

	class FS10RScenario;

	struct FS10RCullTickFunction : public FTickFunction
	{
		FS10RScenario* Owner = nullptr;
		virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& CompletionGraphEvent) override;
		virtual FString DiagnosticMessage() override { return TEXT("StealthHarness S10R cull tick"); }
	};

	class FS10RScenario final : public IScenario
	{
	public:
		FS10RScenario()
		{
			GLog->AddOutputDevice(&Faults);
			EnsureHandle = FCoreDelegates::OnHandleSystemEnsure.AddLambda([this]() { ++Faults.Ensures; });
			bGuard = FRun::IntParam(TEXT("ReopenGuard"), 1) != 0;
			bRepair = FRun::IntParam(TEXT("Repair"), bGuard ? 1 : 0) != 0;
			bProbe = FRun::IntParam(TEXT("LostProbe"), 1) != 0;
			bWarmup = FRun::IntParam(TEXT("Warmup"), 1) != 0;
			ReopenAckTimeoutSec = FRun::IntParam(TEXT("ReopenAckTimeoutMs"), 350) / 1000.0;
			WarmupMaxFrames = FRun::IntParam(TEXT("ReopenWarmupMaxFrames"), 3);
			RepairMaxPerSec = FRun::IntParam(TEXT("ChannelRepairMaxPerSec"), 4);
			InjectCloseDrops = FRun::IntParam(TEXT("InjectCloseDrops"), 0);
			AiSubjects = FRun::IntParam(TEXT("AiSubjects"), 2);
			PlayerSubjects = FRun::IntParam(TEXT("PlayerSubjects"), 2);
			CaptureBudget = FRun::IntParam(TEXT("CaptureReopens"), 0);
			// Client hooks are process-wide; installed at construction so the first pawns of the join are covered.
			UStealthSpikeActionStateComponent::ExternalApply() = true;
			StealthSpikeHooks::SetPostNetInitHook([this](AActor* Pawn) { OnClientPostNetInit(Pawn); });
			StealthSpikeHooks::SetChannelOpenHook([this](AActor* Pawn, int32 ChIndex, int32 PacketId, UNetConnection* Connection) { OnClientChannelOpen(Pawn, ChIndex, PacketId, Connection); });
			StealthSpikeHooks::SetChannelFailureHook([this](APlayerController* Controller, UActorChannel* Channel) { OnServerChannelFailure(Controller, Channel); });
			TickStartHandle = FWorldDelegates::OnWorldTickStart.AddLambda([this](UWorld* World, ELevelTick, float) { OnWorldTickStart(World); });
			PostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([this](UWorld* World, ELevelTick, float) { OnWorldPostActorTick(World); });
		}

		virtual ~FS10RScenario() override
		{
			GLog->RemoveOutputDevice(&Faults);
			FCoreDelegates::OnHandleSystemEnsure.Remove(EnsureHandle);
			FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle);
			FWorldDelegates::OnWorldPostActorTick.Remove(PostActorTickHandle);
		}

		virtual void OnFinish(FRun& Run) override
		{
			Run.Write(TEXT("faults"), FString::Printf(TEXT("%s,%s"), *Faults.Describe(), *CounterFields()));
			if (CullTick.IsTickFunctionRegistered())
			{
				CullTick.UnRegisterTickFunction();
			}
			StealthSpikeHooks::SetRelevancyOverride({});
			StealthSpikeHooks::SetPostNetInitHook({});
			StealthSpikeHooks::SetChannelOpenHook({});
			StealthSpikeHooks::SetChannelFailureHook({});
			WorldPtr = nullptr;
		}

		// ================================================================ server ================================

		virtual void OnAllJoined(FRun& Run, UWorld* World) override
		{
			RunPtr = &Run;
			WorldPtr = World;
			Duration = FRun::IntParam(TEXT("Duration"), 640);
			if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
			{
				GameMode->StartRound();
			}
			GridOrigin = World->GetTimeSeconds();
			Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"startRound\",\"gridOrigin\":%.6f,\"duration\":%d,%s"), GridOrigin, Duration, *FRun::ServerTimeFields(World)));
			Run.Write(TEXT("s10rConfig"), FString::Printf(TEXT("\"guard\":%d,\"repair\":%d,\"probe\":%d,\"warmup\":%d,\"reopenAckTimeoutMs\":%.0f,\"warmupMaxFrames\":%d,\"repairMaxPerSec\":%d,\"injectCloseDrops\":%d,\"aiSubjects\":%d,\"playerSubjects\":%d"),
				(int32)bGuard, (int32)bRepair, (int32)bProbe, (int32)bWarmup, ReopenAckTimeoutSec * 1000.0, WarmupMaxFrames, RepairMaxPerSec, InjectCloseDrops, AiSubjects, PlayerSubjects));
			// ADR-0002 section 9 exchange on every client's controller (owner only).
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (PC && PC->GetNetConnection() && !PC->FindComponentByClass<UStealthSpikeClockComponent>())
				{
					UStealthSpikeClockComponent* Clock = NewObject<UStealthSpikeClockComponent>(PC, TEXT("SpikeClock"));
					Clock->RegisterComponent();
					PC->AddInstanceComponent(Clock);
				}
			}
			StealthSpikeHooks::SetRelevancyOverride([this](const AActor* Subject, const AActor* RealViewer, const AActor*) -> TOptional<bool>
			{
				return Relevancy(Subject, RealViewer);
			});
			CullTick.Owner = this;
			CullTick.TickGroup = TG_PostUpdateWork;
			CullTick.bCanEverTick = true;
			CullTick.bTickEvenWhenPaused = true;
			CullTick.RegisterTickFunction(World->PersistentLevel);
		}

		virtual bool TickServer(FRun& Run, UWorld* World, double ScenarioSeconds) override
		{
			const double Now = World->GetTimeSeconds();
			if (ScenarioSeconds >= Duration)
			{
				Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"end\",\"cycles\":%d,%s"), TotalCycles, *FRun::ServerTimeFields(World)));
				return true;
			}
			if (CrowdAt < 0.0)
			{
				// Wait for the production round to be Active (its entry rebuilds the crowd and hitches the listen host for
				// ~0.6-1.3 s, which steps TimeSeconds against the platform clock), then a settle window longer than the
				// ADR-0002 estimator's 16-sample window (2 s cadence: 32 s), so cycles run on a steady clock.
				const URoundMatchStore* Round = URoundMatchStore::Get(World);
				if (Round != nullptr && Round->GetPhase() == ERoundPhase::Active)
				{
					ActiveSince = ActiveSince < 0.0 ? Now : ActiveSince;
				}
				if (ActiveSince < 0.0 || Now - ActiveSince < SettleSeconds)
				{
					if (Now - GridOrigin > 120.0 && ActiveSince < 0.0 && !bActiveWaitReported)
					{
						bActiveWaitReported = true;
						Run.Write(TEXT("event"), TEXT("\"ev\":\"roundNotActive\""));
					}
					return false;
				}
				int32 Crowd = 0;
				for (TActorIterator<AStealthCharacter> It(World); It; ++It)
				{
					Crowd += Cast<AStealthNpcController>(It->GetController()) ? 1 : 0;
				}
				const int32 Wanted = AiSubjects + PlayerSubjects;
				if (Crowd >= Wanted + 2 || Now - GridOrigin > S10R::CrowdWaitCeiling)
				{
					CrowdAt = Now;
					Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"crowd\",\"npcs\":%d,%s"), Crowd, *FRun::ServerTimeFields(World)));
					// Interleaved kinds, so the two kinds share positions in front of the render client equally.
					Subjects.SetNum(Wanted);
					for (int32 Index = 0; Index < Wanted; ++Index)
					{
						// Kinds in the order ai, player, player, ai (repeating), so both kinds sit at the same mean distance.
						const int32 Players = Subjects.FilterByPredicate([](const FS10RSubject& S) { return S.Kind == ES10RKind::Player; }).Num();
						const bool bWantPlayer = (Index % 4 == 1 || Index % 4 == 2);
						const bool bPlayer = (bWantPlayer && Players < PlayerSubjects) || (Index - Players >= AiSubjects);
						Subjects[Index].Kind = bPlayer ? ES10RKind::Player : ES10RKind::Ai;
					}
					for (int32 Index = 0; Index < Wanted; ++Index)
					{
						ReplaceSubject(Run, World, Index, Now + 1.0 + 0.8 * Index);
					}
				}
				return false;
			}
			for (int32 Index = 0; Index < Subjects.Num(); ++Index)
			{
				FS10RSubject& Subject = Subjects[Index];
				AStealthCharacter* Pawn = Subject.Pawn.Get();
				if (!Pawn || Pawn->IsDying())
				{
					if (Subject.State == ES10RState::Visible && Now >= Subject.NextCommitAt && !IsObservationOpen(Subject))
					{
						ReplaceSubject(Run, World, Index, Now + 0.5); // a killed subject is replaced by a fresh crowd pawn
					}
					continue;
				}
				Hold(Subject);
				if (Subject.State == ES10RState::Visible && Now >= Subject.NextCommitAt)
				{
					// Every (pawn, connection) pair is observed until its unverified flag clears (S10-R7 (1)).
					const bool bOpen = IsObservationOpen(Subject);
					if (!bOpen || Now - Subject.NextCommitAt > S10R::ObservationCeilingSec)
					{
						if (bOpen)
						{
							++ObservationCeilings;
							Run.Write(TEXT("observationCeiling"), FString::Printf(TEXT("\"subject\":%s,\"cycle\":%d"), *FRun::Q(Subject.Guid), Subject.Cycle));
						}
						Commit(Run, World, Subject);
					}
				}
				else if (Subject.State == ES10RState::Culled && Subject.Case != ES10RCase::C && Now - Subject.CulledAt >= S10R::HoldCulledSec)
				{
					Uncull(Run, World, Subject, Index);
				}
			}
			return false;
		}

		/** TG_PostUpdateWork, every server frame: acknowledgements, timeouts, opens, due culls, race un-culls, repairs. */
		void ExecuteCullTick()
		{
			if (!WorldPtr || !RunPtr)
			{
				return;
			}
			UWorld* World = WorldPtr;
			const double Now = World->GetTimeSeconds();
			const double NowT = FPlatformTime::Seconds();
			UNetDriver* Driver = World->GetNetDriver();
			if (!Driver)
			{
				return;
			}
			UStealthSpikeClockComponent::NoteServerFrame(World);
			if (GFrameCounter % 6 == 0)
			{
				// Truth for the clock error: server time = st + (QPC - fs) within the frame (ADR-0002 section 9 precise clock).
				RunPtr->Write(TEXT("srvClk"), FString::Printf(TEXT("\"fs\":%.6f,\"st\":%.6f"), FApp::GetCurrentTime(), World->GetTimeSeconds()));
			}
			for (int32 Index = 0; Index < Subjects.Num(); ++Index)
			{
				FS10RSubject& Subject = Subjects[Index];
				AStealthCharacter* Pawn = Subject.Pawn.Get();
				for (UNetConnection* Connection : Driver->ClientConnections)
				{
					if (!Connection)
					{
						continue;
					}
					FS10RPair& Pair = Subject.Pairs.FindOrAdd(Connection);
					UpdateEntries(Subject, Connection, Pair, Now, NowT);
					if (Pawn)
					{
						UpdateOpen(Subject, Pawn, Connection, Pair, NowT);
					}
				}
				if (!Pawn)
				{
					continue;
				}
				if (Subject.State == ES10RState::CullPending && Now >= Subject.CullDeadline)
				{
					CloseAll(Subject, Pawn, Driver, Now, NowT);
				}
				else if (Subject.State == ES10RState::Culled && Subject.Case == ES10RCase::C && GFrameCounter >= Subject.ClosedFrame + S10R::RaceFrames)
				{
					Uncull(*RunPtr, World, Subject, Index);
				}
			}
			ProcessRepairs(Driver, Now, NowT);
		}

	private:
		/** The relevancy hook: culled -> nobody; a non-stale pending close for this connection -> held (S10-R2). */
		TOptional<bool> Relevancy(const AActor* Actor, const AActor* RealViewer)
		{
			FS10RSubject* Subject = Subjects.FindByPredicate([Actor](const FS10RSubject& S) { return S.Pawn.Get() == Actor; });
			if (!Subject)
			{
				return true; // as S10: every non-subject stays relevant to every viewer
			}
			if (Subject->State == ES10RState::Culled)
			{
				return false;
			}
			const APlayerController* PC = Cast<APlayerController>(RealViewer);
			UNetConnection* Connection = PC ? PC->GetNetConnection() : nullptr;
			FS10RPair* Pair = Connection ? Subject->Pairs.Find(Connection) : nullptr;
			if (bGuard && Pair && Pair->Entries.ContainsByPredicate([](const FS10RPendingClose& E) { return !E.bStale; }))
			{
				if (Pair->HeldSinceT < 0.0)
				{
					Pair->HeldSinceT = FPlatformTime::Seconds();
				}
				return false; // re-open held
			}
			return true;
		}

		/** True while any pair of the subject is unverified, has a queued repair, or holds a re-open. */
		bool IsObservationOpen(const FS10RSubject& Subject) const
		{
			for (const TPair<UNetConnection*, FS10RPair>& It : Subject.Pairs)
			{
				const FS10RPair& Pair = It.Value;
				if (Pair.bUnverified || Pair.bRepairQueued || Pair.Entries.ContainsByPredicate([](const FS10RPendingClose& E) { return !E.bStale; }))
				{
					return true;
				}
			}
			return RepairQueue.ContainsByPredicate([&Subject](const FS10RRepairReport& R) { return R.Pawn.Get() == Subject.Pawn.Get(); });
		}

		static const FOutBunch* FindOutBunch(const UChannel* Channel, bool bClose)
		{
			for (const FOutBunch* Out = Channel ? Channel->OutRec : nullptr; Out; Out = Out->Next)
			{
				if (bClose ? Out->bClose : Out->bOpen)
				{
					return Out;
				}
			}
			return nullptr;
		}

		/** S10-R2/R3: in-sequence acknowledgement from the slot, timeouts, unverified clears, close resends (and drops). */
		void UpdateEntries(FS10RSubject& Subject, UNetConnection* Connection, FS10RPair& Pair, double Now, double NowT)
		{
			for (int32 EntryIndex = Pair.Entries.Num() - 1; EntryIndex >= 0; --EntryIndex)
			{
				FS10RPendingClose& Entry = Pair.Entries[EntryIndex];
				const bool bInSlot = Connection->Channels.IsValidIndex(Entry.ChIndex) && Connection->Channels[Entry.ChIndex].Get() == Entry.ClosedChannel;
				const bool bAcked = !bInSlot || !Entry.ClosedChannel->Closing;
				if (!bAcked)
				{
					// Still in the slot: the pointer is live. Track the close bunch's (re)sends.
					if (const FOutBunch* Close = FindOutBunch(Entry.ClosedChannel, /*bClose*/ true))
					{
						if (Entry.ClosePackets.Num() == 0 || Entry.ClosePackets.Last() != Close->PacketId)
						{
							Entry.ClosePackets.Add(Close->PacketId);
							if (Entry.ClosePackets.Num() > 1)
							{
								RunPtr->Write(TEXT("closeResend"), FString::Printf(TEXT("\"subject\":%s,\"c\":%s,\"ch\":%d,\"pkt\":%d,\"sends\":%d"),
									*FRun::Q(Subject.Guid), *FRun::Q(FRun::ConnectionName(Connection)), Entry.ChIndex, Close->PacketId, Entry.ClosePackets.Num()));
								if (Entry.DropsLeft > 0)
								{
									--Entry.DropsLeft;
									DropThisFrame(Connection, Subject, Entry);
								}
							}
						}
					}
					if (!Entry.bStale && Now - Entry.CloseSentServerTime >= ReopenAckTimeoutSec)
					{
						Entry.bStale = true;
						Pair.bUnverified = true;
						Pair.UnverifiedClearAt = -1.0;
						++ReopenAckTimeouts;
						RunPtr->Write(TEXT("reopenTimeout"), FString::Printf(TEXT("\"subject\":%s,\"kind\":%s,\"c\":%s,\"ch\":%d,\"repair\":%d,\"cycle\":%d,\"case\":%d,\"sinceSentMs\":%.3f,\"visible\":%d,\"closeSends\":%d"),
							*FRun::Q(Subject.Guid), KindName(Subject.Kind), *FRun::Q(FRun::ConnectionName(Connection)), Entry.ChIndex, (int32)Entry.bRepair, Entry.Cycle, Entry.Case,
							(NowT - Entry.CloseSentT) * 1000.0, (int32)(Subject.State == ES10RState::Visible), Entry.ClosePackets.Num()));
						ReleaseHold(Subject, Connection, Pair, NowT, TEXT("timeout"));
					}
					continue;
				}
				const bool bWasStale = Entry.bStale;
				RunPtr->Write(TEXT("closeAck"), FString::Printf(TEXT("\"subject\":%s,\"kind\":%s,\"c\":%s,\"ch\":%d,\"repair\":%d,\"cycle\":%d,\"case\":%d,\"sentT\":%.6f,\"ackMs\":%.3f,\"stale\":%d,\"closeSends\":%d,\"pooledReuse\":%d"),
					*FRun::Q(Subject.Guid), KindName(Subject.Kind), *FRun::Q(FRun::ConnectionName(Connection)), Entry.ChIndex, (int32)Entry.bRepair, Entry.Cycle, Entry.Case,
					Entry.CloseSentT, (NowT - Entry.CloseSentT) * 1000.0, (int32)bWasStale, Entry.ClosePackets.Num(), (int32)bInSlot));
				Pair.Entries.RemoveAt(EntryIndex);
				if (bWasStale && Pair.bUnverified && !Pair.Entries.ContainsByPredicate([](const FS10RPendingClose& E) { return E.bStale; }))
				{
					Pair.UnverifiedClearAt = Now + 2.0 * ReopenAckTimeoutSec;
				}
				if (!bWasStale && !Pair.Entries.ContainsByPredicate([](const FS10RPendingClose& E) { return !E.bStale; }))
				{
					ReleaseHold(Subject, Connection, Pair, NowT, TEXT("ack"));
				}
			}
			if (Pair.bUnverified && Pair.UnverifiedClearAt > 0.0 && Now >= Pair.UnverifiedClearAt && !Pair.bRepairQueued)
			{
				SetVerified(Subject, Connection, Pair, TEXT("ackPlus2Timeout"));
			}
		}

		void SetVerified(FS10RSubject& Subject, UNetConnection* Connection, FS10RPair& Pair, const TCHAR* Why)
		{
			if (!Pair.bUnverified)
			{
				return;
			}
			Pair.bUnverified = false;
			Pair.UnverifiedClearAt = -1.0;
			RunPtr->Write(TEXT("verified"), FString::Printf(TEXT("\"subject\":%s,\"c\":%s,\"why\":%s"), *FRun::Q(Subject.Guid), *FRun::Q(FRun::ConnectionName(Connection)), *FRun::Q(Why)));
		}

		void ReleaseHold(FS10RSubject& Subject, UNetConnection* Connection, FS10RPair& Pair, double NowT, const TCHAR* Why)
		{
			if (Pair.HeldSinceT >= 0.0)
			{
				RunPtr->Write(TEXT("holdRelease"), FString::Printf(TEXT("\"subject\":%s,\"kind\":%s,\"c\":%s,\"heldMs\":%.3f,\"why\":%s,\"cycle\":%d"),
					*FRun::Q(Subject.Guid), KindName(Subject.Kind), *FRun::Q(FRun::ConnectionName(Connection)), (NowT - Pair.HeldSinceT) * 1000.0, *FRun::Q(Why), Subject.Cycle));
				Pair.HeldSinceT = -1.0;
			}
		}

		/** New channel for the subject on this connection (opened by the last ServerReplicateActors) and its open (re)sends. */
		void UpdateOpen(FS10RSubject& Subject, AStealthCharacter* Pawn, UNetConnection* Connection, FS10RPair& Pair, double NowT)
		{
			UActorChannel* Channel = Connection->FindActorChannelRef(Pawn);
			if (Channel && Channel != Pair.OpenChannel && !Channel->Closing)
			{
				Pair.OpenChannel = Channel;
				Pair.OpenChIndex = Channel->ChIndex;
				Pair.OpenPackets.Reset();
				Pair.bOpenTracking = true;
				const FOutBunch* Open = FindOutBunch(Channel, /*bClose*/ false);
				const int32 FirstPacket = Open ? Open->PacketId : Channel->OpenPacketId.First;
				Pair.OpenPackets.Add(FirstPacket);
				RunPtr->Write(TEXT("open"), FString::Printf(TEXT("\"subject\":%s,\"kind\":%s,\"c\":%s,\"ch\":%d,\"pkt\":%d,\"cycle\":%d,\"repair\":%d,\"held\":%d,\"heldMs\":%.3f,\"state\":%d,\"guid\":%s,%s"),
					*FRun::Q(Subject.Guid), KindName(Subject.Kind), *FRun::Q(FRun::ConnectionName(Connection)), Channel->ChIndex, FirstPacket, Subject.Cycle,
					(int32)Pair.bNextOpenIsRepair, (int32)(Pair.HeldSinceT >= 0.0), Pair.HeldSinceT >= 0.0 ? (NowT - Pair.HeldSinceT) * 1000.0 : 0.0, (int32)Subject.State,
					*FRun::Q(FRun::Guid(WorldPtr, Pawn)), *FRun::ServerTimeFields(WorldPtr)));
				Pair.bNextOpenIsRepair = false;
				Pair.HeldSinceT = -1.0;
				return;
			}
			if (Channel && Channel == Pair.OpenChannel && Pair.bOpenTracking)
			{
				if (Channel->OpenAcked)
				{
					Pair.bOpenTracking = false;
					RunPtr->Write(TEXT("openAcked"), FString::Printf(TEXT("\"subject\":%s,\"c\":%s,\"ch\":%d,\"sends\":%d"),
						*FRun::Q(Subject.Guid), *FRun::Q(FRun::ConnectionName(Connection)), Channel->ChIndex, Pair.OpenPackets.Num()));
				}
				else if (const FOutBunch* Open = FindOutBunch(Channel, /*bClose*/ false); Open && Open->PacketId != Pair.OpenPackets.Last())
				{
					Pair.OpenPackets.Add(Open->PacketId);
					RunPtr->Write(TEXT("openResend"), FString::Printf(TEXT("\"subject\":%s,\"c\":%s,\"ch\":%d,\"pkt\":%d,\"sends\":%d"),
						*FRun::Q(Subject.Guid), *FRun::Q(FRun::ConnectionName(Connection)), Channel->ChIndex, Open->PacketId, Pair.OpenPackets.Num()));
				}
			}
		}

		/** Closes one channel for the Policy and adds the S10-R2 entry when the close was actually sent. */
		bool CloseChannel(FS10RSubject& Subject, UNetConnection* Connection, FS10RPair& Pair, UActorChannel* Channel, bool bRepairClose, double Now, double NowT)
		{
			const int32 ChIndex = Channel->ChIndex;
			Channel->Close(EChannelCloseReason::Relevancy);
			Pair.OpenChannel = nullptr;
			Pair.bOpenTracking = false;
			// "or at the pawn's next close to that connection" (S10-R3).
			SetVerified(Subject, Connection, Pair, bRepairClose ? TEXT("repairClose") : TEXT("nextClose"));
			if (!Channel->Closing)
			{
				++CloseNotSent;
				RunPtr->Write(TEXT("closeNotSent"), FString::Printf(TEXT("\"subject\":%s,\"c\":%s,\"ch\":%d"), *FRun::Q(Subject.Guid), *FRun::Q(FRun::ConnectionName(Connection)), ChIndex));
				return false;
			}
			FS10RPendingClose& Entry = Pair.Entries.AddDefaulted_GetRef();
			Entry.Connection = Connection;
			Entry.ClosedChannel = Channel;
			Entry.ChIndex = ChIndex;
			Entry.CloseSentServerTime = Now;
			Entry.CloseSentT = NowT;
			Entry.bRepair = bRepairClose;
			Entry.Cycle = Subject.Cycle;
			Entry.Case = (int32)Subject.Case;
			if (const FOutBunch* Close = FindOutBunch(Channel, /*bClose*/ true))
			{
				Entry.ClosePackets.Add(Close->PacketId);
			}
			if (!bRepairClose && Subject.bInjectThisCycle && InjectCloseDrops > 0)
			{
				Entry.DropsLeft = InjectCloseDrops - 1;
				DropThisFrame(Connection, Subject, Entry);
			}
			return true;
		}

		/** Fault injection: the per-connection loss emulation drops every packet this connection flushes this frame. */
		void DropThisFrame(UNetConnection* Connection, const FS10RSubject& Subject, const FS10RPendingClose& Entry)
		{
#if DO_ENABLE_NET_TEST
			if (!DroppedConnections.Contains(Connection))
			{
				DroppedConnections.Add(Connection, Connection->PacketSimulationSettings.PktLoss);
				Connection->PacketSimulationSettings.PktLoss = 100;
			}
			++InjectedDrops;
			RunPtr->Write(TEXT("inject"), FString::Printf(TEXT("\"subject\":%s,\"c\":%s,\"ch\":%d,\"closeSends\":%d"),
				*FRun::Q(Subject.Guid), *FRun::Q(FRun::ConnectionName(Connection)), Entry.ChIndex, Entry.ClosePackets.Num()));
#endif
		}

		void OnWorldTickStart(UWorld* World)
		{
#if DO_ENABLE_NET_TEST
			if (World == WorldPtr && DroppedConnections.Num() > 0)
			{
				for (const TPair<UNetConnection*, int32>& It : DroppedConnections)
				{
					if (World->GetNetDriver() && World->GetNetDriver()->ClientConnections.Contains(It.Key))
					{
						It.Key->PacketSimulationSettings.PktLoss = It.Value;
					}
				}
				DroppedConnections.Reset();
			}
#endif
		}

		void CloseAll(FS10RSubject& Subject, AStealthCharacter* Pawn, UNetDriver* Driver, double Now, double NowT)
		{
			RefreshGuid(Subject);
			Subject.State = ES10RState::Culled;
			Subject.CulledAt = Now;
			Subject.ClosedFrame = GFrameCounter;
			FString Closes;
			for (UNetConnection* Connection : Driver->ClientConnections)
			{
				UActorChannel* Channel = Connection ? Connection->FindActorChannelRef(Pawn) : nullptr;
				if (!Channel)
				{
					continue;
				}
				FS10RPair& Pair = Subject.Pairs.FindOrAdd(Connection);
				const int32 ChIndex = Channel->ChIndex;
				const bool bSent = CloseChannel(Subject, Connection, Pair, Channel, /*bRepairClose*/ false, Now, NowT);
				Closes += FString::Printf(TEXT("%s{\"c\":%s,\"ch\":%d,\"sent\":%d}"), Closes.IsEmpty() ? TEXT("") : TEXT(","), *FRun::Q(FRun::ConnectionName(Connection)), ChIndex, (int32)bSent);
			}
			RunPtr->Write(TEXT("cullClose"), FString::Printf(TEXT("\"subject\":%s,\"kind\":%s,\"cycle\":%d,\"case\":%d,\"inject\":%d,\"closes\":[%s],\"deadline\":%.6f,\"lateBy\":%.6f,%s"),
				*FRun::Q(Subject.Guid), KindName(Subject.Kind), Subject.Cycle, (int32)Subject.Case, (int32)Subject.bInjectThisCycle, *Closes, Subject.CullDeadline, Now - Subject.CullDeadline, *FRun::ServerTimeFields(WorldPtr)));
		}

		/** S10-R4 server half: the engine calls this inside UControlChannel::ReceivedBunch; only queue. */
		void OnServerChannelFailure(APlayerController* Controller, UActorChannel* Channel)
		{
			if (!RunPtr || !Channel)
			{
				return;
			}
			UNetConnection* Connection = Controller ? Controller->GetNetConnection() : nullptr;
			AActor* Actor = Channel->Actor;
			const bool bPawn = Cast<AStealthCharacter>(Actor) != nullptr;
			++FailureReports;
			RunPtr->Write(TEXT("failureReport"), FString::Printf(TEXT("\"c\":%s,\"ch\":%d,\"actor\":%s,\"guid\":%s,\"pawn\":%d,\"closing\":%d,\"repairOn\":%d"),
				*FRun::Q(FRun::ConnectionName(Connection)), Channel->ChIndex, *FRun::Q(GetNameSafe(Actor)), *FRun::Q(FRun::Guid(WorldPtr, Actor)), (int32)bPawn, (int32)Channel->Closing, (int32)bRepair));
			if (!bRepair || !bPawn || !Connection)
			{
				if (bRepair)
				{
					++UnexpectedChannelFailure;
					RunPtr->Write(TEXT("unexpectedChannelFailure"), FString::Printf(TEXT("\"why\":\"notPawnOrNoConnection\",\"actor\":%s"), *FRun::Q(GetNameSafe(Actor))));
				}
				return;
			}
			// One queued entry per (pawn, connection): a duplicate is the same report.
			if (RepairQueue.ContainsByPredicate([Actor, Connection](const FS10RRepairReport& R) { return R.Pawn.Get() == Actor && R.Connection.Get() == Connection; }))
			{
				RunPtr->Write(TEXT("repairDuplicate"), FString::Printf(TEXT("\"c\":%s,\"ch\":%d"), *FRun::Q(FRun::ConnectionName(Connection)), Channel->ChIndex));
				return;
			}
			FS10RRepairReport& Report = RepairQueue.AddDefaulted_GetRef();
			Report.Pawn = Actor;
			Report.Connection = Connection;
			Report.Channel = Channel;
			Report.ChIndex = Channel->ChIndex;
			Report.ReceivedT = FPlatformTime::Seconds();
			if (FS10RSubject* Subject = Subjects.FindByPredicate([Actor](const FS10RSubject& S) { return S.Pawn.Get() == Actor; }))
			{
				Subject->Pairs.FindOrAdd(Connection).bRepairQueued = true;
			}
		}

		/** Cull tick: re-check each queued report, rate-limited per connection (deferred, never dropped). */
		void ProcessRepairs(UNetDriver* Driver, double Now, double NowT)
		{
			for (int32 ReportIndex = 0; ReportIndex < RepairQueue.Num();)
			{
				FS10RRepairReport& Report = RepairQueue[ReportIndex];
				UNetConnection* Connection = Report.Connection.Get();
				AActor* Actor = Report.Pawn.Get();
				FS10RSubject* Subject = Actor ? Subjects.FindByPredicate([Actor](const FS10RSubject& S) { return S.Pawn.Get() == Actor; }) : nullptr;
				FS10RPair* Pair = (Subject && Connection) ? Subject->Pairs.Find(Connection) : nullptr;
				const bool bSlot = Connection && Connection->Channels.IsValidIndex(Report.ChIndex) && Connection->Channels[Report.ChIndex].Get() == Report.Channel;
				const UActorChannel* Live = bSlot ? Cast<UActorChannel>(Connection->Channels[Report.ChIndex]) : nullptr;
				const bool bChannelOk = Live && Live->Actor == Actor && !Live->Closing;
				const bool bRelevant = Subject && Subject->State == ES10RState::Visible && Pair
					&& !Pair->Entries.ContainsByPredicate([](const FS10RPendingClose& E) { return !E.bStale; });
				const bool bUnverifiedPair = Pair && Pair->bUnverified;
				if (!bChannelOk || !bRelevant || !bUnverifiedPair)
				{
					++UnexpectedChannelFailure;
					RunPtr->Write(TEXT("unexpectedChannelFailure"), FString::Printf(TEXT("\"subject\":%s,\"c\":%s,\"ch\":%d,\"slot\":%d,\"channelOk\":%d,\"relevant\":%d,\"unverified\":%d"),
						*FRun::Q(Subject ? Subject->Guid : FString(TEXT("?"))), *FRun::Q(FRun::ConnectionName(Connection)), Report.ChIndex, (int32)bSlot, (int32)bChannelOk, (int32)bRelevant, (int32)bUnverifiedPair));
					if (Pair)
					{
						Pair->bRepairQueued = false;
					}
					RepairQueue.RemoveAt(ReportIndex);
					continue;
				}
				TArray<double>& Recent = RepairTimes.FindOrAdd(Connection);
				Recent.RemoveAll([NowT](double At) { return NowT - At > 1.0; });
				if (Recent.Num() >= RepairMaxPerSec)
				{
					++RepairDeferrals;
					RunPtr->Write(TEXT("repairDeferred"), FString::Printf(TEXT("\"subject\":%s,\"c\":%s,\"ch\":%d"), *FRun::Q(Subject->Guid), *FRun::Q(FRun::ConnectionName(Connection)), Report.ChIndex));
					++ReportIndex; // FIFO: later reports of this connection wait too; other connections continue
					continue;
				}
				Recent.Add(NowT);
				++Repairs;
				const double QueuedMs = (NowT - Report.ReceivedT) * 1000.0;
				Pair->bRepairQueued = false;
				Pair->bNextOpenIsRepair = true;
				const bool bSent = CloseChannel(*Subject, Connection, *Pair, const_cast<UActorChannel*>(Live), /*bRepairClose*/ true, Now, NowT);
				RunPtr->Write(TEXT("repair"), FString::Printf(TEXT("\"subject\":%s,\"kind\":%s,\"c\":%s,\"ch\":%d,\"queuedMs\":%.3f,\"sent\":%d,\"cycle\":%d,%s"),
					*FRun::Q(Subject->Guid), KindName(Subject->Kind), *FRun::Q(FRun::ConnectionName(Connection)), Report.ChIndex, QueuedMs, (int32)bSent, Subject->Cycle, *FRun::ServerTimeFields(WorldPtr)));
				RepairQueue.RemoveAt(ReportIndex);
			}
		}

		void ReplaceSubject(FRun& Run, UWorld* World, int32 Index, double FirstCommitAt)
		{
			APlayerController* Viewer = FRun::FindPlayer(World, TEXT("C3"));
			APawn* ViewerPawn = Viewer ? Viewer->GetPawn() : nullptr;
			const FVector Eye = ViewerPawn ? ViewerPawn->GetActorLocation() : FVector::ZeroVector;
			// The render client's camera follows its control rotation: place the subjects along it, clear of its own pawn.
			const FVector Forward = Viewer ? FRotator(0.0, Viewer->GetControlRotation().Yaw, 0.0).Vector()
				: (ViewerPawn ? ViewerPawn->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector);
			AStealthCharacter* Best = nullptr;
			for (TActorIterator<AStealthCharacter> It(World); It; ++It)
			{
				const bool bTaken = Subjects.ContainsByPredicate([&It](const FS10RSubject& S) { return S.Pawn.Get() == *It; });
				if (!bTaken && !It->IsDying() && Cast<AStealthNpcController>(It->GetController()) && (!Best
					|| FVector::DistSquared(It->GetActorLocation(), Eye) < FVector::DistSquared(Best->GetActorLocation(), Eye)))
				{
					Best = *It;
				}
			}
			FS10RSubject& Subject = Subjects[Index];
			const ES10RKind Kind = Subject.Kind;
			const TWeakObjectPtr<APlayerController> KeptController = Subject.PlayerController;
			const int32 Cycle = Subject.Cycle;
			Subject = FS10RSubject();
			Subject.Kind = Kind;
			Subject.PlayerController = KeptController;
			Subject.Cycle = Cycle;
			Subject.NextCommitAt = FirstCommitAt;
			if (!Best)
			{
				Run.Write(TEXT("event"), TEXT("\"ev\":\"noSubject\""));
				return;
			}
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
			// Lateral slots to the right of the viewer's own pawn (none behind it; L_Prototype's open side from the team
			// start the render client uses): 150, 280, 410, 540 cm for four subjects.
			const double Lateral = 150.0 + 130.0 * Index;
			Subject.HoldAt = Eye + Forward * 650.0 + Right * Lateral;
			Best->TeleportTo(Subject.HoldAt, (-Forward).Rotation());
			Subject.HoldAt = Best->GetActorLocation();
			if (Kind == ES10RKind::Player)
			{
				// Player-possessed: a server PlayerController with a PlayerState and no connection (as the listen host's own player).
				APlayerController* PC = Subject.PlayerController.Get();
				if (!PC)
				{
					FActorSpawnParameters Params;
					Params.Name = MakeUniqueObjectName(World->PersistentLevel, APlayerController::StaticClass(), *FString::Printf(TEXT("S10RSubjectPC_%d"), Index));
					PC = World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
					Subject.PlayerController = PC;
				}
				if (AController* Old = Best->GetController(); Old && Old != PC)
				{
					Old->UnPossess();
					Old->Destroy();
				}
				if (PC)
				{
					PC->Possess(Best);
				}
			}
			Subject.Pawn = Best;
			Subject.Guid = FRun::Guid(World, Best);
			UStealthSpikeActionStateComponent* Stub = NewObject<UStealthSpikeActionStateComponent>(Best, TEXT("SpikeActionState"));
			Stub->RegisterComponent();
			Best->AddInstanceComponent(Stub);
			Stub->State.StartServerTime = World->GetTimeSeconds();
			const AController* Controller = Best->GetController();
			Run.Write(TEXT("subject"), FString::Printf(TEXT("\"index\":%d,\"kind\":%s,\"guid\":%s,\"name\":%s,\"controller\":%s,\"playerControlled\":%d,\"hold\":[%.3f,%.3f,%.3f],%s"),
				Index, KindName(Kind), *FRun::Q(Subject.Guid), *FRun::Q(Best->GetName()), *FRun::Q(GetNameSafe(Controller)), (int32)Best->IsPlayerControlled(),
				Subject.HoldAt.X, Subject.HoldAt.Y, Subject.HoldAt.Z, *FRun::ServerTimeFields(World)));
		}

		void Hold(FS10RSubject& Subject)
		{
			AStealthCharacter* Pawn = Subject.Pawn.Get();
			if (AController* Controller = Pawn ? Pawn->GetController() : nullptr)
			{
				Controller->StopMovement();
			}
		}

		/** A subject picked before its first replication has no NetGUID yet ("0"): take it once it exists. */
		void RefreshGuid(FS10RSubject& Subject) const
		{
			if (Subject.Guid == TEXT("0") && Subject.Pawn.IsValid() && WorldPtr)
			{
				Subject.Guid = FRun::Guid(WorldPtr, Subject.Pawn.Get());
			}
		}

		void Commit(FRun& Run, UWorld* World, FS10RSubject& Subject)
		{
			RefreshGuid(Subject);
			const double Now = World->GetTimeSeconds();
			const double Period = 1.0 / S10R::PawnNetUpdateHz;
			const double Earliest = Now + S10R::ConcealCullDelaySec;
			// The census of this observation's end: every connection with an open channel must hold the pawn on its client.
			FString Census;
			if (UNetDriver* Driver = World->GetNetDriver(); Driver && Subject.Pawn.IsValid())
			{
				for (UNetConnection* Connection : Driver->ClientConnections)
				{
					const UActorChannel* Channel = Connection ? Connection->FindActorChannelRef(Subject.Pawn.Get()) : nullptr;
					Census += FString::Printf(TEXT("%s{\"c\":%s,\"open\":%d,\"ch\":%d}"), Census.IsEmpty() ? TEXT("") : TEXT(","),
						*FRun::Q(FRun::ConnectionName(Connection)), (int32)(Channel && !Channel->Closing), Channel ? Channel->ChIndex : -1);
				}
			}
			Subject.CullDeadline = GridOrigin + FMath::CeilToDouble((Earliest - GridOrigin) / Period - 1e-9) * Period;
			Subject.State = ES10RState::CullPending;
			Subject.Case = (Subject.Cycle % 20 == 19) ? ES10RCase::B : (Subject.Cycle % 2 == 0 ? ES10RCase::A : ES10RCase::C);
			Subject.bInjectThisCycle = InjectCloseDrops > 0 && Subject.Case == ES10RCase::C;
			Run.Write(TEXT("cullCommit"), FString::Printf(TEXT("\"subject\":%s,\"kind\":%s,\"cycle\":%d,\"case\":%d,\"commit\":%.6f,\"deadline\":%.6f,\"waitedMs\":%.3f,\"census\":[%s],%s"),
				*FRun::Q(Subject.Guid), KindName(Subject.Kind), Subject.Cycle, (int32)Subject.Case, Now, Subject.CullDeadline, (Now - Subject.NextCommitAt) * 1000.0, *Census, *FRun::ServerTimeFields(World)));
		}

		void Uncull(FRun& Run, UWorld* World, FS10RSubject& Subject, int32 Index)
		{
			AStealthCharacter* Pawn = Subject.Pawn.Get();
			const double Now = World->GetTimeSeconds();
			if (Subject.Case == ES10RCase::B && Pawn)
			{
				if (UStealthSpikeActionStateComponent* Stub = Pawn->FindComponentByClass<UStealthSpikeActionStateComponent>())
				{
					Stub->State.DeathServerTime = Now;
				}
				// The production death path (story asn-027 retired the prototype immediate kill): the pawn half of the death write
				// (FDeathState on every machine, presented as Death_Fall by the encounter presentation), then the harness's own
				// body bookkeeping that the retired kill did (controller released, the body removed after S10's body life).
				S10R::KillByDeathWrite(*Pawn, Now);
			}
			Subject.State = ES10RState::Visible; // the record returns to Visible in this step; held channels open later
			FString Held;
			for (TPair<UNetConnection*, FS10RPair>& It : Subject.Pairs)
			{
				if (It.Value.Entries.ContainsByPredicate([](const FS10RPendingClose& E) { return !E.bStale; }))
				{
					Held += FString::Printf(TEXT("%s%s"), Held.IsEmpty() ? TEXT("") : TEXT(","), *FRun::Q(FRun::ConnectionName(It.Key)));
				}
			}
			const FVector Location = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
			Run.Write(TEXT("uncull"), FString::Printf(TEXT("\"subject\":%s,\"kind\":%s,\"index\":%d,\"cycle\":%d,\"case\":%d,\"sinceClose\":%.6f,\"framesSinceClose\":%llu,\"guid\":%s,\"held\":[%s],\"loc\":[%.3f,%.3f,%.3f],%s"),
				*FRun::Q(Subject.Guid), KindName(Subject.Kind), Index, Subject.Cycle, (int32)Subject.Case, Now - Subject.CulledAt, (unsigned long long)(GFrameCounter - Subject.ClosedFrame),
				*FRun::Q(FRun::Guid(World, Pawn)), *Held, Location.X, Location.Y, Location.Z, *FRun::ServerTimeFields(World)));
			++Subject.Cycle;
			++TotalCycles;
			Subject.NextCommitAt = Now + S10R::HoldVisibleSec;
		}

		FString CounterFields() const
		{
			return FString::Printf(TEXT("\"reopenAckTimeout\":%d,\"closeNotSent\":%d,\"failureReports\":%d,\"repairs\":%d,\"repairDeferrals\":%d,\"unexpectedChannelFailure\":%d,\"injectedDrops\":%d,\"observationCeilings\":%d,")
				TEXT("\"warmupTimeout\":%d,\"clockGatedWaits\":%d,\"lostProbeQueued\":%d,\"lostProbeReported\":%d,\"lostProbeNotReported\":%d"),
				ReopenAckTimeouts, CloseNotSent, FailureReports, Repairs, RepairDeferrals, UnexpectedChannelFailure, InjectedDrops, ObservationCeilings,
				WarmupTimeouts, ClockGatedWaits, LostProbeQueued, LostProbeReported, LostProbeNotReported);
		}

		// ================================================================ client ================================

		/** Client: every new network pawn instance starts warm-up hidden (S10-R5). */
		void OnClientPostNetInit(AActor* Actor)
		{
			AStealthCharacter* Pawn = Cast<AStealthCharacter>(Actor);
			if (!Pawn || Pawn->GetNetMode() != NM_Client)
			{
				return;
			}
			EnsureClientWorld(Pawn->GetWorld());
			FS10RClientPawn& Info = ClientPawns.FindOrAdd(Pawn);
			Info.Pawn = Pawn;
			Info.Name = Pawn->GetName();
			Info.Guid = FRun::Guid(Pawn->GetWorld(), Pawn);
			Info.bSubject = Pawn->FindComponentByClass<UStealthSpikeActionStateComponent>() != nullptr;
			Info.bPostNetInit = true;
			Info.PostNetInitFrame = GFrameCounter;
			if (bWarmup)
			{
				Pawn->SetActorHiddenInGame(true);
			}
			ClientRun()->Write(TEXT("warm"), FString::Printf(TEXT("\"phase\":\"postNetInit\",\"name\":%s,\"guid\":%s,\"subject\":%d,\"dying\":%d,\"hidden\":%d,\"frame\":%llu"),
				*FRun::Q(Info.Name), *FRun::Q(Info.Guid), (int32)Info.bSubject, (int32)Pawn->IsDying(), (int32)Pawn->IsHidden(), (unsigned long long)GFrameCounter));
			TryApply(Info);
		}

		/** Client: presentation apply with the estimator, then warm-up mode (S10-R5); waits while the clock is unsynced. */
		void TryApply(FS10RClientPawn& Info)
		{
			AStealthCharacter* Pawn = Info.Pawn.Get();
			if (!Pawn || Info.bApplied)
			{
				return;
			}
			FStealthSpikeClockEstimator& Clock = FStealthSpikeClockEstimator::Get();
			if (!Clock.bSynced)
			{
				if (!Info.bClockWaitRecorded)
				{
					Info.bClockWaitRecorded = true;
					++ClockGatedWaits;
					ClientRun()->Write(TEXT("warm"), FString::Printf(TEXT("\"phase\":\"clockWait\",\"name\":%s,\"guid\":%s,\"subject\":%d"), *FRun::Q(Info.Name), *FRun::Q(Info.Guid), (int32)Info.bSubject));
				}
				return;
			}
			Info.bApplied = true;
			Info.AppliedFrame = GFrameCounter;
			const double EvalT = FPlatformTime::Seconds();
			const double Est = Clock.EstimatedServerNow(EvalT);
			if (UStealthSpikeActionStateComponent* Stub = Pawn->FindComponentByClass<UStealthSpikeActionStateComponent>())
			{
				Stub->ApplyPresentation(Est);
			}
			USkeletalMeshComponent* Mesh = Pawn->GetMesh();
			if (bWarmup && Mesh)
			{
				Info.PreviousTickOption = Mesh->VisibilityBasedAnimTickOption;
				Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
				Mesh->EnableExternalTickRateControl(true);
				Mesh->SetExternalTickRate(1);
				const TWeakObjectPtr<AStealthCharacter> Weak(Pawn);
				Info.FinalizeHandle = Mesh->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateLambda([this, Weak]()
				{
					OnClientBonesFinalized(Weak.Get());
				}));
			}
			ClientRun()->Write(TEXT("warm"), FString::Printf(TEXT("\"phase\":\"applied\",\"name\":%s,\"guid\":%s,\"subject\":%d,\"est\":%.6f,\"evalT\":%.6f,\"frame\":%llu,\"prevTickOption\":%d,\"framesSincePostNetInit\":%llu"),
				*FRun::Q(Info.Name), *FRun::Q(Info.Guid), (int32)Info.bSubject, Est, EvalT, (unsigned long long)GFrameCounter, (int32)Info.PreviousTickOption,
				(unsigned long long)(GFrameCounter - Info.PostNetInitFrame)));
		}

		/** Client: the first bone-transform finalisation after the apply shows the pawn (S10-R5). */
		void OnClientBonesFinalized(AStealthCharacter* Pawn)
		{
			FS10RClientPawn* Info = Pawn ? ClientPawns.Find(Pawn) : nullptr;
			if (!Info || !Info->bApplied || Info->bWarm)
			{
				return;
			}
			Info->bWarm = true;
			ShowAfterWarmup(*Info, TEXT("warm"));
		}

		void ShowAfterWarmup(FS10RClientPawn& Info, const TCHAR* Phase)
		{
			AStealthCharacter* Pawn = Info.Pawn.Get();
			if (!Pawn || Info.bShown)
			{
				return;
			}
			Info.bShown = true;
			if (USkeletalMeshComponent* Mesh = Pawn->GetMesh())
			{
				if (Info.FinalizeHandle.IsValid())
				{
					Mesh->UnregisterOnBoneTransformsFinalizedDelegate(Info.FinalizeHandle);
					Info.FinalizeHandle.Reset();
				}
				Mesh->VisibilityBasedAnimTickOption = Info.PreviousTickOption;
				Mesh->EnableExternalTickRateControl(false);
			}
			Pawn->SetActorHiddenInGame(false);
			ClientRun()->Write(TEXT("warm"), FString::Printf(TEXT("\"phase\":%s,\"name\":%s,\"guid\":%s,\"subject\":%d,\"frame\":%llu,\"framesSinceApply\":%llu"),
				*FRun::Q(Phase), *FRun::Q(Info.Name), *FRun::Q(Info.Guid), (int32)Info.bSubject, (unsigned long long)GFrameCounter, (unsigned long long)(GFrameCounter - Info.AppliedFrame)));
		}

		/** Client: OnActorChannelOpen on every open, including a re-bind of a live instance (E18). */
		void OnClientChannelOpen(AActor* Actor, int32 ChIndex, int32 PacketId, UNetConnection* Connection)
		{
			AStealthCharacter* Pawn = Cast<AStealthCharacter>(Actor);
			if (!Pawn || Pawn->GetNetMode() != NM_Client)
			{
				return;
			}
			EnsureClientWorld(Pawn->GetWorld());
			FS10RClientPawn& Info = ClientPawns.FindOrAdd(Pawn);
			Info.Pawn = Pawn;
			Info.Name = Pawn->GetName();
			++Info.ChannelOpenCount;
			Info.LastOpenChIndex = ChIndex;
			const UActorChannel* Channel = Connection && Connection->Channels.IsValidIndex(ChIndex) ? Cast<UActorChannel>(Connection->Channels[ChIndex]) : nullptr;
			Info.OpenNetGUID = Channel ? Channel->ActorNetGUID : FNetworkGUID();
			ClientRun()->Write(TEXT("chanOpen"), FString::Printf(TEXT("\"name\":%s,\"guid\":%s,\"ch\":%d,\"pkt\":%d,\"count\":%d"),
				*FRun::Q(Info.Name), *FRun::Q(Info.OpenNetGUID.ToString()), ChIndex, PacketId, Info.ChannelOpenCount));
		}

		/** Client: a network destroy (EndPlay Destroyed; clients never destroy replicated pawns themselves). */
		void OnClientActorDestroyed(AActor* Actor)
		{
			FS10RClientPawn* Info = Actor ? ClientPawns.Find(Actor) : nullptr;
			if (!Info)
			{
				return;
			}
			// Render client: the frame without the subject (the silhouette probe's background), armed for its re-open.
			// The stub is a replicated subobject the channel destroys before the actor, so use the flag taken at PostNetInit.
			const bool bSubjectInstance = Info->bSubject;
			if (bSubjectInstance && CaptureBudget > 0 && CaptureOwner == nullptr
				&& IsRenderClient() && (ArmedGuid.IsEmpty() || FPlatformTime::Seconds() - ArmedAt > 5.0) && HasClearView(Actor))
			{
				RequestCapture(*Info, TEXT("gone"), Actor);
				ArmedGuid = Info->Guid;
				ArmedAt = FPlatformTime::Seconds();
			}
			if (Info->ChannelOpenCount >= 2 && bProbe)
			{
				// Queue only; evaluated in the next client frame (never sent from inside channel cleanup).
				++LostProbeQueued;
				PendingProbes.Add({ Info->LastOpenChIndex, Info->OpenNetGUID, Info->Name });
				ClientRun()->Write(TEXT("lostProbeQueued"), FString::Printf(TEXT("\"name\":%s,\"guid\":%s,\"ch\":%d,\"count\":%d"),
					*FRun::Q(Info->Name), *FRun::Q(Info->OpenNetGUID.ToString()), Info->LastOpenChIndex, Info->ChannelOpenCount));
			}
			if (CaptureOwner == Actor)
			{
				CaptureOwner = nullptr;
			}
			ClientPawns.Remove(Actor);
		}

		void EvaluateProbes(UWorld* World)
		{
			if (PendingProbes.Num() == 0)
			{
				return;
			}
			UNetDriver* Driver = World->GetNetDriver();
			UNetConnection* Server = Driver ? Driver->ServerConnection : nullptr;
			for (const FPendingProbe& Probe : PendingProbes)
			{
				const UActorChannel* Channel = Server && Server->Channels.IsValidIndex(Probe.ChIndex) ? Cast<UActorChannel>(Server->Channels[Probe.ChIndex]) : nullptr;
				const bool bConnectionOk = Server && Server->GetConnectionState() == USOCK_Open && Server->Channels.Num() > 0 && Server->Channels[0] && !Server->Channels[0]->Closing;
				const bool bChannelOk = Channel && !Channel->Closing && Channel->ActorNetGUID == Probe.NetGUID;
				const TCHAR* Result = !bConnectionOk ? TEXT("connectionNotOpen") : (!Channel ? TEXT("slotEmpty") : (Channel->Closing ? TEXT("closing") : (!bChannelOk ? TEXT("otherGuid") : TEXT("reported"))));
				if (bConnectionOk && bChannelOk)
				{
					int32 ChIndex = Probe.ChIndex;
					FNetControlMessage<NMT_ActorChannelFailure>::Send(Server, ChIndex);
					++LostProbeReported;
				}
				else
				{
					++LostProbeNotReported;
				}
				ClientRun()->Write(TEXT("lostProbe"), FString::Printf(TEXT("\"name\":%s,\"guid\":%s,\"ch\":%d,\"result\":%s,\"broken\":%d,\"hasActor\":%d"),
					*FRun::Q(Probe.Name), *FRun::Q(Probe.NetGUID.ToString()), Probe.ChIndex, *FRun::Q(Result), Channel ? (int32)Channel->Broken : -1, Channel ? (int32)IsValid(Channel->Actor) : -1));
			}
			PendingProbes.Reset();
		}

		void EnsureClientWorld(UWorld* World)
		{
			if (!World || World == ClientWorld)
			{
				return;
			}
			ClientWorld = World;
			World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateLambda([this](AActor* Actor) { OnClientActorDestroyed(Actor); }));
		}

		static FRun* ClientRun()
		{
			static FRun* Fallback = nullptr;
			return FRun::Get() ? FRun::Get() : Fallback;
		}

		bool IsRenderClient() const
		{
			return !FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
		}

		/** Render client: the camera sees the subject's whole height (no wall between), so a silhouette can be measured. */
		bool HasClearView(const AActor* Actor) const
		{
			APlayerController* PC = ClientWorld ? ClientWorld->GetFirstPlayerController() : nullptr;
			if (!PC || !PC->PlayerCameraManager || !Actor)
			{
				return false;
			}
			const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
			FCollisionQueryParams Params(TEXT("S10RClearView"), /*bTraceComplex*/ false);
			Params.AddIgnoredActor(Actor);
			Params.AddIgnoredActor(PC->GetPawn());
			const AStealthCharacter* Pawn = Cast<AStealthCharacter>(Actor);
			const float Half = Pawn && Pawn->GetCapsuleComponent() ? Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 86.f;
			const FVector Feet = Actor->GetActorLocation() - FVector(0.0, 0.0, Half - 10.0);
			for (const double Up : { 0.0, 80.0, 160.0 })
			{
				FHitResult Hit;
				if (ClientWorld->LineTraceSingleByChannel(Hit, Eye, Feet + FVector(0.0, 0.0, Up), ECC_Visibility, Params) && !Cast<APawn>(Hit.GetActor()))
				{
					return false;
				}
			}
			return true;
		}

		void RequestCapture(const FS10RClientPawn& Info, const TCHAR* Stage, const AActor* Actor)
		{
			const FString File = FPaths::Combine(FRun::StringParam(TEXT("Out"), FPaths::ProjectSavedDir()), TEXT("frames"),
				FString::Printf(TEXT("s10r_%s_%s_%llu.png"), *Info.Guid, Stage, (unsigned long long)GFrameCounter));
			FScreenshotRequest::RequestScreenshot(File, /*bShowUI*/ false, /*bAddFilenameSuffix*/ false);
			FVector2D Foot(-1.0, -1.0), Head(-1.0, -1.0);
			if (APlayerController* PC = ClientWorld ? ClientWorld->GetFirstPlayerController() : nullptr; PC && Actor)
			{
				const AStealthCharacter* Pawn = Cast<AStealthCharacter>(Actor);
				const float Half = Pawn && Pawn->GetCapsuleComponent() ? Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 86.f;
				const FVector Feet = Actor->GetActorLocation() - FVector(0.0, 0.0, Half);
				PC->ProjectWorldLocationToScreen(Feet, Foot);
				PC->ProjectWorldLocationToScreen(Feet + FVector(0.0, 0.0, 170.0), Head);
			}
			ClientRun()->Write(TEXT("frameCapture"), FString::Printf(TEXT("\"file\":%s,\"name\":%s,\"guid\":%s,\"stage\":%s,\"frame\":%llu,\"foot\":[%.1f,%.1f],\"head\":[%.1f,%.1f]"),
				*FRun::Q(File), *FRun::Q(Info.Name), *FRun::Q(Info.Guid), *FRun::Q(Stage), (unsigned long long)GFrameCounter, Foot.X, Foot.Y, Head.X, Head.Y));
		}

		/** Head-bone height above the capsule bottom and head-to-root distance, from the component-space transforms. */
		static bool MeasurePose(const AStealthCharacter* Pawn, double& OutHeadHeight, double& OutHeadRoot)
		{
			const USkeletalMeshComponent* Mesh = Pawn ? Pawn->GetMesh() : nullptr;
			if (!Mesh)
			{
				return false;
			}
			const int32 Head = Mesh->GetBoneIndex(TEXT("head"));
			const TArray<FTransform>& Space = Mesh->GetComponentSpaceTransforms();
			if (Head == INDEX_NONE || !Space.IsValidIndex(Head) || Space.Num() == 0)
			{
				return false;
			}
			const FVector HeadWorld = Mesh->GetComponentTransform().TransformPosition(Space[Head].GetLocation());
			const float Half = Pawn->GetCapsuleComponent() ? Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 86.f;
			OutHeadHeight = HeadWorld.Z - (Pawn->GetActorLocation().Z - Half);
			OutHeadRoot = FVector::Dist(Space[Head].GetLocation(), Space[0].GetLocation());
			return true;
		}

		/** Client, after every tick group of the frame (what this frame renders): failsafe, probes, first visible frame. */
		void OnWorldPostActorTick(UWorld* World)
		{
			if (!World || World != ClientWorld || World->GetNetMode() != NM_Client)
			{
				return;
			}
			EvaluateProbes(World);
			// Clock-gated applies and the failsafe (frames counted from the apply, synced pawns only).
			for (TPair<AActor*, FS10RClientPawn>& It : ClientPawns)
			{
				FS10RClientPawn& Info = It.Value;
				if (!Info.Pawn.IsValid())
				{
					continue;
				}
				if (Info.bPostNetInit && !Info.bApplied)
				{
					TryApply(Info);
				}
				if (bWarmup && Info.bApplied && !Info.bShown && GFrameCounter - Info.AppliedFrame + 1 >= (uint64)WarmupMaxFrames)
				{
					++WarmupTimeouts;
					ensureMsgf(false, TEXT("S10R WarmupTimeout %s"), *Info.Name); // development builds: the ADR's ensure
					ShowAfterWarmup(Info, TEXT("warmupTimeout"));
				}
			}
			// The probe: the first frame each instance is visible, from what this frame renders (not the warm-up flag).
			for (TPair<AActor*, FS10RClientPawn>& It : ClientPawns)
			{
				FS10RClientPawn& Info = It.Value;
				AStealthCharacter* Pawn = Info.Pawn.Get();
				const USkeletalMeshComponent* Mesh = Pawn ? Pawn->GetMesh() : nullptr;
				if (!Pawn || !Mesh || !Info.bPostNetInit)
				{
					continue;
				}
				const bool bVisible = !Pawn->IsHidden() && Mesh->IsVisible();
				if (!bVisible)
				{
					continue;
				}
				++Info.VisibleFrames;
				double HeadHeight = -1.0, HeadRoot = -1.0;
				if (!Info.bFirstVisibleDone)
				{
					Info.bFirstVisibleDone = true;
					MeasurePose(Pawn, HeadHeight, HeadRoot);
					ClientRun()->Write(TEXT("firstVisible"), FString::Printf(TEXT("\"name\":%s,\"guid\":%s,\"subject\":%d,\"dying\":%d,\"warm\":%d,\"applied\":%d,\"frame\":%llu,\"framesSincePostNetInit\":%llu,\"headH\":%.2f,\"headRoot\":%.2f,\"tickOption\":%d,\"bones\":%d"),
						*FRun::Q(Info.Name), *FRun::Q(Info.Guid), (int32)Info.bSubject, (int32)Pawn->IsDying(), (int32)Info.bWarm, (int32)Info.bApplied, (unsigned long long)GFrameCounter,
						(unsigned long long)(GFrameCounter - Info.PostNetInitFrame), HeadHeight, HeadRoot, (int32)Mesh->VisibilityBasedAnimTickOption, Mesh->GetComponentSpaceTransforms().Num()));
					if (!ArmedGuid.IsEmpty() && Info.Guid == ArmedGuid && IsRenderClient() && CaptureBudget > 0 && CaptureOwner == nullptr)
					{
						ArmedGuid.Reset();
						--CaptureBudget;
						CaptureOwner = Pawn;
						Info.CaptureStage = 0;
						RequestCapture(Info, TEXT("f00"), Pawn);
					}
				}
				else if (CaptureOwner == Pawn && Info.CaptureStage >= 0)
				{
					++Info.CaptureStage;
					if (Info.CaptureStage == 1 || Info.CaptureStage == 5 || Info.CaptureStage == 10)
					{
						RequestCapture(Info, *FString::Printf(TEXT("f%02d"), Info.CaptureStage), Pawn);
					}
					if (Info.CaptureStage >= 10)
					{
						Info.CaptureStage = -1;
						CaptureOwner = nullptr;
					}
				}
				if (!Info.bRefDone && Info.VisibleFrames >= S10R::RefVisibleFrame)
				{
					Info.bRefDone = true;
					MeasurePose(Pawn, HeadHeight, HeadRoot);
					ClientRun()->Write(TEXT("poseRef"), FString::Printf(TEXT("\"name\":%s,\"guid\":%s,\"subject\":%d,\"dying\":%d,\"headH\":%.2f,\"headRoot\":%.2f"),
						*FRun::Q(Info.Name), *FRun::Q(Info.Guid), (int32)Info.bSubject, (int32)Pawn->IsDying(), HeadHeight, HeadRoot));
				}
			}
			// E10: a client channel still holding queued bunches destroys its actor later (KeepProcessingActorChannelBunchesMap).
			if (UNetConnection* Server = World->GetNetDriver() ? World->GetNetDriver()->ServerConnection : nullptr)
			{
				TSet<FNetworkGUID> Now;
				for (const TPair<FNetworkGUID, TArray<TObjectPtr<UActorChannel>>>& It : Server->KeepProcessingActorChannelBunchesMap)
				{
					Now.Add(It.Key);
				}
				for (const FNetworkGUID& Guid : Now)
				{
					if (!KeepProcessing.Contains(Guid))
					{
						ClientRun()->Write(TEXT("keepProcessing"), FString::Printf(TEXT("\"guid\":%s,\"on\":1"), *FRun::Q(Guid.ToString())));
					}
				}
				for (const FNetworkGUID& Guid : KeepProcessing)
				{
					if (!Now.Contains(Guid))
					{
						ClientRun()->Write(TEXT("keepProcessing"), FString::Printf(TEXT("\"guid\":%s,\"on\":0"), *FRun::Q(Guid.ToString())));
					}
				}
				KeepProcessing = MoveTemp(Now);
			}
		}

	public:
		virtual void TickClient(FRun& Run, UWorld* World, double ClientSeconds) override
		{
			EnsureClientWorld(World);
			// Render client: no HUD over the silhouette probe's frames (client-local).
			if (!bHudHidden && IsRenderClient())
			{
				if (APlayerController* PC = World->GetFirstPlayerController(); PC && PC->MyHUD)
				{
					PC->MyHUD->bShowHUD = false;
					bHudHidden = true;
				}
			}
			if (ClientSeconds - LastFaultSample >= 10.0)
			{
				LastFaultSample = ClientSeconds;
				int32 Pawns = 0;
				for (TActorIterator<AStealthCharacter> It(World); It; ++It)
				{
					++Pawns;
				}
				Run.Write(TEXT("faults"), FString::Printf(TEXT("%s,%s,\"pawns\":%d"), *Faults.Describe(), *CounterFields(), Pawns));
			}
		}

	private:
		struct FPendingProbe
		{
			int32 ChIndex = INDEX_NONE;
			FNetworkGUID NetGUID;
			FString Name;
		};

		FRun* RunPtr = nullptr;
		UWorld* WorldPtr = nullptr;
		UWorld* ClientWorld = nullptr;
		FS10RCullTickFunction CullTick;
		TArray<FS10RSubject> Subjects;
		TArray<FS10RRepairReport> RepairQueue;
		TMap<UNetConnection*, TArray<double>> RepairTimes;
		TMap<UNetConnection*, int32> DroppedConnections;
		TMap<AActor*, FS10RClientPawn> ClientPawns;
		TArray<FPendingProbe> PendingProbes;
		TSet<FNetworkGUID> KeepProcessing;
		const AActor* CaptureOwner = nullptr;
		FString ArmedGuid;
		double ArmedAt = 0.0;
		bool bHudHidden = false;
		FS10RFaultCounter Faults;
		FDelegateHandle EnsureHandle;
		FDelegateHandle TickStartHandle;
		FDelegateHandle PostActorTickHandle;
		double GridOrigin = 0.0;
		double CrowdAt = -1.0;
		double ActiveSince = -1.0;
		double SettleSeconds = FRun::IntParam(TEXT("SettleSeconds"), 35);
		bool bActiveWaitReported = false;
		double LastFaultSample = -100.0;
		double ReopenAckTimeoutSec = 0.35;
		int32 Duration = 640;
		int32 TotalCycles = 0;
		int32 WarmupMaxFrames = 3;
		int32 RepairMaxPerSec = 4;
		int32 InjectCloseDrops = 0;
		int32 AiSubjects = 2;
		int32 PlayerSubjects = 2;
		int32 CaptureBudget = 0;
		bool bGuard = true;
		bool bRepair = true;
		bool bProbe = true;
		bool bWarmup = true;
		// Counters (development-build counters of the ADR; harness records).
		int32 ReopenAckTimeouts = 0;
		int32 CloseNotSent = 0;
		int32 FailureReports = 0;
		int32 Repairs = 0;
		int32 RepairDeferrals = 0;
		int32 UnexpectedChannelFailure = 0;
		int32 InjectedDrops = 0;
		int32 ObservationCeilings = 0;
		int32 WarmupTimeouts = 0;
		int32 ClockGatedWaits = 0;
		int32 LostProbeQueued = 0;
		int32 LostProbeReported = 0;
		int32 LostProbeNotReported = 0;
	};

	void FS10RCullTickFunction::ExecuteTick(float, ELevelTick, ENamedThreads::Type, const FGraphEventRef&)
	{
		if (Owner)
		{
			Owner->ExecuteCullTick();
		}
	}

	static TUniquePtr<IScenario> MakeS10RScenario()
	{
		return MakeUnique<FS10RScenario>();
	}

	static FScenarioRegistrar GRegisterS10R(TEXT("S10R"), &MakeS10RScenario);
}
