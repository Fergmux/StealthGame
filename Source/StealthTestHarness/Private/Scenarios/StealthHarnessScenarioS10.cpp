// Scenario "S10": IVP spikes S10 (story production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md)
// and S12 (story-006-spike-s12-cull-delay-vs-measured-l99.md) on the S9 cull rig (ADR-0003 sections 5.3-5.8, E10/E11).
//
// THROWAWAY spike code. The rig is S9's rule shape (StealthHarnessScenarioS9.cpp): a per-subject cull record, the
// deadline on the global 1/15 s grid at or after commit + ConcealCullDelayMs, an explicit Close(Relevancy) of every
// denied connection's channel in a TG_PostUpdateWork tick, and AStealthCharacter::IsNetRelevantFor replaced by the
// spike hook (a culled subject is relevant to nobody here: every subject is a crowd pawn, every viewer is denied).
// On top of it, each subject loops cull -> re-open cycles of three kinds:
//   A  un-cull in one server step (as HideExit / OnConcealmentBroken will do) after the subject has been culled;
//   C  the in-flight race: un-cull RaceFrames server frames after the close, long before the client can have
//      acknowledged it (RTT ~220 ms), so a second channel for the same NetGUID opens while the first is closing;
//   B  a death-write re-open: the culled subject is killed (KillCharacter) and un-culled in the same step.
// Subjects are held still and carry the S10 presentation stub (UStealthSpikeActionStateComponent) so the client can
// show what a re-created actor initialises from. Run with -StealthPacketTiming for the S12 packet-level latency.
// Analyser: tools/ci/mp/analyze_s10.py (and analyze_net.py for S12).
#include "Scenarios/StealthHarnessRun.h"

#include "Server/Spikes/StealthSpikeHooks.h"
#include "Spikes/Visibility/StealthSpikeActionStateComponent.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthNpcController.h"
#include "StealthTestHarnessModule.h"

#include "Engine/ActorChannel.h"
#include "Engine/Channel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "Templates/Atomic.h"
#include "Misc/Parse.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

namespace StealthHarness
{
	namespace S10
	{
		constexpr double PawnNetUpdateHz = 15.0;
		constexpr double ConcealCullDelaySec = 0.250;
		constexpr double HoldCulledSec = 1.2;
		constexpr double HoldVisibleSec = 1.5;
		constexpr int32 RaceFrames = 2;
		constexpr int32 SubjectCount = 3;
		constexpr double CrowdWaitCeiling = 40.0;
	}

	/** Counts ensures and error/warning log lines by category (client and server fault accounting). */
	class FS10FaultCounter final : public FOutputDevice
	{
	public:
		TMap<FName, int32> Errors;
		TMap<FName, int32> Warnings;
		TAtomic<int32> Ensures{ 0 };
		mutable FCriticalSection Mutex;

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			FScopeLock Lock(&Mutex); // log lines can arrive from any thread
			if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Fatal)
			{
				Errors.FindOrAdd(Category)++;
			}
			else if (Verbosity == ELogVerbosity::Warning)
			{
				Warnings.FindOrAdd(Category)++;
			}
		}

		virtual bool CanBeUsedOnAnyThread() const override { return true; }

		FString Describe() const
		{
			FScopeLock Lock(&Mutex);
			FString Out = FString::Printf(TEXT("\"ensures\":%d,\"errors\":{"), Ensures.Load());
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

	enum class ES10State : uint8 { Visible, CullPending, Culled };
	enum class ES10Case : uint8 { A, B, C };

	struct FS10Subject
	{
		TWeakObjectPtr<AStealthCharacter> Pawn;
		FString Guid;
		FVector HoldAt = FVector::ZeroVector;
		ES10State State = ES10State::Visible;
		double NextCommitAt = 0.0;
		double CullDeadline = 0.0;
		double CulledAt = 0.0;
		uint64 ClosedFrame = 0;
		int32 Cycle = 0;
		ES10Case Case = ES10Case::A;
	};

	class FS10Scenario;

	struct FS10CullTickFunction : public FTickFunction
	{
		FS10Scenario* Owner = nullptr;
		virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& CompletionGraphEvent) override;
		virtual FString DiagnosticMessage() override { return TEXT("StealthHarness S10 cull tick"); }
	};

	class FS10Scenario final : public IScenario
	{
	public:
		FS10Scenario()
		{
			GLog->AddOutputDevice(&Faults);
			EnsureHandle = FCoreDelegates::OnHandleSystemEnsure.AddLambda([this]() { ++Faults.Ensures; });
		}

		virtual ~FS10Scenario() override
		{
			GLog->RemoveOutputDevice(&Faults);
			FCoreDelegates::OnHandleSystemEnsure.Remove(EnsureHandle);
		}

		virtual void OnFinish(FRun& Run) override
		{
			Run.Write(TEXT("faults"), Faults.Describe());
			if (CullTick.IsTickFunctionRegistered())
			{
				CullTick.UnRegisterTickFunction();
			}
			StealthSpikeHooks::SetRelevancyOverride({});
			WorldPtr = nullptr;
		}

		virtual void OnAllJoined(FRun& Run, UWorld* World) override
		{
			RunPtr = &Run;
			WorldPtr = World;
			Duration = FRun::IntParam(TEXT("Duration"), 660);
			if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
			{
				GameMode->StartRound();
			}
			GridOrigin = World->GetTimeSeconds();
			Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"startRound\",\"gridOrigin\":%.6f,\"duration\":%d,%s"), GridOrigin, Duration, *FRun::ServerTimeFields(World)));
			bRaceGuard = FRun::IntParam(TEXT("RaceGuard"), 0) != 0;
			Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"config\",\"raceGuard\":%d"), (int32)bRaceGuard));
			StealthSpikeHooks::SetRelevancyOverride([this](const AActor* Subject, const AActor* RealViewer, const AActor*) -> TOptional<bool>
			{
				const FS10Subject* Record = Subjects.FindByPredicate([Subject](const FS10Subject& S) { return S.Pawn.Get() == Subject; });
				if (Record && Record->State == ES10State::Culled)
				{
					return false; // every viewer is denied a culled crowd pawn
				}
				return !(Record && bRaceGuard && IsCloseUnacknowledged(Subject, RealViewer));
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
				int32 Crowd = 0;
				for (TActorIterator<AStealthCharacter> It(World); It; ++It)
				{
					Crowd += Cast<AStealthNpcController>(It->GetController()) ? 1 : 0;
				}
				if (Crowd >= S10::SubjectCount + 2 || Now - GridOrigin > S10::CrowdWaitCeiling)
				{
					CrowdAt = Now;
					Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"crowd\",\"npcs\":%d,%s"), Crowd, *FRun::ServerTimeFields(World)));
					for (int32 Index = 0; Index < S10::SubjectCount; ++Index)
					{
						Subjects.AddDefaulted();
						ReplaceSubject(Run, World, Index, Now + 1.0 + 1.1 * Index);
					}
				}
				return false;
			}
			for (int32 Index = 0; Index < Subjects.Num(); ++Index)
			{
				FS10Subject& Subject = Subjects[Index];
				AStealthCharacter* Pawn = Subject.Pawn.Get();
				if (!Pawn || Pawn->IsDying())
				{
					if (Subject.State == ES10State::Visible && Now >= Subject.NextCommitAt)
					{
						ReplaceSubject(Run, World, Index, Now + 0.5); // a killed subject is replaced by a fresh crowd pawn
					}
					continue;
				}
				Hold(Subject);
				if (Subject.State == ES10State::Visible && Now >= Subject.NextCommitAt)
				{
					Commit(Run, World, Subject);
				}
				else if (Subject.State == ES10State::Culled && Subject.Case != ES10Case::C && Now - Subject.CulledAt >= S10::HoldCulledSec)
				{
					Uncull(Run, World, Subject, Index);
				}
			}
			return false;
		}

		/** TG_PostUpdateWork: due culls close their channels; race un-culls run RaceFrames frames after the close. */
		void ExecuteCullTick()
		{
			if (!WorldPtr || !RunPtr)
			{
				return;
			}
			const double Now = WorldPtr->GetTimeSeconds();
			UNetDriver* Driver = WorldPtr->GetNetDriver();
			for (int32 Index = 0; Index < Subjects.Num(); ++Index)
			{
				FS10Subject& Subject = Subjects[Index];
				AStealthCharacter* Pawn = Subject.Pawn.Get();
				if (!Pawn || !Driver)
				{
					continue;
				}
				if (Subject.State == ES10State::CullPending && Now >= Subject.CullDeadline)
				{
					Subject.State = ES10State::Culled;
					Subject.CulledAt = Now;
					Subject.ClosedFrame = GFrameCounter;
					int32 Closed = 0;
					for (UNetConnection* Connection : Driver->ClientConnections)
					{
						if (UActorChannel* Channel = Connection ? Connection->FindActorChannelRef(Pawn) : nullptr)
						{
							Channel->Close(EChannelCloseReason::Relevancy);
							ClosingChannels.Add(TPair<const AActor*, const UNetConnection*>(Pawn, Connection), Channel);
							++Closed;
						}
					}
					RunPtr->Write(TEXT("cullClose"), FString::Printf(TEXT("\"subject\":%s,\"cycle\":%d,\"case\":%d,\"closed\":%d,\"deadline\":%.6f,\"lateBy\":%.6f,%s"),
						*FRun::Q(Subject.Guid), Subject.Cycle, (int32)Subject.Case, Closed, Subject.CullDeadline, Now - Subject.CullDeadline, *FRun::ServerTimeFields(WorldPtr)));
				}
				else if (Subject.State == ES10State::Culled && Subject.Case == ES10Case::C && GFrameCounter >= Subject.ClosedFrame + S10::RaceFrames)
				{
					Uncull(*RunPtr, WorldPtr, Subject, Index);
				}
			}
		}

	private:
		/**
		 * Race guard (the candidate ADR-0003 fix measured by this spike): a re-open is held back for a connection while the
		 * channel closed for that subject is still in the connection's OpenChannels, i.e. until the client has
		 * acknowledged the close. Without it, an open processed before a delayed (lost and resent) close binds the
		 * existing client actor, which the late close then destroys (E10 in-flight race).
		 */
		bool IsCloseUnacknowledged(const AActor* Subject, const AActor* RealViewer)
		{
			const APlayerController* PC = Cast<APlayerController>(RealViewer);
			const UNetConnection* Connection = PC ? PC->GetNetConnection() : nullptr;
			const TPair<const AActor*, const UNetConnection*> Key(Subject, Connection);
			const TWeakObjectPtr<UChannel>* Closing = ClosingChannels.Find(Key);
			if (!Closing)
			{
				return false;
			}
			if (Closing->IsValid() && Connection && Connection->OpenChannels.Contains(Closing->Get()))
			{
				if (!HeldSince.Contains(Key) && WorldPtr)
				{
					HeldSince.Add(Key, WorldPtr->GetTimeSeconds());
				}
				return true;
			}
			ClosingChannels.Remove(Key);
			if (const double* Since = HeldSince.Find(Key); Since && RunPtr && WorldPtr)
			{
				RunPtr->Write(TEXT("raceGuardRelease"), FString::Printf(TEXT("\"subject\":%s,\"conn\":%s,\"heldMs\":%.3f,%s"),
					*FRun::Q(FRun::Guid(WorldPtr, Subject)), *FRun::Q(FRun::ConnectionName(Connection)), (WorldPtr->GetTimeSeconds() - *Since) * 1000.0, *FRun::ServerTimeFields(WorldPtr)));
				HeldSince.Remove(Key);
			}
			return false;
		}

		void ReplaceSubject(FRun& Run, UWorld* World, int32 Index, double FirstCommitAt)
		{
			// The nearest live crowd pawn that is not already a subject, placed in front of C3 (the render client).
			APlayerController* Viewer = FRun::FindPlayer(World, TEXT("C3"));
			APawn* ViewerPawn = Viewer ? Viewer->GetPawn() : nullptr;
			const FVector Eye = ViewerPawn ? ViewerPawn->GetActorLocation() : FVector::ZeroVector;
			const FVector Forward = ViewerPawn ? ViewerPawn->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
			AStealthCharacter* Best = nullptr;
			for (TActorIterator<AStealthCharacter> It(World); It; ++It)
			{
				const bool bTaken = Subjects.ContainsByPredicate([&It](const FS10Subject& S) { return S.Pawn.Get() == *It; });
				if (!bTaken && !It->IsDying() && Cast<AStealthNpcController>(It->GetController()) && (!Best
					|| FVector::DistSquared(It->GetActorLocation(), Eye) < FVector::DistSquared(Best->GetActorLocation(), Eye)))
				{
					Best = *It;
				}
			}
			FS10Subject& Subject = Subjects[Index];
			Subject = FS10Subject();
			Subject.NextCommitAt = FirstCommitAt;
			if (!Best)
			{
				Run.Write(TEXT("event"), TEXT("\"ev\":\"noSubject\""));
				return;
			}
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
			Subject.HoldAt = Eye + Forward * 650.0 + Right * (Index - 1) * 260.0; // in view of C3's third-person camera, none behind its pawn
			Best->TeleportTo(Subject.HoldAt, (-Forward).Rotation());
			Subject.HoldAt = Best->GetActorLocation();
			Subject.Pawn = Best;
			Subject.Guid = FRun::Guid(World, Best);
			UStealthSpikeActionStateComponent* Stub = NewObject<UStealthSpikeActionStateComponent>(Best, TEXT("SpikeActionState"));
			Stub->RegisterComponent();
			Best->AddInstanceComponent(Stub);
			Stub->State.StartServerTime = World->GetTimeSeconds();
			if (Viewer && ViewerPawn)
			{
				Viewer->ClientSetRotation(Forward.Rotation());
			}
			Run.Write(TEXT("subject"), FString::Printf(TEXT("\"index\":%d,\"guid\":%s,\"name\":%s,\"hold\":[%.3f,%.3f,%.3f],%s"),
				Index, *FRun::Q(Subject.Guid), *FRun::Q(Best->GetName()), Subject.HoldAt.X, Subject.HoldAt.Y, Subject.HoldAt.Z, *FRun::ServerTimeFields(World)));
		}

		void Hold(FS10Subject& Subject)
		{
			AStealthCharacter* Pawn = Subject.Pawn.Get();
			if (AController* Controller = Pawn ? Pawn->GetController() : nullptr)
			{
				Controller->StopMovement();
			}
		}

		void Commit(FRun& Run, UWorld* World, FS10Subject& Subject)
		{
			const double Now = World->GetTimeSeconds();
			const double Period = 1.0 / S10::PawnNetUpdateHz;
			const double Earliest = Now + S10::ConcealCullDelaySec;
			Subject.CullDeadline = GridOrigin + FMath::CeilToDouble((Earliest - GridOrigin) / Period - 1e-9) * Period;
			Subject.State = ES10State::CullPending;
			// B consumes the subject (a kill), so it is every 20th cycle: the crowd lasts the whole run (NpcCount 30).
			Subject.Case = (Subject.Cycle % 20 == 19) ? ES10Case::B : (Subject.Cycle % 2 == 0 ? ES10Case::A : ES10Case::C);
			Run.Write(TEXT("cullCommit"), FString::Printf(TEXT("\"subject\":%s,\"cycle\":%d,\"case\":%d,\"commit\":%.6f,\"deadline\":%.6f,%s"),
				*FRun::Q(Subject.Guid), Subject.Cycle, (int32)Subject.Case, Now, Subject.CullDeadline, *FRun::ServerTimeFields(World)));
		}

		void Uncull(FRun& Run, UWorld* World, FS10Subject& Subject, int32 Index)
		{
			AStealthCharacter* Pawn = Subject.Pawn.Get();
			const double Now = World->GetTimeSeconds();
			if (Subject.Case == ES10Case::B && Pawn)
			{
				// Death write and re-open in one server step (ADR-0003: a killed culled victim re-opens only at the death write).
				if (UStealthSpikeActionStateComponent* Stub = Pawn->FindComponentByClass<UStealthSpikeActionStateComponent>())
				{
					Stub->State.DeathServerTime = Now;
				}
				Pawn->KillCharacter();
			}
			Subject.State = ES10State::Visible; // the channel re-opens on the subject's own update schedule (no forced update or relevancy)
			const FVector Location = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
			Run.Write(TEXT("uncull"), FString::Printf(TEXT("\"subject\":%s,\"index\":%d,\"cycle\":%d,\"case\":%d,\"sinceClose\":%.6f,\"framesSinceClose\":%llu,\"guid\":%s,\"loc\":[%.3f,%.3f,%.3f],%s"),
				*FRun::Q(Subject.Guid), Index, Subject.Cycle, (int32)Subject.Case, Now - Subject.CulledAt, (unsigned long long)(GFrameCounter - Subject.ClosedFrame),
				*FRun::Q(FRun::Guid(World, Pawn)), Location.X, Location.Y, Location.Z, *FRun::ServerTimeFields(World)));
			++Subject.Cycle;
			++TotalCycles;
			Subject.NextCommitAt = Now + S10::HoldVisibleSec;
		}

	public:
		virtual void TickClient(FRun& Run, UWorld* World, double ClientSeconds) override
		{
			if (!bClientConfigured)
			{
				bClientConfigured = true;
				if (FRun::IntParam(TEXT("CaptureReopens"), 0) > 0 && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
				{
					UStealthSpikeActionStateComponent::CaptureFrames() = { 0, 1, 2, 5 };
					UStealthSpikeActionStateComponent::CaptureBudget() = FRun::IntParam(TEXT("CaptureReopens"), 0);
				}
			}
			// Client fault accounting every 10 s: ensures, error/warning lines by category, pawn census.
			if (ClientSeconds - LastFaultSample >= 10.0)
			{
				LastFaultSample = ClientSeconds;
				int32 Pawns = 0;
				for (TActorIterator<AStealthCharacter> It(World); It; ++It)
				{
					++Pawns;
				}
				Run.Write(TEXT("faults"), FString::Printf(TEXT("%s,\"pawns\":%d"), *Faults.Describe(), Pawns));
			}
		}

	private:
		FRun* RunPtr = nullptr;
		UWorld* WorldPtr = nullptr;
		FS10CullTickFunction CullTick;
		TArray<FS10Subject> Subjects;
		FS10FaultCounter Faults;
		FDelegateHandle EnsureHandle;
		double GridOrigin = 0.0;
		double CrowdAt = -1.0;
		double LastFaultSample = -100.0;
		int32 Duration = 660;
		int32 TotalCycles = 0;
		bool bClientConfigured = false;
		bool bRaceGuard = false;
		TMap<TPair<const AActor*, const UNetConnection*>, TWeakObjectPtr<UChannel>> ClosingChannels;
		TMap<TPair<const AActor*, const UNetConnection*>, double> HeldSince;
	};

	void FS10CullTickFunction::ExecuteTick(float, ELevelTick, ENamedThreads::Type, const FGraphEventRef&)
	{
		if (Owner)
		{
			Owner->ExecuteCullTick();
		}
	}

	static TUniquePtr<IScenario> MakeS10Scenario()
	{
		return MakeUnique<FS10Scenario>();
	}

	static FScenarioRegistrar GRegisterS10(TEXT("S10"), &MakeS10Scenario);
}
