// Story con-002: every machine builds the same bush runtime from the Manifest's BushSite records. See
// View/BushRuntimeSubsystem.h.

#include "View/BushRuntimeSubsystem.h"

#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Sound/SoundBase.h"
#include "View/BushVolumeSet.h"

bool UBushRuntimeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UBushRuntimeSubsystem::Deinitialize()
{
	Clear();
	Super::Deinitialize();
}

UBushRuntimeSubsystem* UBushRuntimeSubsystem::Get(const UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<UBushRuntimeSubsystem>() : nullptr;
}

EBushSiteReject UBushRuntimeSubsystem::ValidateSites(TConstArrayView<FBushSiteRecord> Sites)
{
	TSet<uint16> Seen;
	for (const FBushSiteRecord& Site : Sites)
	{
		if ((Site.EntryAxisMask & ConcealmentGeometry::AllSidesMask) == 0)
		{
			return EBushSiteReject::EmptyEntryAxis; // V-BUSH-1: the whole layout is rejected
		}
		bool bDuplicate = false;
		Seen.Add(Site.BushId, &bDuplicate);
		if (Site.BushId == 0 || bDuplicate)
		{
			return EBushSiteReject::BadBushId;
		}
	}
	return EBushSiteReject::None;
}

bool UBushRuntimeSubsystem::BuildFromSites(TConstArrayView<FBushSiteRecord> Sites)
{
	Clear();
	LastReject = ValidateSites(Sites);
	if (LastReject != EBushSiteReject::None)
	{
		return false;
	}
	for (const FBushSiteRecord& Site : Sites)
	{
		Bushes.Add(ConcealmentGeometry::MakeBushRuntime(Site.BushId, Site.GroundCentre, Site.EntryAxisMask & ConcealmentGeometry::AllSidesMask));
	}
	Bushes.Sort([](const FBushRuntime& A, const FBushRuntime& B) { return A.Id < B.Id; });
	if (UWorld* World = GetWorld(); World != nullptr && Bushes.Num() > 0)
	{
		FActorSpawnParameters Params;
		Params.Name = MakeUniqueObjectName(World->PersistentLevel, ABushVolumeSet::StaticClass(), TEXT("BushVolumeSet"));
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		if (ABushVolumeSet* Set = World->SpawnActor<ABushVolumeSet>(ABushVolumeSet::StaticClass(), FTransform::Identity, Params))
		{
			Set->BuildVolumes(Bushes);
			VolumeSet = Set;
		}
	}
	bBuilt = true;
	++BuildCount;
	OnBuilt.Broadcast();
	return true;
}

void UBushRuntimeSubsystem::Clear()
{
	if (ABushVolumeSet* Set = VolumeSet.Get())
	{
		Set->Destroy();
	}
	VolumeSet.Reset();
	Bushes.Reset();
	RustleLog.Reset();
	bBuilt = false;
}

const FBushRuntime* UBushRuntimeSubsystem::Find(uint16 Id) const
{
	return Bushes.FindByPredicate([Id](const FBushRuntime& Bush) { return Bush.Id == Id; });
}

const FBushRuntime* UBushRuntimeSubsystem::FindEnterableBush(const FVector& Location, double ReachCm) const
{
	const FBushRuntime* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (const FBushRuntime& Bush : Bushes)
	{
		const double Distance = ConcealmentGeometry::DistanceToFootprintCm(Bush.Footprint, Location);
		const uint8 Side = ConcealmentGeometry::SideFromYaw(ConcealmentGeometry::YawFromAnchor(Bush, Location));
		if (Distance <= ReachCm && Bush.IsSideOpen(Side) && Distance < BestDistance)
		{
			Best = &Bush;
			BestDistance = Distance;
		}
	}
	return Best;
}

const FBushRuntime* UBushRuntimeSubsystem::FindBushAt(const FVector& Location) const
{
	for (const FBushRuntime& Bush : Bushes)
	{
		if (Bush.Footprint.IsInside(FVector2D(Location)))
		{
			return &Bush;
		}
	}
	return nullptr;
}

// ---- Rustle (story con-029) ----------------------------------------------------------------------------------------------

namespace BushRustleLocal
{
	/** The observation log's bound (halved when full). */
	constexpr int32 MaxLogRows = 1024;
}

int32 UBushRuntimeSubsystem::PushRustlePulse(uint16 BushId, uint8 Side, double StartServerTime, EBushRustleKind Kind, double NowServerTime)
{
	const UWorld* World = GetWorld();
	const FBushRuntime* Bush = Find(BushId);
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer || Bush == nullptr)
	{
		return INDEX_NONE;
	}
	FConcealmentTuning_Public Tuning; // the schema defaults until the TDC store feeds features (as every Concealment reader)
	const FBushRustlePulse Pulse = BushRustle::MakePulse(*Bush, ConcealmentGeometry::ValidateSide(Bush->EntryAxisMask, Side), Kind, StartServerTime, Tuning, RustleSound);
	if (NowServerTime > Pulse.OnsetServerTime + BushRustle::PulseLifeSec)
	{
		return INDEX_NONE; // long over: a late actor never replays a finished rustle
	}
	const int32 Slot = RustleRing.Write(Pulse);
	if (RustleLog.Num() >= BushRustleLocal::MaxLogRows)
	{
		RustleLog.RemoveAt(0, BushRustleLocal::MaxLogRows / 2);
	}
	RustleLog.Add(Pulse);
	if (RustleCollection != nullptr)
	{
		if (UMaterialParameterCollectionInstance* Instance = GetWorld()->GetParameterCollectionInstance(RustleCollection))
		{
			Instance->SetVectorParameterValue(*FString::Printf(TEXT("RustleA_%d"), Slot), FLinearColor(Pulse.WorldPos.X, Pulse.WorldPos.Y, Pulse.WorldPos.Z, Pulse.OnsetServerTime));
			Instance->SetVectorParameterValue(*FString::Printf(TEXT("RustleB_%d"), Slot), FLinearColor(Pulse.SideDir.X, Pulse.SideDir.Y, Pulse.AmplitudeCm, 0.0f));
		}
	}
	PlayRustleSound(Pulse, FMath::Max(NowServerTime - Pulse.OnsetServerTime, 0.0));
	return Slot;
}

void UBushRuntimeSubsystem::PlayRustleSound(const FBushRustlePulse& Pulse, double SeekSec)
{
	UWorld* World = GetWorld();
	if (World == nullptr || !World->bAllowAudioPlayback || World->GetAudioDeviceRaw() == nullptr)
	{
		return; // no audio on this machine (a -nullrhi test run): the pulse record is the observation
	}
	USoundBase* Sound = Cast<USoundBase>(Pulse.Sound.TryLoad());
	if (Sound == nullptr)
	{
		return;
	}
	const int32 Slot = (RustleRing.GetNextSlot() + BushRustle::SlotCount - 1) % BushRustle::SlotCount;
	if (RustlePool.Num() < BushRustle::SlotCount)
	{
		RustlePool.SetNum(BushRustle::SlotCount);
	}
	TObjectPtr<UAudioComponent>& Component = RustlePool[Slot];
	if (Component == nullptr || !IsValid(Component))
	{
		// Spatialised at the face; started at the onset, or seeked into the clip by a late write (never restarted).
		Component = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Pulse.WorldPos, FRotator::ZeroRotator, 1.0f, 1.0f,
			static_cast<float>(SeekSec), nullptr, nullptr, /*bAutoDestroy*/ false);
		if (Component == nullptr)
		{
			return;
		}
	}
	else
	{
		Component->Stop();
		Component->SetSound(Sound);
		Component->SetWorldLocation(Pulse.WorldPos);
		Component->Play(static_cast<float>(SeekSec));
	}
	++RustleSoundPlays;
}
