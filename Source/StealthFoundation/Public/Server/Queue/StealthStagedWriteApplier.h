#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthQueuePayloads.h"
#include "UObject/Interface.h"

#include "StealthStagedWriteApplier.generated.h"

/**
 * Staged-write appliers and the round-end route (ADR-0002 sections 6 and 8.4). Declared with the queue contract
 * so the payload types and every interface that reads them stay in Foundation (ADR-0002 section 2a); the
 * staging pass that calls them is a later Exit A story.
 */

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UStagedWriteApplier : public UInterface
{
	GENERATED_BODY()
};

/**
 * Applies one already-decided staged write (DeathWrite, CounterPose; Assassination registers them). Apply-only:
 * it decides nothing, never calls StageWrite or ScheduleAt, and treats a stale FPawnHandle as a no-op.
 */
class STEALTHFOUNDATION_API IStagedWriteApplier
{
	GENERATED_BODY()

public:
	virtual void ApplyStagedWrite(const FStagedWrite& Write) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTerminalDecision : public UInterface
{
	GENERATED_BODY()
};

/** Round's round-end step (ADR-0002 section 8.4): the RoundEndStep write is routed here, never to an applier. */
class STEALTHFOUNDATION_API ITerminalDecision
{
	GENERATED_BODY()

public:
	virtual void RunRoundEndStep(const FRoundEndPayload& Payload) = 0;
};
