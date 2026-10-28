// Objectives Chain story oc-011 (production/epics/objectives-chain/story-011-team-view-component-and-replication.md): the
// Objectives part of C-TeamView. See View/ObjectiveTeamViewComponent.h.

#include "View/ObjectiveTeamViewComponent.h"

#include "GameFramework/PlayerController.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

UObjectiveTeamViewComponent::UObjectiveTeamViewComponent() = default;

void UObjectiveTeamViewComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ADR-0020 section 6: one push-based property; COND_None because the controller is relevant only to its owner (E1).
	FDoRepLifetimeParams Params;
	Params.Condition = COND_None;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UObjectiveTeamViewComponent, View, Params);
}

bool UObjectiveTeamViewComponent::ApplyView(const FObjectiveTeamView& NewView)
{
	if (View == NewView)
	{
		return false; // change-only: an identical write costs nothing
	}
	View = NewView;
	++ServerWriteCount;
	MARK_PROPERTY_DIRTY_FROM_NAME(UObjectiveTeamViewComponent, View, this);
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	// The listen host's own view never replicates to it (no OnRep on the server): raise the owner event here (host parity).
	if (const APlayerController* Controller = Cast<APlayerController>(GetOwner()); Controller && Controller->IsLocalController())
	{
		OnViewUpdated.Broadcast(View);
	}
	return true;
}

void UObjectiveTeamViewComponent::OnRep_View()
{
	++ClientChangeCount;
	OnViewUpdated.Broadcast(View);
}

#if WITH_STEALTH_TEST_SEAMS
void UObjectiveTeamViewComponent::ApplyViewWithoutDirtyMarkForTest(const FObjectiveTeamView& NewView)
{
	View = NewView;
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UObjectiveTeamViewComponent::MarkViewDirtyForTest()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(UObjectiveTeamViewComponent, View, this);
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}
#endif
