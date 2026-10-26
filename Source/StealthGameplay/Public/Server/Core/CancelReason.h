#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#include "CancelReason.generated.h"

/**
 * Why an action ended without completing (ADR-0011 section 6; story iif-001). Server-only: it names the cause in the
 * action hub's OnActionCancelled for feature listeners and is never sent to anyone (FHoldStatus carries no cancel
 * reason, ADR-0011 section 7). Declared with the hub in StealthGameplay, not in Foundation.
 */
UENUM()
enum class ECancelReason : uint8
{
	Released,
	Moved,
	Superseded,
	Illegal,
	OutOfRange,
	TargetInvalid,
	PawnRemoved,
	Reversed,
};

/** Number of ECancelReason values. */
inline constexpr int32 GCancelReasonCount = 8;
