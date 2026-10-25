// Scenario "S9": IVP spike S9, explicit channel close at the cull deadline and the channel stays closed
// (story production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md;
// ADR-0003 sections 5.3-5.5, E2/E5/E10/E15).
//
// THROWAWAY spike code in the rule shape the production cull will take: a per-pawn cull record
// {State (Visible, CullPending, Culled), CommitServerTime, CullDeadline, Reason}; the deadline is the first
// point of a global grid (multiples of 1 / PawnNetUpdateHz from the round's Active entry) at or after
// CommitServerTime + ConcealCullDelayMs; a tick function in TG_PostUpdateWork moves due records to Culled
// and closes every denied connection's channel (FindActorChannelRef -> Close(Relevancy)) in that frame;
// AStealthCharacter::IsNetRelevantFor is replaced entirely (spike hook) and keeps returning false for a
// denied viewer, resolved from RealViewer (the connection's PlayerController) through a server-side
// team cache; an unknown viewer is denied for a culled subject.
//
// Run: listen host + 3 clients under StealthLeak1 and StealthLeak2. Four subjects (three crowd pawns and
// C1's pawn) are committed in one server frame; L27 is staged both ways (a denied viewer's pawn standing
// on a culled pawn, and a culled pawn standing on a denied viewer's pawn); denied viewers are then parked
// next to culled pawns. Every frame for 60 s the server checks each (subject, denied connection) pair for
// a channel and for a ForceRelevantFrame request. Analyser: tools/ci/mp/analyze_s9.py.
#include "Scenarios/StealthHarnessRun.h"

#include "Server/Spikes/StealthSpikeHooks.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthNpcController.h"
#include "StealthPlayerState.h"
#include "StealthTestHarnessModule.h"

#include "Components/CapsuleComponent.h"
#include "Engine/ActorChannel.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/Level.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/NetworkObjectList.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace StealthHarness
{
	namespace S9
	{
		/** ADR-0003 section 1 / IVP: pawn update grid rate and the cull delay (IVP F3 default 250 ms). */
		constexpr double PawnNetUpdateHz = 15.0;
		constexpr double ConcealCullDelaySec = 0.250;
		/** Scenario steps, seconds after the crowd exists (commit) or after the cull deadline (the rest). */
		constexpr double PlaceAt = 1.0;
		constexpr double CommitAt = 4.0;
		constexpr double L27ViewerOnSubjectAt = 5.0;
		constexpr double L27SubjectOnViewerAt = 20.0;
		constexpr double ParkViewersAt = 35.0;
		constexpr double ObserveSeconds = 62.0;
		constexpr double BaseSamplePeriod = 0.25;
		/** How long each staged L27 base relation is held (re-asserted every server frame). */
		constexpr double ForcedBaseSeconds = 10.0;
		constexpr int32 CrowdNeeded = 6;
		constexpr double CrowdWaitCeiling = 30.0;
		constexpr uint8 NoTeam = 255;
	}

	enum class ECullState : uint8 { Visible, CullPending, Culled };

	/** Per-pawn cull record (the production rule shape, ADR-0003 section 5). */
	struct FCullRecord
	{
		TWeakObjectPtr<AStealthCharacter> Pawn;
		FString Guid;
		FString Label;
		ECullState State = ECullState::Visible;
		double CommitServerTime = 0.0;
		double CullDeadline = 0.0;
		FString Reason;
		uint8 SubjectTeam = S9::NoTeam;
		TArray<TWeakObjectPtr<UNetConnection>> Denied;
		TArray<TWeakObjectPtr<UNetConnection>> Allowed;
		TMap<FString, int32> FramesWithChannel;    // denied connection -> frames a channel existed after the cull
		TMap<FString, int32> AllowedFramesMissing; // allowed connection -> frames with no channel after the cull
		uint32 LastForceRelevantFrame = 0;
		int32 ForceRelevantRequests = 0;
		/** False only for the negative control: relevancy override alone, no explicit close (ADR-0003 E2). */
		bool bExplicitClose = true;
		/** Server time a denied connection was first seen without a channel after the cull. */
		TMap<FString, double> ChannelGoneAt;
	};

	class FS9Scenario;

	/** One staged L27 relation: Top stands on (is based on) Bottom until Until (server time). */
	struct FForcedBase
	{
		TWeakObjectPtr<APawn> Top;
		TWeakObjectPtr<APawn> Bottom;
		FString Label;
		double Until = 0.0;
	};

	/** TG_PostUpdateWork tick: after gameplay and movement, before the replication flush of this frame. */
	struct FS9CullTickFunction : public FTickFunction
	{
		FS9Scenario* Owner = nullptr;
		virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& CompletionGraphEvent) override;
		virtual FString DiagnosticMessage() override { return TEXT("StealthHarness S9 cull tick"); }
	};

	class FS9Scenario final : public IScenario
	{
	public:
		virtual void OnFinish(FRun&) override
		{
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
			WriteE15(Run, World);
			if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
			{
				GameMode->StartRound();
			}
			// The round's Active entry is the grid origin (ADR-0003 section 5.3).
			GridOrigin = World->GetTimeSeconds();
			Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"startRound\",\"gridOrigin\":%.6f,%s"), GridOrigin, *FRun::ServerTimeFields(World)));

			StealthSpikeHooks::SetRelevancyOverride([this](const AActor* Subject, const AActor* RealViewer, const AActor*) -> TOptional<bool>
			{
				return IsRelevant(Subject, RealViewer);
			});

			CullTick.Owner = this;
			CullTick.TickGroup = TG_PostUpdateWork;
			CullTick.bCanEverTick = true;
			CullTick.bTickEvenWhenPaused = true;
			CullTick.bAllowTickOnDedicatedServer = true;
			CullTick.RegisterTickFunction(World->PersistentLevel);
		}

		virtual bool TickServer(FRun& Run, UWorld* World, double) override
		{
			const double Now = World->GetTimeSeconds();
			if (CrowdAt < 0.0)
			{
				int32 Crowd = 0;
				for (TActorIterator<AStealthCharacter> It(World); It; ++It)
				{
					Crowd += Cast<AStealthNpcController>(It->GetController()) ? 1 : 0;
				}
				if (Crowd >= S9::CrowdNeeded || Now - GridOrigin > S9::CrowdWaitCeiling)
				{
					CrowdAt = Now;
					Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"crowd\",\"npcs\":%d,%s"), Crowd, *FRun::ServerTimeFields(World)));
				}
				return false;
			}
			const double T = Now - CrowdAt;
			if (Step == 0 && T >= S9::PlaceAt)
			{
				ChooseAndPlace(Run, World);
				++Step;
			}
			if (Step == 1 && T >= S9::CommitAt)
			{
				Commit(Run, World);
				++Step;
			}
			if (Step < 2)
			{
				return false;
			}
			HoldStagedNpcs();
			MonitorChannels(World);
			const double SinceDeadline = Now - FirstDeadline;
			if (Now - LastBaseSample >= S9::BaseSamplePeriod)
			{
				SampleBases(Run, World);
				LastBaseSample = Now;
			}
			if (Step == 2 && SinceDeadline >= S9::L27ViewerOnSubjectAt)
			{
				StandOn(Run, World, ViewerPawn(World, TEXT("C2")), Records.Num() > 0 ? Records[0].Pawn.Get() : nullptr, TEXT("L27a viewer C2 on culled subject"));
				++Step;
			}
			if (Step == 3 && SinceDeadline >= S9::L27SubjectOnViewerAt)
			{
				StandOn(Run, World, Records.Num() > 1 ? Records[1].Pawn.Get() : nullptr, ViewerPawn(World, TEXT("C3")), TEXT("L27b culled subject on viewer C3"));
				++Step;
			}
			if (Step == 4 && SinceDeadline >= S9::ParkViewersAt)
			{
				ParkViewers(Run, World);
				++Step;
			}
			if (SinceDeadline >= S9::ObserveSeconds)
			{
				WriteSummary(Run, World);
				return true;
			}
			return false;
		}

		/** The cull tick (TG_PostUpdateWork). */
		void ExecuteCullTick()
		{
			if (!WorldPtr || !RunPtr)
			{
				return;
			}
			const double Now = WorldPtr->GetTimeSeconds();
			HoldForcedBases(Now);
			for (FCullRecord& Record : Records)
			{
				AStealthCharacter* Pawn = Record.Pawn.Get();
				if (Record.State != ECullState::CullPending || Now < Record.CullDeadline || Pawn == nullptr)
				{
					continue;
				}
				Record.State = ECullState::Culled;
				for (const TWeakObjectPtr<UNetConnection>& Weak : Record.Denied)
				{
					UNetConnection* Connection = Weak.Get();
					if (!Connection)
					{
						continue;
					}
					UActorChannel* Channel = Connection->FindActorChannelRef(Pawn);
					const bool bHadChannel = Channel != nullptr;
					if (Channel && Record.bExplicitClose)
					{
						Channel->Close(EChannelCloseReason::Relevancy);
					}
					RunPtr->Write(TEXT("cullClose"), FString::Printf(
						TEXT("\"subject\":%s,\"label\":%s,\"explicitClose\":%d,\"conn\":%s,\"hadChannel\":%d,\"commit\":%.6f,\"deadline\":%.6f,\"lateBy\":%.6f,\"frameDelta\":%.6f,%s"),
						*FRun::Q(Record.Guid), *FRun::Q(Record.Label), (int32)Record.bExplicitClose, *FRun::Q(FRun::ConnectionName(Connection)), (int32)bHadChannel,
						Record.CommitServerTime, Record.CullDeadline, Now - Record.CullDeadline, WorldPtr->GetDeltaSeconds(), *FRun::ServerTimeFields(WorldPtr)));
				}
			}
		}

	private:
		/** Replaces APawn relevancy entirely: visible and pending pawns are relevant; a culled pawn only to allies. */
		TOptional<bool> IsRelevant(const AActor* Subject, const AActor* RealViewer) const
		{
			const FCullRecord* Record = Records.FindByPredicate([Subject](const FCullRecord& R) { return R.Pawn.Get() == Subject; });
			if (Record == nullptr || Record->State != ECullState::Culled)
			{
				return true;
			}
			const uint8* ViewerTeam = ViewerTeams.Find(RealViewer);
			if (ViewerTeam == nullptr)
			{
				return false; // unknown viewer for a culled subject: deny
			}
			return Record->SubjectTeam != S9::NoTeam && *ViewerTeam == Record->SubjectTeam;
		}

		static uint8 TeamOf(const APlayerController* PC)
		{
			const AStealthPlayerState* PS = PC ? PC->GetPlayerState<AStealthPlayerState>() : nullptr;
			return PS ? PS->Team : S9::NoTeam;
		}

		static APawn* ViewerPawn(UWorld* World, const TCHAR* Name)
		{
			const APlayerController* PC = FRun::FindPlayer(World, Name);
			return PC ? PC->GetPawn() : nullptr;
		}

		void WriteE15(FRun& Run, UWorld* World)
		{
			// ADR-0003 E15: NetClientTicksPerSecond >= MaxConnections x MaxFps (MaxFps = t.MaxFPS if set, else 240).
			const IConsoleVariable* MaxFpsVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
			const float CVarMaxFps = MaxFpsVar ? MaxFpsVar->GetFloat() : 0.f;
			const float MaxFps = CVarMaxFps > 0.f ? CVarMaxFps : 240.f;
			const AGameModeBase* GameMode = World->GetAuthGameMode();
			const int32 SessionMaxPlayers = GameMode && GameMode->GameSession ? GameMode->GameSession->MaxPlayers : 0;
			constexpr int32 DesignMaxConnections = 10; // ADR-0003 section 1 and the DefaultEngine.ini comment: 5 v 5
			const float Ticks = GEngine ? GEngine->NetClientTicksPerSecond : 0.f;
			const bool bHolds = Ticks >= DesignMaxConnections * MaxFps;
			const bool bHoldsSession = Ticks >= SessionMaxPlayers * MaxFps;
			Run.Write(TEXT("e15"), FString::Printf(TEXT("\"netClientTicksPerSecond\":%.1f,\"maxConnections\":%d,\"maxFps\":%.1f,\"holds\":%d,\"sessionMaxPlayers\":%d,\"holdsWithSessionMaxPlayers\":%d"),
				Ticks, DesignMaxConnections, MaxFps, (int32)bHolds, SessionMaxPlayers, (int32)bHoldsSession));
			UE_CLOGF(!bHolds, LogStealthHarness, Error, "E15 startup assertion failed: NetClientTicksPerSecond %.1f < %d x %.1f", Ticks, DesignMaxConnections, MaxFps);
		}

		void ChooseAndPlace(FRun& Run, UWorld* World)
		{
			ViewerTeams.Reset();
			UNetDriver* Driver = World->GetNetDriver();
			for (UNetConnection* Connection : Driver->ClientConnections)
			{
				if (Connection && Connection->PlayerController)
				{
					ViewerTeams.Add(Connection->PlayerController, TeamOf(Connection->PlayerController));
					Run.Write(TEXT("viewer"), FString::Printf(TEXT("\"conn\":%s,\"team\":%d"), *FRun::Q(FRun::ConnectionName(Connection)), (int32)TeamOf(Connection->PlayerController)));
				}
			}
			const APawn* C2 = ViewerPawn(World, TEXT("C2"));
			const APawn* C3 = ViewerPawn(World, TEXT("C3"));
			const FVector Anchor = C2 && C3 ? (C2->GetActorLocation() + C3->GetActorLocation()) * 0.5 : (C2 ? C2->GetActorLocation() : FVector::ZeroVector);
			TArray<AStealthCharacter*> Npcs;
			for (TActorIterator<AStealthCharacter> It(World); It; ++It)
			{
				if (Cast<AStealthNpcController>(It->GetController()) && !It->IsDying())
				{
					Npcs.Add(*It);
				}
			}
			Npcs.Sort([&Anchor](const AStealthCharacter& A, const AStealthCharacter& B)
			{
				return FVector::DistSquared(A.GetActorLocation(), Anchor) < FVector::DistSquared(B.GetActorLocation(), Anchor);
			});
			const int32 NpcSubjects = FMath::Min(3, Npcs.Num());
			for (int32 Index = 0; Index < NpcSubjects; ++Index)
			{
				FCullRecord& Record = Records.AddDefaulted_GetRef();
				Record.Pawn = Npcs[Index];
				Record.Label = FString::Printf(TEXT("npc%d"), Index + 1);
				Record.SubjectTeam = S9::NoTeam;
				Npcs[Index]->TeleportTo(Anchor + FVector(300.0 + 120.0 * Index, -200.0, 0.0), FRotator::ZeroRotator);
			}
			if (APlayerController* C1 = FRun::FindPlayer(World, TEXT("C1")); C1 && C1->GetPawn())
			{
				FCullRecord& Record = Records.AddDefaulted_GetRef();
				Record.Pawn = Cast<AStealthCharacter>(C1->GetPawn());
				Record.Label = TEXT("playerC1");
				Record.SubjectTeam = TeamOf(C1);
				C1->GetPawn()->TeleportTo(Anchor + FVector(300.0, 200.0, 0.0), FRotator::ZeroRotator);
			}
			if (Npcs.IsValidIndex(NpcSubjects))
			{
				// Negative control (ADR-0003 E2): the relevancy override alone, no explicit close. The monitor must
				// see this channel stay open until the engine's relevancy timeout closes it.
				FCullRecord& Record = Records.AddDefaulted_GetRef();
				Record.Pawn = Npcs[NpcSubjects];
				Record.Label = TEXT("controlOverrideOnly");
				Record.SubjectTeam = S9::NoTeam;
				Record.bExplicitClose = false;
				Npcs[NpcSubjects]->TeleportTo(Anchor + FVector(300.0, -400.0, 0.0), FRotator::ZeroRotator);
			}
			for (FCullRecord& Record : Records)
			{
				Record.Guid = FRun::Guid(World, Record.Pawn.Get());
				for (UNetConnection* Connection : Driver->ClientConnections)
				{
					if (!Connection || !Connection->PlayerController)
					{
						continue;
					}
					const bool bAlly = Record.SubjectTeam != S9::NoTeam && TeamOf(Connection->PlayerController) == Record.SubjectTeam;
					(bAlly ? Record.Allowed : Record.Denied).Add(Connection);
				}
				Run.Write(TEXT("subject"), FString::Printf(TEXT("\"guid\":%s,\"label\":%s,\"name\":%s,\"team\":%d,\"denied\":%d,\"allowed\":%d"),
					*FRun::Q(Record.Guid), *FRun::Q(Record.Label), *FRun::Q(GetNameSafe(Record.Pawn.Get())), (int32)Record.SubjectTeam, Record.Denied.Num(), Record.Allowed.Num()));
			}
		}

		void Commit(FRun& Run, UWorld* World)
		{
			// Every subject commits in this one server frame (the same-frame case).
			const double Now = World->GetTimeSeconds();
			const double Period = 1.0 / S9::PawnNetUpdateHz;
			const double Earliest = Now + S9::ConcealCullDelaySec;
			const double Deadline = GridOrigin + FMath::CeilToDouble((Earliest - GridOrigin) / Period - 1e-9) * Period;
			for (FCullRecord& Record : Records)
			{
				Record.State = ECullState::CullPending;
				Record.CommitServerTime = Now;
				Record.CullDeadline = Deadline;
				Record.Reason = TEXT("spike-commit");
			}
			FirstDeadline = Deadline;
			Run.Write(TEXT("cullCommit"), FString::Printf(TEXT("\"subjects\":%d,\"commit\":%.6f,\"deadline\":%.6f,\"gridOrigin\":%.6f,\"period\":%.6f,%s"),
				Records.Num(), Now, Deadline, GridOrigin, Period, *FRun::ServerTimeFields(World)));
		}

		/** Keeps the staged crowd pawns still (their AI would wander off a base). */
		void HoldStagedNpcs()
		{
			for (const TWeakObjectPtr<AStealthCharacter>& Weak : Held)
			{
				if (AStealthCharacter* Pawn = Weak.Get())
				{
					if (AController* Controller = Pawn->GetController())
					{
						Controller->StopMovement();
					}
				}
			}
		}

		void StandOn(FRun& Run, UWorld* World, APawn* Top, APawn* Bottom, const TCHAR* Label)
		{
			if (!Top || !Bottom)
			{
				Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"standOnFailed\",\"label\":%s"), *FRun::Q(Label)));
				return;
			}
			for (APawn* Pawn : { Top, Bottom })
			{
				if (AStealthCharacter* Character = Cast<AStealthCharacter>(Pawn); Character && Cast<AStealthNpcController>(Character->GetController()))
				{
					Held.AddUnique(Character);
				}
			}
			HoldStagedNpcs();
			// Stock characters slide off another character's capsule (ECB_No; also with ECB_Yes in the second run),
			// so the base relation is forced: Top is held above Bottom in MOVE_None and SetBase(Bottom's capsule)
			// is re-asserted every server frame for ForcedBaseSeconds (see HoldForcedBases).
			FForcedBase& Forced = ForcedBases.AddDefaulted_GetRef();
			Forced.Top = Top;
			Forced.Bottom = Bottom;
			Forced.Label = Label;
			Forced.Until = World->GetTimeSeconds() + S9::ForcedBaseSeconds;
			if (ACharacter* TopCharacter = Cast<ACharacter>(Top))
			{
				TopCharacter->GetCharacterMovement()->SetMovementMode(MOVE_None);
			}
			HoldForcedBases(World->GetTimeSeconds());
			Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"standOn\",\"label\":%s,\"top\":%s,\"bottom\":%s,%s"),
				*FRun::Q(Label), *FRun::Q(FRun::Guid(World, Top)), *FRun::Q(FRun::Guid(World, Bottom)), *FRun::ServerTimeFields(World)));
		}

		void HoldForcedBases(double Now)
		{
			for (FForcedBase& Forced : ForcedBases)
			{
				ACharacter* Top = Cast<ACharacter>(Forced.Top.Get());
				APawn* Bottom = Forced.Bottom.Get();
				if (!Top || !Bottom || Forced.Until <= 0.0)
				{
					continue;
				}
				if (Now >= Forced.Until)
				{
					Top->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
					Forced.Until = 0.0;
					if (RunPtr && WorldPtr)
					{
						RunPtr->Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"releaseBase\",\"label\":%s,%s"), *FRun::Q(Forced.Label), *FRun::ServerTimeFields(WorldPtr)));
					}
					continue;
				}
				UCapsuleComponent* Capsule = Bottom->FindComponentByClass<UCapsuleComponent>();
				const double Height = Capsule ? Capsule->GetScaledCapsuleHalfHeight() * 2.0 + 2.0 : 200.0;
				Top->SetActorLocation(Bottom->GetActorLocation() + FVector(0.0, 0.0, Height), false, nullptr, ETeleportType::TeleportPhysics);
				FMovementBaseInterfaceData BaseData = MovementBaseUtility::GetMovementBaseDataFromPhysicsOwner(Capsule);
				Top->SetBase(&BaseData);
			}
		}

		void ParkViewers(FRun& Run, UWorld* World)
		{
			int32 Index = 0;
			for (const TCHAR* Name : { TEXT("C2"), TEXT("C3") })
			{
				APawn* Viewer = ViewerPawn(World, Name);
				const FCullRecord* Record = Records.IsValidIndex(Records.Num() - 1 - Index) ? &Records[Records.Num() - 1 - Index] : nullptr;
				const AStealthCharacter* Subject = Record ? Record->Pawn.Get() : nullptr;
				if (Viewer && Subject)
				{
					Viewer->TeleportTo(Subject->GetActorLocation() + FVector(90.0, 0.0, 0.0), (-FVector::ForwardVector).Rotation());
				}
				++Index;
			}
			Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"parkViewers\",%s"), *FRun::ServerTimeFields(World)));
		}

		void SampleBases(FRun& Run, UWorld* World)
		{
			FString Pawns;
			auto Add = [&](const TCHAR* Label, const APawn* Pawn)
			{
				const ACharacter* Character = Cast<ACharacter>(Pawn);
				const UActorComponent* Base = Character ? Cast<UActorComponent>(Character->GetMovementBaseObject()) : nullptr;
				Pawns += FString::Printf(TEXT("%s{\"label\":%s,\"guid\":%s,\"base\":%s,\"loc\":%s}"), Pawns.IsEmpty() ? TEXT("") : TEXT(","),
					*FRun::Q(Label), *FRun::Q(FRun::Guid(World, Pawn)), *FRun::Q(Base && Base->GetOwner() ? FRun::Guid(World, Base->GetOwner()) : FString(TEXT("-"))),
					*FRun::Q(Pawn ? Pawn->GetActorLocation().ToCompactString() : FString()));
			};
			Add(TEXT("C2"), ViewerPawn(World, TEXT("C2")));
			Add(TEXT("C3"), ViewerPawn(World, TEXT("C3")));
			for (const FCullRecord& Record : Records)
			{
				Add(*Record.Label, Record.Pawn.Get());
			}
			FString Pairs;
			for (const FForcedBase& Forced : ForcedBases)
			{
				const APawn* Top = Forced.Top.Get();
				Pairs += FString::Printf(TEXT("%s{\"label\":%s,\"active\":%d,\"topBasedOnBottom\":%d}"), Pairs.IsEmpty() ? TEXT("") : TEXT(","),
					*FRun::Q(Forced.Label), (int32)(Forced.Until > 0.0), (int32)(Top && Top->IsBasedOnActor(Forced.Bottom.Get())));
			}
			Run.Write(TEXT("bases"), FString::Printf(TEXT("%s,\"pawns\":[%s],\"forced\":[%s]"), *FRun::ServerTimeFields(World), *Pawns, *Pairs));
		}

		void MonitorChannels(UWorld* World)
		{
			UNetDriver* Driver = World->GetNetDriver();
			for (FCullRecord& Record : Records)
			{
				AStealthCharacter* Pawn = Record.Pawn.Get();
				if (Record.State != ECullState::Culled || !Pawn)
				{
					continue;
				}
				for (const TWeakObjectPtr<UNetConnection>& Weak : Record.Denied)
				{
					UNetConnection* Connection = Weak.Get();
					if (!Connection)
					{
						continue;
					}
					const FString Name = FRun::ConnectionName(Connection);
					if (Connection->FindActorChannelRef(Pawn))
					{
						if (Record.FramesWithChannel.FindOrAdd(Name)++ == 0)
						{
							RunPtr->Write(TEXT("channelPresentAfterCull"), FString::Printf(TEXT("\"subject\":%s,\"label\":%s,\"conn\":%s,%s"),
								*FRun::Q(Record.Guid), *FRun::Q(Record.Label), *FRun::Q(Name), *FRun::ServerTimeFields(World)));
						}
						if (Record.ChannelGoneAt.Contains(Name))
						{
							RunPtr->Write(TEXT("channelReappeared"), FString::Printf(TEXT("\"subject\":%s,\"label\":%s,\"conn\":%s,%s"),
								*FRun::Q(Record.Guid), *FRun::Q(Record.Label), *FRun::Q(Name), *FRun::ServerTimeFields(World)));
							Record.ChannelGoneAt.Remove(Name);
						}
					}
					else if (!Record.ChannelGoneAt.Contains(Name))
					{
						Record.ChannelGoneAt.Add(Name, World->GetTimeSeconds());
						RunPtr->Write(TEXT("channelGone"), FString::Printf(TEXT("\"subject\":%s,\"label\":%s,\"conn\":%s,\"sinceDeadline\":%.6f,%s"),
							*FRun::Q(Record.Guid), *FRun::Q(Record.Label), *FRun::Q(Name), World->GetTimeSeconds() - Record.CullDeadline, *FRun::ServerTimeFields(World)));
					}
				}
				for (const TWeakObjectPtr<UNetConnection>& Weak : Record.Allowed)
				{
					if (UNetConnection* Connection = Weak.Get(); Connection && !Connection->FindActorChannelRef(Pawn))
					{
						Record.AllowedFramesMissing.FindOrAdd(FRun::ConnectionName(Connection))++;
					}
				}
				const TSharedPtr<FNetworkObjectInfo> Info = Driver->GetNetworkObjectList().Find(Pawn);
				if (Info.IsValid() && Info->ForceRelevantFrame != Record.LastForceRelevantFrame)
				{
					Record.LastForceRelevantFrame = Info->ForceRelevantFrame;
					++Record.ForceRelevantRequests;
					RunPtr->Write(TEXT("forceRelevant"), FString::Printf(TEXT("\"subject\":%s,\"frame\":%u,%s"),
						*FRun::Q(Record.Guid), Info->ForceRelevantFrame, *FRun::ServerTimeFields(World)));
				}
			}
			++MonitoredFrames;
		}

		void WriteSummary(FRun& Run, UWorld* World)
		{
			for (const FCullRecord& Record : Records)
			{
				FString Denied, Allowed;
				for (const TWeakObjectPtr<UNetConnection>& Weak : Record.Denied)
				{
					const FString Name = FRun::ConnectionName(Weak.Get());
					const int32* Frames = Record.FramesWithChannel.Find(Name);
					Denied += FString::Printf(TEXT("%s{\"conn\":%s,\"framesWithChannel\":%d}"), Denied.IsEmpty() ? TEXT("") : TEXT(","), *FRun::Q(Name), Frames ? *Frames : 0);
				}
				for (const TWeakObjectPtr<UNetConnection>& Weak : Record.Allowed)
				{
					const FString Name = FRun::ConnectionName(Weak.Get());
					const int32* Frames = Record.AllowedFramesMissing.Find(Name);
					Allowed += FString::Printf(TEXT("%s{\"conn\":%s,\"framesMissing\":%d}"), Allowed.IsEmpty() ? TEXT("") : TEXT(","), *FRun::Q(Name), Frames ? *Frames : 0);
				}
				Run.Write(TEXT("subjectSummary"), FString::Printf(TEXT("\"guid\":%s,\"label\":%s,\"explicitClose\":%d,\"state\":%d,\"forceRelevantRequests\":%d,\"monitoredFrames\":%d,\"denied\":[%s],\"allowed\":[%s],%s"),
					*FRun::Q(Record.Guid), *FRun::Q(Record.Label), (int32)Record.bExplicitClose, (int32)Record.State, Record.ForceRelevantRequests, MonitoredFrames, *Denied, *Allowed, *FRun::ServerTimeFields(World)));
			}
		}

		FRun* RunPtr = nullptr;
		UWorld* WorldPtr = nullptr;
		FS9CullTickFunction CullTick;
		TArray<FCullRecord> Records;
		TMap<const AActor*, uint8> ViewerTeams;
		TArray<TWeakObjectPtr<AStealthCharacter>> Held;
		TArray<FForcedBase> ForcedBases;
		double GridOrigin = 0.0;
		double CrowdAt = -1.0;
		double FirstDeadline = 0.0;
		double LastBaseSample = -1.0;
		int32 Step = 0;
		int32 MonitoredFrames = 0;
	};

	void FS9CullTickFunction::ExecuteTick(float, ELevelTick, ENamedThreads::Type, const FGraphEventRef&)
	{
		if (Owner)
		{
			Owner->ExecuteCullTick();
		}
	}

	static TUniquePtr<IScenario> MakeS9Scenario()
	{
		return MakeUnique<FS9Scenario>();
	}

	static FScenarioRegistrar GRegisterS9(TEXT("S9"), &MakeS9Scenario);
}
