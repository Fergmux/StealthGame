#pragma once

#include "CoreMinimal.h"

/**
 * FIntStream: the public integer stream (ADR-0006 section 6), PCG32 XSH-RR 64/32 (O'Neill), for the Generator's
 * stage streams and client decoration only; never for a hidden decision (those use FSecretStream). Client-safe
 * (Public/View: no server-only guard, no Public/Server include).
 *
 * Seeded exactly as the reference pcg32_srandom_r(initstate, initseq): state = 0; inc = (initseq << 1) | 1; step;
 * state += initstate; step. The reference outputs for (42, 54) are the golden fixture StealthGame.Random.Pcg32Vectors.
 * All arithmetic is uint64/uint32 with defined wrap, shifts are logical, the rotation is written (32u - rot) & 31u
 * (never unary minus on an unsigned value, MSVC C4146), and no float appears anywhere.
 */
struct FIntStream
{
	/** The reference PCG32 LCG multiplier. */
	static constexpr uint64 Multiplier = 6364136223846793005ull;

	/** Seeds as pcg32_srandom_r(InitState, InitSeq). */
	constexpr FIntStream(uint64 InitState, uint64 InitSeq)
		: State(0u)
		, Increment((InitSeq << 1u) | 1u)
	{
		Step();
		State += InitState;
		Step();
	}

	/** The next 32-bit output (pcg32_random_r). */
	constexpr uint32 NextU32()
	{
		const uint64 Old = State;
		Step();
		const uint32 XorShifted = static_cast<uint32>(((Old >> 18u) ^ Old) >> 27u);
		const uint32 Rot = static_cast<uint32>(Old >> 59u);
		return (XorShifted >> Rot) | (XorShifted << ((32u - Rot) & 31u));
	}

private:
	constexpr void Step()
	{
		State = State * Multiplier + Increment;
	}

	uint64 State;
	uint64 Increment;
};
