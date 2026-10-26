#pragma once

// Committed golden vectors of F2's chain-gap compensation (GDD crowd-npc-behaviour F2; story cnb-018, test
// StealthGame.Crowd.Tells.CompensationGolden). The story's Fixtures/CrowdTellCompensationVectors.json is this header
// (no file I/O in a unit test). Computed independently (double precision) from the default interval keys:
//   r_j = 60 / interval (per minute), Lambda = sum r_j = 0.6837988218226827, T_gap = 20 / 60 min,
//   a_j = r_j / (1 - (Lambda - r_j) * T_gap).
// The GDD worked example rounds to bush 0.238 and run 0.180 per minute.

#include "CoreMinimal.h"

namespace CrowdTellCompensationVectors
{
	inline constexpr double Lambda = 0.6837988218226827;

	/** a_j per minute, in ECrowdTell order (Run, Vault, Climb, Brace, DecoyTend, Bush, Reversal, HeadHold). */
	inline constexpr double CompensatedPerMin[] = {
		0.18034961184141263,
		0.052905724964999866,
		0.022624633971608132,
		0.025682719690609625,
		0.005909362112602041,
		0.238454701159717,
		0.18463442271950728,
		0.12416184658361422,
	};
}
