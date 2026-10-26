#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "StealthEventFlushBarrier.generated.h"

/**
 * The Roster event flush barrier (ADR-0002 section 5; Roster R12). After each Resolve the queue calls
 * FlushPending(), then checks HasPending() and reports a fault if anything is still queued, before it dequeues
 * the next item. Slot end means: after Resolve and FlushPending() have both returned. The Roster implements it.
 *
 * HasPending() is the query the queue's "asserts nothing remains queued" needs (RMF Exit A build 2026-10-25,
 * recorded in ADR-0002's Amendments).
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UEventFlushBarrier : public UInterface
{
	GENERATED_BODY()
};

class STEALTHFOUNDATION_API IEventFlushBarrier
{
	GENERATED_BODY()

public:
	/** Dispatches, FIFO, every event queued during the outermost call. */
	virtual void FlushPending() = 0;

	/** True while events are still queued (false right after a complete FlushPending). */
	virtual bool HasPending() const = 0;
};
