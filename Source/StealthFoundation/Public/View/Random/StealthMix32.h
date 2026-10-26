#pragma once

#include "CoreMinimal.h"

/**
 * Mix32: Session F5's reference integer mixer (ADR-0006 section 6; design/gdd/session-travel.md F5), the public
 * sub-seed and Generator stage-seed function. Client-safe (Public/View): it touches only the public Seed.
 *
 *   x = Seed ^ (Domain * 0x9E3779B1) ^ (Attempt * 0x85EBCA6B)          (mod 2^32)
 *   x ^= x >> 16; x *= 0x7FEB352D; x ^= x >> 15; x *= 0x846CA68B; x ^= x >> 16
 *   result = x & 0x7FFFFFFF, with 0 mapped to 1
 *
 * Every operand is uint32 and every shift is logical (a signed shift breaks the golden vectors). The three F5 golden
 * vectors are static_asserted below, so every compiler checks them at build time (ADR-0006 engine review 6); the
 * runtime fixture is StealthGame.Session.SeedDerivationGolden. An ADR may replace the mixer only by keeping the
 * contract and replacing these vectors in the same change (F5, ADR-0006 Guideline 7).
 */
namespace StealthRandom
{
	constexpr uint32 Mix32(uint32 Seed, uint32 Domain, uint32 Attempt)
	{
		uint32 X = Seed ^ (Domain * 0x9E3779B1u) ^ (Attempt * 0x85EBCA6Bu);
		X ^= X >> 16;
		X *= 0x7FEB352Du;
		X ^= X >> 15;
		X *= 0x846CA68Bu;
		X ^= X >> 16;
		X &= 0x7FFFFFFFu;
		return X == 0u ? 1u : X;
	}
}

// F5 golden vectors (Seed 123456789): PublicSubSeed(Layout, 0), PublicSubSeed(Layout, 1), and Mix32(Seed, 2, 0).
static_assert(StealthRandom::Mix32(123456789u, 1u, 0u) == 2107646860u, "Mix32 F5 golden vector (domain 1, attempt 0)");
static_assert(StealthRandom::Mix32(123456789u, 1u, 1u) == 523860287u, "Mix32 F5 golden vector (domain 1, attempt 1)");
static_assert(StealthRandom::Mix32(123456789u, 2u, 0u) == 313478722u, "Mix32 F5 golden vector (domain 2, attempt 0)");
