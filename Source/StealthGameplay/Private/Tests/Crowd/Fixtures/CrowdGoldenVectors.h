#pragma once

// Committed golden vectors of the crowd's continuous-sample mapping (ADR-0016 section 9, ADR-0006 rule 7; story cnb-002,
// test StealthGame.Crowd.GoldenVectors). Inline constants rather than a JSON file so the test needs no file I/O
// (coding standards: unit tests read no files); the story's Fixtures/CrowdGoldenVectors.json path is this header.
//
// UnitFraction53 rows are exact by construction ((Bits >> 11) + 0.5) * 2^-53. The AS241 rows pin the bits this build
// computes; the test also checks each against an independent reference quantile (to 1e-12 relative, 1e-9 in the far
// tail), so a typo in a coefficient cannot be pinned as golden.

#include "CoreMinimal.h"

namespace CrowdGoldenVectors
{
	struct FUnitRow
	{
		uint64 Bits;
		/** The exact double, as its IEEE-754 bit pattern. */
		uint64 ExpectedDoubleBits;
	};

	inline constexpr FUnitRow UnitRows[] = {
		{ 0x0000000000000000ull, 0x3c90000000000000ull }, // all-zero: 2^-54, never 0
		{ 0xFFFFFFFFFFFFFFFFull, 0x3fefffffffffffffull }, // all-ones: 1 - 2^-54 rounds to 1.0 in double; the guard returns the largest double below 1
		{ 0x00000000000007FFull, 0x3c90000000000000ull }, // low 11 bits are dropped
		{ 0x0000000000000800ull, 0x3ca8000000000000ull }, // 1.5 * 2^-53
		{ 0x8000000000000000ull, 0x3fe0000000000000ull }, // 0.5 + 2^-54 ties to even: 0.5
		{ 0x123456789ABCDEF0ull, 0x3fb23456789abcdcull },
		{ 0xDEADBEEFCAFEF00Dull, 0x3febd5b7ddf95fdeull },
	};

	struct FNormalRow
	{
		double U;
		/** An independent reference value of Phi^-1(U) (to at least 15 significant digits). */
		double Reference;
		/** The tolerance against Reference (absolute). */
		double Tolerance;
	};

	inline constexpr FNormalRow NormalReferenceRows[] = {
		{ 0.5, 0.0, 1e-15 },
		{ 0.975, 1.959963984540054, 1e-12 },
		{ 0.025, -1.959963984540054, 1e-12 },
		{ 0.9, 1.2815515655446004, 1e-12 },
		{ 0.1, -1.2815515655446004, 1e-12 },
		{ 0.6, 0.2533471031357997, 1e-12 },
		{ 0.075, -1.4395314709384563, 1e-12 },
		{ 0.001, -3.090232306167814, 1e-12 },
		{ 0.999, 3.090232306167814, 1e-12 },
		{ 1e-10, -6.361340902404056, 1e-9 },
		{ 1e-12, -7.034483825301132, 1e-9 },
		{ 1.0 - 1e-12, 7.0344869100478356, 1e-9 },
	};

	/**
	 * Pinned output bits of InvNormalAS241 for U = UnitFraction53(Bits) at the edge inputs (counter 0, max uint64,
	 * all-zero and all-ones words, tails), recorded 2026-10-26 from the first Win64 run (the test prints a row it cannot match).
	 */
	struct FPinnedRow
	{
		uint64 Bits;
		uint64 ExpectedDoubleBits;
	};

	inline constexpr FPinnedRow PinnedNormalRows[] = {
		{ 0x0000000000000000ull, 0xC02095B059D67C4Cull }, // -8.2923610758135951
		{ 0xFFFFFFFFFFFFFFFFull, 0x40206B48528CEA51ull }, // 8.2095361516013856
		{ 0x8000000000000000ull, 0x0000000000000000ull }, // 0 (the median)
		{ 0x123456789ABCDEF0ull, 0xBFF77B26344B88F0ull }, // -1.4675657313132966
		{ 0xDEADBEEFCAFEF00Dull, 0x3FF20293484BA845ull }, // 1.1256287406963719
		{ 0x0000100000000000ull, 0xC0130D5024A3C6AAull }, // -4.7630010342560727
	};
}
