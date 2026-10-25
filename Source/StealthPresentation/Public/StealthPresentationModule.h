#pragma once

#include "CoreMinimal.h"

/**
 * StealthPresentation: the client-facing runtime module of the ADR-0007 layout (HUD, views,
 * presenters). It may include only Public/View/ headers of the modules below it; a
 * Public/Server/ header fails to compile here through StealthServerOnlyGuard.h.
 */

/** Log category for the Presentation module. New code logs with UE_LOGF only (ADR-0007 section 6). */
STEALTHPRESENTATION_API DECLARE_LOG_CATEGORY_EXTERN(LogStealthPresentation, Log, All);

#if defined(WITH_STEALTH_TEST_SEAMS) && WITH_STEALTH_TEST_SEAMS
/**
 * Build self-test probe (story tdc-002): true when STEALTH_PRESENTATION_MODULE is 1 in the
 * StealthPresentation translation unit that defines this function. A test seam: compiled
 * only under WITH_STEALTH_TEST_SEAMS (story tdc-008), never in Shipping.
 */
STEALTHPRESENTATION_API bool StealthPresentationSeesGuardDefine();
#endif
