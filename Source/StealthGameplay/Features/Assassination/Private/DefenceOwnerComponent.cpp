// Story asn-016 (production/epics/assassination-defence-reversal/story-016-defence-owner-cooldown-component.md): the
// owner-only cooldown end on the PlayerController (ADR-0018 section 1.5). See View/DefenceOwnerComponent.h.

#include "View/DefenceOwnerComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Server/DefenceStateComponent.h"

UDefenceOwnerComponent::UDefenceOwnerComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDefenceOwnerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = false;
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UDefenceOwnerComponent, CooldownEndServerTime, Params);
}

UDefenceOwnerComponent* UDefenceOwnerComponent::Find(const AController* Controller)
{
	return Controller != nullptr ? Controller->FindComponentByClass<UDefenceOwnerComponent>() : nullptr;
}

double UDefenceOwnerComponent::GetCooldownRemaining(double NowServerTime) const
{
	return CooldownEndServerTime >= 0.0 ? FMath::Max(CooldownEndServerTime - NowServerTime, 0.0) : 0.0;
}

bool UDefenceOwnerComponent::SetCooldownEndServerTime(double InCooldownEndServerTime)
{
	const AActor* Owner = GetOwner();
	if ((Owner != nullptr && !Owner->HasAuthority()) || CooldownEndServerTime == InCooldownEndServerTime)
	{
		return false;
	}
	CooldownEndServerTime = InCooldownEndServerTime;
	++WriteCount;
	return true;
}

void UDefenceOwnerComponent::SyncFromPawn(const APawn* Pawn, double CooldownSec)
{
	const UDefenceStateComponent* State = Pawn != nullptr ? UDefenceStateComponent::Find(*Pawn) : nullptr;
	const TOptional<double> End = State != nullptr ? State->GetCore().GetCooldownEndServerTime(CooldownSec) : TOptional<double>();
	SetCooldownEndServerTime(End.IsSet() ? End.GetValue() : -1.0);
}

void UDefenceOwnerComponent::AdoptForRecovery(AController* Controller, APawn* NewPawn, const APawn* LostPawn, double NowServerTime, double CooldownSec)
{
	if (NewPawn == nullptr)
	{
		return;
	}
	UDefenceStateComponent* NewState = UDefenceStateComponent::FindOrAdd(*NewPawn);
	const UDefenceStateComponent* LostState = LostPawn != nullptr ? UDefenceStateComponent::Find(*LostPawn) : nullptr;
	if (NewState != nullptr)
	{
		NewState->GetCore().AdoptForRecovery(LostState != nullptr ? &LostState->GetCore() : nullptr, NowServerTime);
	}
	if (UDefenceOwnerComponent* Owner = Find(Controller))
	{
		Owner->SyncFromPawn(NewPawn, CooldownSec);
	}
}
