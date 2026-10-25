#pragma once

#include "CoreMinimal.h"

/**
 * StealthGameplay: the gameplay runtime module of the ADR-0007 layout. Source lives under
 * Core/ (systems) and Features/ (feature code built on Core); nothing under Core/ includes a
 * Features/ path. Public headers live under Public/View/ or Public/Server/.
 */

/** Log category for the Gameplay module. New code logs with UE_LOGF only (ADR-0007 section 6). */
STEALTHGAMEPLAY_API DECLARE_LOG_CATEGORY_EXTERN(LogStealthGameplay, Log, All);
