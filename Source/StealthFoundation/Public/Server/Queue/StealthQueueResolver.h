#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthQueuePayloads.h"
#include "UObject/Interface.h"

#include "StealthQueueResolver.generated.h"

/**
 * The resolver contract (ADR-0002 section 5; Round R13, R14). Each owning system registers one resolver per
 * item type (and per TimerFired subkind) from the composition root; the queue calls it once per stamped
 * item, in key order, inside the item's slot.
 *
 * A UINTERFACE because the UResolutionQueue facade holds registrations as TWeakInterfacePtr (ADR-0002 E7). The
 * plain FResolutionQueueCore takes raw pointers, so headless tests register plain C++ stubs: UHT gives the
 * I-class a non-pure _getUObject (verified in the 5.8.4 generated headers), so a non-UObject can implement it.
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UQueueResolver : public UInterface
{
	GENERATED_BODY()
};

class STEALTHFOUNDATION_API IQueueResolver
{
	GENERATED_BODY()

public:
	/**
	 * Re-validates Item against live state at its slot and executes it. Follow-ons run inline, depth-first,
	 * through FResolutionQueueCore::ExecuteFollowOn. Voided means the resolver changed nothing and emitted
	 * nothing; the queue then logs exactly one Voided entry for it. Never re-submit or re-stamp an outcome.
	 */
	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) = 0;

	/**
	 * Called instead of Resolve when the queue rejects Item with Rejected_Latched (ADR-0002 section 8.1). The
	 * owner may append its own log entry for the attempt from inside the slot, e.g. the R19
	 * TendCompleted(NoEffect, Latched = true) entry for a rejected final HoldTimer(Complete) (story rmf-017).
	 * It must change no other state. Default: nothing.
	 */
	virtual void OnRejectedLatched(const FQueueItem& Item, FQueueKey Key) {}
};
