#pragma once

// The pre-registered seeds of the Tell Matrix calibration gate (story cnb-023; GDD AC-CNB-10, AC-CNB-31). Registered
// 2026-10-27 before the first run of the gate. A failing seed is a defect: never reseed, never drop a seed.

#include "CoreMinimal.h"

namespace StealthCrowdCalibration
{
	/** One crowd sub-seed per segment; the gate's simulated time is split evenly over them. */
	inline constexpr uint64 Seeds[] = {
		0xCA11B0A7E0000001ull,
		0xCA11B0A7E0000002ull,
		0xCA11B0A7E0000003ull,
		0xCA11B0A7E0000004ull,
		0xCA11B0A7E0000005ull,
		0xCA11B0A7E0000006ull,
	};

	/** Simulated city minutes of each gate (AC-CNB-10 and AC-CNB-31: 600). */
	inline constexpr double GateMinutes = 600.0;
}
