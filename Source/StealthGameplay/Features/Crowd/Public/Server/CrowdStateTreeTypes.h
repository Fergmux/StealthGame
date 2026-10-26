#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"

#include "CrowdStateTreeTypes.generated.h"

/**
 * The crowd StateTree vocabulary (ADR-0016 section 3; GDD States and Transitions; story cnb-012): the state mirror enum
 * the tree's states are configured with, the one task every state runs and the enter condition that selects a state
 * from the agent's activity, and the five interrupt events.
 */

/** The StateTree-facing mirror of ECrowdActivity (same order; the asset stores these values: append only). */
UENUM()
enum class ECrowdTreeActivity : uint8
{
	Dormant,
	Emerging,
	Routing,
	Pausing,
	Conversing,
	Tending,
	Bracing,
	Hiding,
	Exiting,
	Reacting,
	Halted,
};

/** The interrupt events (ADR-0016 section 3). Priority is the state order: Halted > Reacting > the rest. */
STEALTHGAMEPLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Crowd_Stimulus_Strike);
STEALTHGAMEPLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Crowd_Stimulus_Salient);
STEALTHGAMEPLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Crowd_Path_Blocked);
STEALTHGAMEPLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Crowd_Path_Failed);
STEALTHGAMEPLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Crowd_Converse_Faced);

/** Instance data of the activity task (the state it stands for). */
USTRUCT()
struct STEALTHGAMEPLAY_API FCrowdActivityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	ECrowdTreeActivity Activity = ECrowdTreeActivity::Routing;
};

/**
 * FCrowdActivityTask: the one thin task of every crowd state. EnterState writes the activity mirror; each tree tick (the
 * phase-aligned decision grid) asks the crowd controller's agent to decide and completes the state when the agent's
 * activity changed, so the tree re-selects the state that matches it. It never moves the pawn: decisions hand intents to
 * the follower and the sink. Not considered for scheduling, so the crowd grid (UCrowdStateTreeComponent) governs the
 * tick rate.
 */
USTRUCT(meta = (DisplayName = "Crowd Activity", Category = "Crowd"))
struct STEALTHGAMEPLAY_API FCrowdActivityTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCrowdActivityTaskInstanceData;

	FCrowdActivityTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/** Instance data of the activity condition. */
USTRUCT()
struct STEALTHGAMEPLAY_API FCrowdActivityConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	ECrowdTreeActivity Activity = ECrowdTreeActivity::Routing;
};

/** FCrowdActivityCondition: true when the crowd controller's agent is in Activity (a state's enter condition). */
USTRUCT(meta = (DisplayName = "Crowd Activity Is", Category = "Crowd"))
struct STEALTHGAMEPLAY_API FCrowdActivityCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCrowdActivityConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
