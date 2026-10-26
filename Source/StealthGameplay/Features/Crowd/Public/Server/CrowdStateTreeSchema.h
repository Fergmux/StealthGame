#pragma once

#include "StealthServerOnlyGuard.h"

#include "Components/StateTreeAIComponentSchema.h"
#include "CoreMinimal.h"

#include "CrowdStateTreeSchema.generated.h"

class UStateTree;

/**
 * UCrowdStateTreeSchema: the one schema of both crowd StateTrees (ADR-0016 section 3; story cnb-012):
 * ScheduledTickPolicy = Allowed (set here, never left to the StateTree.Component.DefaultScheduledTickAllowed cvar) and
 * AIControllerClass = AStealthCrowdController, context actor the Shared Pawn. The policy is cached on the compiled tree,
 * so the assets are recompiled after any change to this class (Stealth.Crowd.BuildAssets).
 */
UCLASS(NotBlueprintType, NotBlueprintable, meta = (DisplayName = "Crowd (Stealth)"))
class STEALTHGAMEPLAY_API UCrowdStateTreeSchema : public UStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	UCrowdStateTreeSchema(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** True when the schema allows scheduled ticks (the boot test asserts it). */
	bool AllowsScheduledTick() const { return ScheduledTickPolicy == EStateTreeComponentSchemaScheduledTickPolicy::Allowed; }

	/** True when Tree is compiled against this schema class with scheduled ticks allowed (a stale asset fails). */
	static bool IsCrowdTree(const UStateTree* Tree);
};
