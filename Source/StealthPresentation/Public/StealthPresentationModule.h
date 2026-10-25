#pragma once

#include "CoreMinimal.h"

/**
 * StealthPresentation: the client-facing runtime module of the ADR-0007 layout (HUD, views,
 * presenters). It may include only Public/View/ headers of the modules below it; a
 * Public/Server/ header fails to compile here through StealthServerOnlyGuard.h.
 */

/** Log category for the Presentation module. New code logs with UE_LOGF only (ADR-0007 section 6). */
STEALTHPRESENTATION_API DECLARE_LOG_CATEGORY_EXTERN(LogStealthPresentation, Log, All);

#if !UE_BUILD_SHIPPING
/**
 * Build self-test probe (story tdc-002): true when STEALTH_PRESENTATION_MODULE is 1 in the
 * StealthPresentation translation unit that defines this function. Kept out of Shipping
 * (switches to WITH_STEALTH_TEST_SEAMS once that define exists, story tdc-008).
 */
STEALTHPRESENTATION_API bool StealthPresentationSeesGuardDefine();
#endif
