// Story con-002: every machine builds the same bush runtime from the Manifest's BushSite records. See
// View/BushRuntimeSubsystem.h.

#include "View/BushRuntimeSubsystem.h"

#include "Engine/World.h"
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
