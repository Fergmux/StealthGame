#include "Server/CrowdStateTreeSchema.h"

#include "Server/CrowdAssetKeys.h"
#include "Server/StealthCrowdController.h"
#include "StateTree.h"
#include "View/Core/StealthPawn.h"

UCrowdStateTreeSchema::UCrowdStateTreeSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ScheduledTickPolicy = EStateTreeComponentSchemaScheduledTickPolicy::Allowed;
	AIControllerClass = AStealthCrowdController::StaticClass();
	ContextActorClass = AStealthPawn::StaticClass();
}

bool UCrowdStateTreeSchema::IsCrowdTree(const UStateTree* Tree)
{
	if (Tree == nullptr || !Tree->IsReadyToRun())
	{
		return false;
	}
	const UCrowdStateTreeSchema* Schema = Cast<UCrowdStateTreeSchema>(Tree->GetSchema());
	return Schema != nullptr && Schema->AllowsScheduledTick() && Tree->IsScheduledTickAllowed();
}

namespace CrowdStateTreeSchemaLocal
{
	bool CheckTree(const UObject* Object)
	{
		return UCrowdStateTreeSchema::IsCrowdTree(Cast<UStateTree>(Object));
	}

	/** Registers the StateTree half of the asset-key validator (story cnb-003's mechanism) at module start. */
	struct FRegisterSchemaCheck
	{
		FRegisterSchemaCheck() { StealthCrowdTuning::SetStateTreeSchemaCheck(&CheckTree); }
	};
	FRegisterSchemaCheck GRegister;
}
