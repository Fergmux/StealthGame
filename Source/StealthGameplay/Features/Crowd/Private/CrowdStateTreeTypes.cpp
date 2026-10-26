#include "Server/CrowdStateTreeTypes.h"

#include "Server/StealthCrowdController.h"
#include "StateTreeExecutionContext.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Crowd_Stimulus_Strike, "Crowd.Stimulus.Strike", "A strike within reaction range (R8); wakes the crowd tree.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Crowd_Stimulus_Salient, "Crowd.Stimulus.Salient", "Salient motion within range (G4); wakes the crowd tree.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Crowd_Path_Blocked, "Crowd.Path.Blocked", "The follower is blocked (R3.3).");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Crowd_Path_Failed, "Crowd.Path.Failed", "A path query failed or came back partial.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Crowd_Converse_Faced, "Crowd.Converse.Faced", "A resting neighbour faces this pawn (C3).");

FCrowdActivityTask::FCrowdActivityTask()
{
	bShouldCallTick = true;
	// The crowd grid (UCrowdStateTreeComponent) governs the cadence, not the task.
	bConsideredForScheduling = false;
	bShouldStateChangeOnReselect = true;
}

EStateTreeRunStatus FCrowdActivityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AStealthCrowdController* Controller = Cast<AStealthCrowdController>(Context.GetOwner()))
	{
		const FInstanceDataType& Data = Context.GetInstanceData(*this);
		Controller->OnTreeStateEntered(static_cast<uint8>(Data.Activity));
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FCrowdActivityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AStealthCrowdController* Controller = Cast<AStealthCrowdController>(Context.GetOwner());
	if (Controller == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	// One decision on the grid (or after a wake); the state completes when the agent's activity is no longer its own,
	// so the tree re-selects the matching state (the enter conditions mirror the agent).
	Controller->DecideFromTree();
	return Controller->GetTreeActivity() == static_cast<uint8>(Data.Activity) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

bool FCrowdActivityCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AStealthCrowdController* Controller = Cast<AStealthCrowdController>(Context.GetOwner());
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	return Controller != nullptr && Controller->GetTreeActivity() == static_cast<uint8>(Data.Activity);
}
