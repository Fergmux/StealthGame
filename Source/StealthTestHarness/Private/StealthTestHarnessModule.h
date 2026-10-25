#pragma once

#include "CoreMinimal.h"

/**
 * StealthTestHarness: Shipping/Test-excluded runtime module hosting the T2/T3 leak and parity
 * harness (ADR-0008). It exposes no public API, so every file stays in Private/.
 */

/** Log category for the harness. New code logs with UE_LOGF only (ADR-0007 section 6). */
DECLARE_LOG_CATEGORY_EXTERN(LogStealthHarness, Log, All);
