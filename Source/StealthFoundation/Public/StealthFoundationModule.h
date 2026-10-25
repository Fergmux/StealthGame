#pragma once

#include "CoreMinimal.h"

/**
 * StealthFoundation: the bottom runtime module of the ADR-0007 layout (identity and
 * replication policy, resolution queue, tuning, session, seeds and shared test support).
 *
 * Public headers live under Public/View/ (client-safe) or Public/Server/ (server-only,
 * each including StealthServerOnlyGuard.h first). This module header and the guard are
 * the only headers allowed directly in Public/.
 */

/** Log category for the Foundation module. New code logs with UE_LOGF only (ADR-0007 section 6). */
STEALTHFOUNDATION_API DECLARE_LOG_CATEGORY_EXTERN(LogStealthFoundation, Log, All);
