#pragma once

// THROWAWAY spike stub for IVP spike S10 (story production/epics/information-visibility-policy/
// story-005-spike-s10-reopen-behaviour-and-netguid.md, ADR-0003 sections 5.6-5.8): the prototype pawn has no
// FActionState / FDeathState and no RefreshPresentation path, so the S10 rig adds this replicated component to
// each subject at run time. It carries the two server times the production pawn will carry (an action's
// StartServerTime and the DeathServerTime) and, on a client, runs a stub RefreshPresentation(EstimatedServerNow)
// from replicated state only when the (re-created) actor begins play, then samples the pose time and the root
// location at the first frame the new actor ticks. Records: "reopenPresent", "reopenFirstFrame".

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StealthSpikeActionStateComponent.generated.h"

/** Replicated stand-in for FActionState.StartServerTime and FDeathState.DeathServerTime. */
USTRUCT()
struct FStealthSpikeActionState
{
	GENERATED_BODY()

	/** Server world time the current action (a looping pose) started. */
	UPROPERTY()
	double StartServerTime = 0.0;

	/** Server world time of the death write; negative while alive. */
	UPROPERTY()
	double DeathServerTime = -1.0;
};

/** The S10 presentation stub; added by the server to each rig subject. */
UCLASS()
class UStealthSpikeActionStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStealthSpikeActionStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Replicated action and death times. */
	UPROPERTY(Replicated)
	FStealthSpikeActionState State;

	/** Client frames after begin play at which the render client captures a screenshot (empty: none). */
	static TArray<int32>& CaptureFrames();

	/** Client: how many re-opens per process may still capture a frame sequence. */
	static int32& CaptureBudget();

	/**
	 * S10 re-run (story-056): when true, the component does nothing on its own; the scenario's warm-up applies the
	 * presentation with ApplyPresentation(EstimatedServerNow()) from the ADR-0002 section 9 harness estimator.
	 */
	static bool& ExternalApply();

	/** Client: the S10 re-run presentation apply ("present" record: the estimator time used by the evaluation). */
	void ApplyPresentation(double EstimatedServerNow);

private:
	/** Client: the idempotent presentation refresh from replicated state (stub: computes the pose and death times). */
	void RefreshPresentation(double EstimatedServerNow, const TCHAR* Phase);

	int32 TicksSinceBegin = 0;
	bool bCapturing = false;
	FString CaptureTag;
};
