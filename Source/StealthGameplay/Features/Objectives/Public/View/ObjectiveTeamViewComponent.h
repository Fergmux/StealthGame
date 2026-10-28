#pragma once

#include "CoreMinimal.h"
#include "View/ObjectiveTeamView.h"
#include "View/StealthRosterTeamView.h"

#include "ObjectiveTeamViewComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnObjectiveTeamViewUpdated, const FObjectiveTeamView& /*View*/);

/**
 * The Objectives part of C-TeamView (ADR-0020 section 6; ADR-0001 carrier; story oc-011). A default subobject of
 * AStealthPlayerController carrying one replicated, push-based FObjectiveTeamView. Controllers are bOnlyRelevantToOwner and
 * replicate their components through the registered sub-object list, so the view reaches the owning connection only
 * (COND_None, as the ADR states: the carrier, not a condition, is the owner-only gate; OBJ-V1). The listen host's own
 * controller holds the same component written by the same code.
 *
 * Server: written ONLY by UObjectivesService (its team-view writer: inside a queue slot, the OnRoundActive step, the Roster
 * rebind handler, or the reset clear), through ApplyView, which is change-only and marks the push-model property dirty on
 * every write, so the view updates whether Net.IsPushModelEnabled is on or off. Several writes in one server frame reach the
 * client as one change (OBJ-V2). Client: OnRep_View raises OnViewUpdated; presentation may also read GetView() each frame.
 *
 * Usage (client or host HUD):
 *   if (const UObjectiveTeamViewComponent* Objectives = PC->FindComponentByClass<UObjectiveTeamViewComponent>())
 *   {
 *       const FObjectiveTeamView& View = Objectives->GetView();   // own team only
 *   }
 */
UCLASS()
class STEALTHGAMEPLAY_API UObjectiveTeamViewComponent : public UTeamViewComponentBase
{
	GENERATED_BODY()

public:
	UObjectiveTeamViewComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Server (UObjectivesService only): writes NewView when it differs from the current value, marks the property dirty and
	 * asks for a net update. Returns false (nothing written, nothing marked) for an identical value.
	 */
	bool ApplyView(const FObjectiveTeamView& NewView);

	/** Server: last written; client: last received. */
	const FObjectiveTeamView& GetView() const { return View; }

	/** Server writes that changed the value (tests: the change-only and rebind write counts). */
	int32 GetServerWriteCount() const { return ServerWriteCount; }

	/** Client: replicated changes received (OnRep_View calls; tests: OBJ-V2's "exactly one change"). */
	int32 GetClientChangeCount() const { return ClientChangeCount; }

	/** Raised on the owning client when a new view arrives (and on the listen host for its own controller's writes). */
	FOnObjectiveTeamViewUpdated OnViewUpdated;

#if WITH_STEALTH_TEST_SEAMS
	/** Test seam (story oc-011 AC3 negative control): writes the value WITHOUT the push-model dirty mark. */
	void ApplyViewWithoutDirtyMarkForTest(const FObjectiveTeamView& NewView);
	/** Test seam: marks the property dirty and asks for a net update (the control's repair). */
	void MarkViewDirtyForTest();
#endif

protected:
	UFUNCTION()
	void OnRep_View();

private:
	UPROPERTY(ReplicatedUsing = OnRep_View)
	FObjectiveTeamView View;

	int32 ServerWriteCount = 0;
	int32 ClientChangeCount = 0;
};
