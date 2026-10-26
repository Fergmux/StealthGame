#include "View/StealthRosterTeamView.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

UTeamViewComponentBase::UTeamViewComponentBase()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

URosterTeamViewComponent::URosterTeamViewComponent() = default;

void URosterTeamViewComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ADR-0001: relevant only to the owning connection; push-based so an unchanged view costs nothing.
	FDoRepLifetimeParams Params;
	Params.Condition = COND_OwnerOnly;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(URosterTeamViewComponent, View, Params);
}

void URosterTeamViewComponent::ApplyView(const FTeamView& NewView)
{
	View = NewView;
	++ServerWriteCount;
	MARK_PROPERTY_DIRTY_FROM_NAME(URosterTeamViewComponent, View, this);
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	// The listen host's own view never replicates to it (no OnRep on the server): raise the owner event here, so the
	// host's HUD and command builder see their own view like a remote owner does (story iif-071).
	if (const APlayerController* Controller = Cast<APlayerController>(GetOwner()); Controller && Controller->IsLocalController())
	{
		OnViewUpdated.Broadcast(View);
	}
}

void URosterTeamViewComponent::OnRep_View()
{
	OnViewUpdated.Broadcast(View);
}
