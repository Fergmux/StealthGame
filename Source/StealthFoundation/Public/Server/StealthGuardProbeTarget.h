#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/**
 * Placeholder server-only header used by the ADR-0007 guard compile probe
 * (tools/ci/compile-probes/ServerHeaderInPresentation.cpp, story tdc-005). Including it from a
 * StealthPresentation file must fail through StealthServerOnlyGuard.h; including it from
 * StealthGame (the positive control) must compile. It declares nothing used by game code.
 */
namespace StealthGuardProbe
{
	/** Trivial declaration so the header has content; never read by game code. */
	inline constexpr int32 ServerOnlyMarker = 1;
}
