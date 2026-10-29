// Story con-002: the local, non-replicated hide-volume set. See View/BushVolumeSet.h.

#include "View/BushVolumeSet.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "View/ConcealCollision.h"
#include "View/ConcealmentTypes.h"

ABushVolumeSet::ABushVolumeSet()
{
	bReplicates = false;
	SetReplicatingMovement(false);
	bNetLoadOnClient = false;
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ABushVolumeSet::BuildVolumes(TConstArrayView<FBushRuntime> Bushes)
{
	for (UBoxComponent* Old : Volumes)
	{
		if (Old != nullptr)
		{
			Old->DestroyComponent();
		}
	}
	Volumes.Reset();
	VolumeBushIds.Reset();
	for (const FBushRuntime& Bush : Bushes)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(this, *FString::Printf(TEXT("HideVolume_%u"), static_cast<uint32>(Bush.Id)));
		Box->SetupAttachment(RootComponent);
		Box->SetMobility(EComponentMobility::Static);
		const FVector Centre = Bush.HideVolume.GetCenter();
		Box->SetBoxExtent(Bush.HideVolume.GetExtent(), false);
		Box->SetWorldLocation(Centre);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ConcealmentCollision::BushVolumeChannel);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Box->SetGenerateOverlapEvents(false);
		Box->SetCanEverAffectNavigation(true);
		Box->bHiddenInGame = true;
		Box->RegisterComponent();
		Volumes.Add(Box);
		VolumeBushIds.Add(Bush.Id);
	}
}
