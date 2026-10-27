// Story asn-003: the non-replicated per-pawn defence component (ADR-0018 section 4.1). See Server/DefenceStateComponent.h.

#include "Server/DefenceStateComponent.h"

#include "GameFramework/Actor.h"

UDefenceStateComponent::UDefenceStateComponent()
{
	// Server-only state: never replicated, never ticks (the window and cooldown are queue items and stamps).
	SetIsReplicatedByDefault(false);
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UDefenceStateComponent* UDefenceStateComponent::Find(const AActor& Actor)
{
	return Actor.FindComponentByClass<UDefenceStateComponent>();
}

UDefenceStateComponent* UDefenceStateComponent::FindOrAdd(AActor& Actor)
{
	if (UDefenceStateComponent* Existing = Find(Actor))
	{
		return Existing;
	}
	if (!Actor.HasAuthority())
	{
		return nullptr;
	}
	UDefenceStateComponent* Created = NewObject<UDefenceStateComponent>(&Actor, TEXT("DefenceState"));
	Actor.AddInstanceComponent(Created);
	if (Actor.GetWorld() != nullptr)
	{
		Created->RegisterComponent();
	}
	return Created;
}
