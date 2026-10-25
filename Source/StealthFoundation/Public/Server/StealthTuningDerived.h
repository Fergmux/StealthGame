#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

struct FStealthServerTuning;

/**
 * The F-3 derived design indicators (GDD F-3; ADR-0004 section 3; story tdc-047), computed in double
 * precision on the clamped value set and read by the invariant checks (story tdc-048). They are design
 * indicators for invariants and warnings, never gameplay values, and computing them never writes a value.
 * With L = NpcKillStrikeLimit:
 *   CrowdRatio         = NpcCount / (2 * TeamSize)
 *   DutyCycle          = DefenceDurationSec / DefenceCooldownSec
 *   P                  = TeamSize / (TeamSize + NpcCount)
 *   BlindGuessSuccess  = 1 - (1 - P)^L
 *   ExpectedBlindKills = L * P / (1 - P)
 *   MissToHit          = NpcCount / TeamSize
 * F-1 always runs first in the pipeline, so the denominators are positive; on unclamped input a ratio
 * whose denominator is not positive is reported as 0 rather than dividing by zero.
 */
struct STEALTHFOUNDATION_API FTuningDerived
{
	double CrowdRatio = 0.0;
	double DutyCycle = 0.0;
	double P = 0.0;
	double BlindGuessSuccess = 0.0;
	double ExpectedBlindKills = 0.0;
	double MissToHit = 0.0;

	/** Computes every indicator from a (clamped) server face. Pure. */
	static FTuningDerived Compute(const FStealthServerTuning& Values);
};
