#pragma once

// Shared support of the story asn-022 PIE tests (CulledVictimOpensFalling, LateActorSeeksNeverSnaps): the per-connection
// relevancy deny seam (Server/Testing/StealthRelevancyDenySeam.h) applied to a victim for one client, and two per-frame
// watchers on one engine clock (GFrameCounter, shared by every PIE world of the process):
//   FChannelWatch   server, after the world's tick flush (FWorldDelegates::OnWorldTickEnd, so a channel opened by this
//                   frame's replication is seen in this frame): whether the denied connection has an open actor channel
//                   for the victim, per frame;
//   FInstanceWatch  one client, after its actor ticks (FWorldDelegates::OnWorldPostActorTick: animation evaluated and bones
//                   finalised for the frame): every client instance of the victim (found by NetGUID: S10 Q2, a re-open
//                   re-uses it), whether it carried FDeathState when first seen, and per frame its visibility, the
//                   Death_Fall montage's blend weight and its root-relative component-space bone positions.
// Harness expectations from spike S10 (production/qa/spikes/2026-10-25_S10.md, 2026-10-26_S10-rerun.md): a re-open creates a
// new client actor under the same NetGUID; the prototype AnimBP renders one collapsed frame unless the pawn is warmed up
// hidden (S10-R5, now done by UEncounterPresentationSubsystem for a fresh actor carrying a death); a close must be
// acknowledged before the re-open (S10-R2), so every deny is held for at least DenySettleSec before anything re-opens it.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/Testing/StealthRelevancyDenySeam.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"
#include "View/EncounterPresentationSettings.h"

namespace StealthReopenPie
{
	/** The fixed frame rate of every PIE world during these tests (a blend of 0.1 s spans 6 frames). */
	constexpr float FixedFrameRate = 60.0f;
	/** A deny is held this long before the victim may re-open (far above a PIE round trip: the close is acknowledged). */
	constexpr double DenySettleSec = 0.6;
	/** Frames sampled per client instance (the pop check reads the first 10 visible ones). */
	constexpr int32 MaxSamplesPerInstance = 90;

	/** The victim's NetGUID on the server (invalid when the server has not assigned one). */
	inline FNetworkGUID GuidOf(const AActor* ServerActor)
	{
		const UWorld* World = ServerActor != nullptr ? ServerActor->GetWorld() : nullptr;
		const UNetDriver* Driver = World != nullptr ? World->GetNetDriver() : nullptr;
		return Driver != nullptr && Driver->GetNetGuidCache().IsValid() ? Driver->GetNetGuidCache()->GetNetGUID(ServerActor) : FNetworkGUID();
	}

	/** The live instance of Guid on a client world (null when it has none). */
	inline AStealthPawn* InstanceOf(UWorld* ClientWorld, const FNetworkGUID& Guid)
	{
		const UNetDriver* Driver = ClientWorld != nullptr ? ClientWorld->GetNetDriver() : nullptr;
		if (Driver == nullptr || !Driver->GetNetGuidCache().IsValid() || !Guid.IsValid())
		{
			return nullptr;
		}
		for (TActorIterator<AStealthPawn> It(ClientWorld); It; ++It)
		{
			if (IsValid(*It) && Driver->GetNetGuidCache()->GetNetGUID(*It) == Guid)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** The local player controller of a client world. */
	inline AStealthPlayerController* LocalOf(UWorld* World)
	{
		return World != nullptr ? Cast<AStealthPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	/** The Death_Fall montage the presentation plays. */
	inline UAnimMontage* FallMontage()
	{
		return UEncounterPresentationSettings::Get()->DeathFallMontage.LoadSynchronous();
	}

	/** True when Pawn renders on its machine (mesh visible flag, not hidden in game, actor not hidden). */
	inline bool IsShown(const AStealthPawn* Pawn)
	{
		const USkeletalMeshComponent* Mesh = Pawn != nullptr ? Pawn->GetMesh() : nullptr;
		return Mesh != nullptr && !Pawn->IsHidden() && Mesh->IsVisible();
	}

	/**
	 * Moves Striker aside from any earlier body and spawns an AI-driven NPC DistanceCm ahead of it, facing it. Tries a few
	 * spots (the factory refuses a spawn that overlaps level geometry or a pawn); null when every spot is blocked.
	 */
	inline AStealthPawn* SpawnNpcAheadAside(AStealthPawn* Striker, double DistanceCm)
	{
		UStealthPawnFactory* Factory = Striker != nullptr ? UStealthPawnFactory::Get(Striker->GetWorld()) : nullptr;
		if (Factory == nullptr)
		{
			return nullptr;
		}
		const FVector Origin = Striker->GetActorLocation();
		const FVector Forward = Striker->GetActorForwardVector().GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		const FVector Offsets[] = { Right * 600.0, -Right * 600.0, Right * 1200.0, -Right * 1200.0, -Forward * 600.0, Forward * 600.0, Right * 1800.0, -Right * 1800.0 };
		for (const FVector& Offset : Offsets)
		{
			if (!Striker->TeleportTo(Origin + Offset, Striker->GetActorRotation()))
			{
				continue;
			}
			FStealthSpawnRequest Request;
			Request.Transform = FTransform((-Forward).Rotation(), Striker->GetActorLocation() + Forward * DistanceCm);
			Request.ControlKind = EStealthPawnControlKind::Ai;
			if (AStealthPawn* Npc = Factory->SpawnSharedPawn(Request).Pawn)
			{
				return Npc;
			}
		}
		return nullptr;
	}

	/** Root-relative component-space bone positions (empty without a mesh). */
	inline TArray<FVector> Bones(const AStealthPawn* Pawn)
	{
		TArray<FVector> Out;
		const USkeletalMeshComponent* Mesh = Pawn != nullptr ? Pawn->GetMesh() : nullptr;
		if (Mesh == nullptr || Mesh->GetComponentSpaceTransforms().IsEmpty())
		{
			return Out;
		}
		const TArray<FTransform>& Space = Mesh->GetComponentSpaceTransforms();
		const FVector Root = Space[0].GetLocation();
		Out.Reserve(Space.Num());
		for (const FTransform& Bone : Space)
		{
			Out.Add(Bone.GetLocation() - Root);
		}
		return Out;
	}

	/** Largest per-bone distance between two poses of one mesh (0 when either is empty or they differ in size). */
	inline double MaxBoneDelta(const TArray<FVector>& A, const TArray<FVector>& B)
	{
		if (A.IsEmpty() || A.Num() != B.Num())
		{
			return 0.0;
		}
		double Max = 0.0;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			Max = FMath::Max(Max, FVector::Dist(A[Index], B[Index]));
		}
		return Max;
	}

	/** Whether the server holds an open (not closing) actor channel for Pawn on Viewer's connection. */
	inline bool HasOpenChannel(const AActor* Pawn, const APlayerController* Viewer)
	{
		UNetConnection* Connection = Viewer != nullptr ? Viewer->GetNetConnection() : nullptr;
		UActorChannel* Channel = Connection != nullptr && Pawn != nullptr ? Connection->FindActorChannelRef(TWeakObjectPtr<AActor>(const_cast<AActor*>(Pawn))) : nullptr;
		return Channel != nullptr && !Channel->Closing;
	}

	/** Server: per frame, whether the viewer's connection has an open channel for the pawn. */
	class FChannelWatch
	{
	public:
		~FChannelWatch() { Stop(); }

		void Start(AStealthPawn* InPawn, APlayerController* InViewer)
		{
			Stop();
			Pawn = InPawn;
			Viewer = InViewer;
			OpenFrames.Reset();
			ClosedFrames.Reset();
			Handle = FWorldDelegates::OnWorldTickEnd.AddLambda([this](UWorld* World, ELevelTick, float)
			{
				if (Pawn.IsValid() && World == Pawn->GetWorld())
				{
					(HasOpenChannel(Pawn.Get(), Viewer.Get()) ? OpenFrames : ClosedFrames).Add(GFrameCounter);
				}
			});
		}

		void Stop()
		{
			if (Handle.IsValid())
			{
				FWorldDelegates::OnWorldTickEnd.Remove(Handle);
				Handle.Reset();
			}
		}

		/** The first frame at or after From with an open channel (MAX_uint64 when none). */
		uint64 FirstOpenFrom(uint64 From) const
		{
			uint64 Best = MAX_uint64;
			for (const uint64 Frame : OpenFrames)
			{
				Best = Frame >= From ? FMath::Min(Best, Frame) : Best;
			}
			return Best;
		}

		/** Frames in [From, To) with an open channel. */
		int32 OpenBetween(uint64 From, uint64 To) const
		{
			return OpenFrames.FilterByPredicate([From, To](uint64 Frame) { return Frame >= From && Frame < To; }).Num();
		}

		TWeakObjectPtr<AStealthPawn> Pawn;
		TWeakObjectPtr<APlayerController> Viewer;
		TArray<uint64> OpenFrames;
		TArray<uint64> ClosedFrames;

	private:
		FDelegateHandle Handle;
	};

	/** One frame of one client instance. */
	struct FFrameSample
	{
		uint64 Frame = 0;
		double ServerNow = 0.0;
		bool bShown = false;
		bool bWarming = false;
		/** The Death_Fall montage instance's blend weight (-1 when it is not playing). */
		float FallWeight = -1.f;
		TArray<FVector> Bones;
	};

	/** One client instance of the victim. */
	struct FInstance
	{
		TWeakObjectPtr<AStealthPawn> Pawn;
		/** The instance pointer as the presentation log's key (never dereferenced). */
		const AStealthPawn* Key = nullptr;
		uint64 FirstFrame = 0;
		double FirstServerNow = 0.0;
		/** FDeathState was already set when the instance was first seen (it came in the initial block). */
		bool bDeathAtFirstSight = false;
		/** The frame the instance was last seen alive in the world. */
		uint64 LastFrame = 0;
		TArray<FFrameSample> Samples;

		/** The first sample shown (null when never shown). */
		const FFrameSample* FirstShown() const { return Samples.FindByPredicate([](const FFrameSample& S) { return S.bShown; }); }
		bool EverShown() const { return FirstShown() != nullptr; }
	};

	/** One client: every instance of the victim's NetGUID, sampled every frame. */
	class FInstanceWatch
	{
	public:
		~FInstanceWatch() { Stop(); }

		void Start(UWorld* InWorld, const FNetworkGUID& InGuid)
		{
			Stop();
			World = InWorld;
			Guid = InGuid;
			Instances.Reset();
			Montage = FallMontage();
			Handle = FWorldDelegates::OnWorldPostActorTick.AddLambda([this](UWorld* Ticked, ELevelTick, float)
			{
				if (Ticked == World.Get())
				{
					Sample();
				}
			});
		}

		void Stop()
		{
			if (Handle.IsValid())
			{
				FWorldDelegates::OnWorldPostActorTick.Remove(Handle);
				Handle.Reset();
			}
		}

		/** The instance live at Frame (null when none existed then). */
		const FInstance* LiveAt(uint64 Frame) const
		{
			return Instances.FindByPredicate([Frame](const FInstance& I) { return I.FirstFrame <= Frame && I.LastFrame >= Frame; });
		}

		/** Instances first seen in [From, To). */
		TArray<const FInstance*> CreatedBetween(uint64 From, uint64 To) const
		{
			TArray<const FInstance*> Out;
			for (const FInstance& I : Instances)
			{
				if (I.FirstFrame >= From && I.FirstFrame < To)
				{
					Out.Add(&I);
				}
			}
			return Out;
		}

		/** Frames in [From, To) at which an instance existed on the client. */
		int32 PresentFramesBetween(uint64 From, uint64 To) const
		{
			int32 Count = 0;
			for (const FInstance& I : Instances)
			{
				const uint64 Lo = FMath::Max(From, I.FirstFrame);
				const uint64 Hi = FMath::Min(To, I.LastFrame + 1);
				Count += Hi > Lo ? static_cast<int32>(Hi - Lo) : 0;
			}
			return Count;
		}

		TWeakObjectPtr<UWorld> World;
		FNetworkGUID Guid;
		TArray<FInstance> Instances;

	private:
		void Sample()
		{
			UWorld* Client = World.Get();
			AStealthPawn* Live = InstanceOf(Client, Guid);
			if (Live == nullptr)
			{
				return;
			}
			FInstance* Current = Instances.Num() > 0 && Instances.Last().Pawn.Get() == Live ? &Instances.Last() : nullptr;
			if (Current == nullptr)
			{
				Current = &Instances.AddDefaulted_GetRef();
				Current->Pawn = Live;
				Current->Key = Live;
				Current->FirstFrame = GFrameCounter;
				Current->FirstServerNow = StealthAssassinationPie::ServerNow(Client);
				Current->bDeathAtFirstSight = Live->HasDeathWrite();
			}
			Current->LastFrame = GFrameCounter;
			if (Current->Samples.Num() >= MaxSamplesPerInstance)
			{
				return;
			}
			const UEncounterPresentationSubsystem* Presentation = StealthPresentationPie::PresentationOf(Client);
			FFrameSample& S = Current->Samples.AddDefaulted_GetRef();
			S.Frame = GFrameCounter;
			S.ServerNow = StealthAssassinationPie::ServerNow(Client);
			S.bShown = IsShown(Live);
			S.bWarming = Presentation != nullptr && Presentation->IsWarmingUp(Live);
			USkeletalMeshComponent* Mesh = Live->GetMesh();
			const UAnimInstance* Anim = Mesh != nullptr ? Mesh->GetAnimInstance() : nullptr;
			if (const FAnimMontageInstance* Fall = Anim != nullptr && Montage.IsValid() ? Anim->GetActiveInstanceForMontage(Montage.Get()) : nullptr)
			{
				S.FallWeight = Fall->GetWeight();
			}
			S.Bones = Bones(Live);
			// Keep the bones of the sampled copy current on a machine that renders nothing (-nullrhi): after its warm-up, so
			// the warm-up's own setting and restore are what the first frames show.
			if (Mesh != nullptr && !S.bWarming && Mesh->VisibilityBasedAnimTickOption != EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones)
			{
				Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			}
		}

		TWeakObjectPtr<UAnimMontage> Montage;
		FDelegateHandle Handle;
	};

	/** Server: every frame, the server's own copy's bones (the clip's own motion reference). */
	class FServerBoneWatch
	{
	public:
		~FServerBoneWatch() { Stop(); }

		void Start(AStealthPawn* InPawn)
		{
			Stop();
			Pawn = InPawn;
			Samples.Reset();
			if (USkeletalMeshComponent* Mesh = InPawn != nullptr ? InPawn->GetMesh() : nullptr)
			{
				Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			}
			Handle = FWorldDelegates::OnWorldPostActorTick.AddLambda([this](UWorld* World, ELevelTick, float)
			{
				if (Pawn.IsValid() && World == Pawn->GetWorld())
				{
					FFrameSample& S = Samples.AddDefaulted_GetRef();
					S.Frame = GFrameCounter;
					S.ServerNow = StealthAssassinationPie::ServerNow(World);
					S.Bones = Bones(Pawn.Get());
				}
			});
		}

		void Stop()
		{
			if (Handle.IsValid())
			{
				FWorldDelegates::OnWorldPostActorTick.Remove(Handle);
				Handle.Reset();
			}
		}

		TWeakObjectPtr<AStealthPawn> Pawn;
		TArray<FFrameSample> Samples;

	private:
		FDelegateHandle Handle;
	};

	/** The first ClipStart row of Death_Fall for an instance key on a client (null when none). */
	inline const FEncounterPresentationLogRow* FirstFallRow(UWorld* World, const AStealthPawn* Key)
	{
		return StealthPresentationPie::FirstRow(World, Key, EEncounterPresentationEvent::ClipStart, EEncounterClip::DeathFall);
	}

	/** The first clip row (ClipStart of any clip) for an instance key on a client (null when none). */
	inline const FEncounterPresentationLogRow* FirstClipRow(UWorld* World, const AStealthPawn* Key)
	{
		return StealthPresentationPie::FirstRow(World, Key, EEncounterPresentationEvent::ClipStart);
	}

	/**
	 * The S10-R5 warm-up of a fresh instance: a WarmUpBegin row in its first frame, a WarmUpShown row (never a timeout), and
	 * no sampled frame shown before it. Under -nullrhi the prototype AnimBP's collapsed first frame (a render-client effect,
	 * S10 Q3b) does not occur, so the pose bound alone cannot see a missing warm-up; this checks the mechanism.
	 */
	inline bool WarmedUpBeforeShown(UWorld* World, const FInstance& Instance, FString& OutWhy)
	{
		const FEncounterPresentationLogRow* Begin = StealthPresentationPie::FirstRow(World, Instance.Key, EEncounterPresentationEvent::WarmUpBegin);
		const FEncounterPresentationLogRow* Shown = StealthPresentationPie::FirstRow(World, Instance.Key, EEncounterPresentationEvent::WarmUpShown);
		const FEncounterPresentationLogRow* Timeout = StealthPresentationPie::FirstRow(World, Instance.Key, EEncounterPresentationEvent::WarmUpTimeout);
		const FFrameSample* FirstShown = Instance.FirstShown();
		OutWhy = FString::Printf(TEXT("begin %s (frame %llu, instance %llu), shown %s (frame %llu), timeout %d, first shown sample %llu"),
			Begin ? TEXT("yes") : TEXT("no"), Begin ? Begin->Frame : 0ull, Instance.FirstFrame, Shown ? TEXT("yes") : TEXT("no"), Shown ? Shown->Frame : 0ull,
			Timeout ? 1 : 0, FirstShown ? FirstShown->Frame : 0ull);
		return Begin != nullptr && Begin->Frame <= Instance.FirstFrame && Shown != nullptr && Timeout == nullptr
			&& (FirstShown == nullptr || FirstShown->Frame >= Shown->Frame);
	}

	/** Frames from the first sample with a Death_Fall weight to the first with weight >= 1 (0 when it never ramps). */
	inline int32 RampFrames(const FInstance& Instance, float& OutFirstWeight)
	{
		OutFirstWeight = -1.f;
		uint64 Start = 0;
		for (const FFrameSample& S : Instance.Samples)
		{
			if (S.FallWeight < 0.f)
			{
				continue;
			}
			if (OutFirstWeight < 0.f)
			{
				OutFirstWeight = S.FallWeight;
				Start = S.Frame;
			}
			if (S.FallWeight >= 0.999f)
			{
				return static_cast<int32>(S.Frame - Start) + 1;
			}
		}
		return 0;
	}
}

#endif
