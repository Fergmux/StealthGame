#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

/**
 * Test-seam sentinel (ADR-0008 sections 4 and 7, story tdc-006).
 *
 * WITH_STEALTH_TEST_SEAMS is a preprocessor macro and leaves no symbol in a binary, so test-seam
 * code carries a unique sentinel string (prefix STEALTH_TEST_SEAM_SENTINEL_, stored once as ASCII
 * and once as UTF-16) that tools/ci/shipping-symbol-check.sh searches for: it must be found in a
 * Development package (positive control) and absent from a Shipping one.
 */

/**
 * Stores the sentinel literals in volatile globals so the optimiser cannot drop them. Called once
 * from StealthFoundation's module startup; it registers nothing else.
 */
STEALTHFOUNDATION_API void StealthRegisterTestSeamSentinel();

/** The registered wide sentinel, or nullptr before StealthRegisterTestSeamSentinel ran. */
STEALTHFOUNDATION_API const TCHAR* StealthGetTestSeamSentinel();

#endif // WITH_STEALTH_TEST_SEAMS
