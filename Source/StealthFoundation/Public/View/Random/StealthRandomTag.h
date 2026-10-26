#pragma once

#include "CoreMinimal.h"

/**
 * Stream tags (ADR-0006 section 5): fixed uint64 constants per consumer, packed little-endian from exactly
 * eight ASCII bytes. Multicharacter literals are `int` and implementation-defined (ADR-0006 engine review 5),
 * so tags are always built with this constexpr function.
 */
namespace StealthRandom
{
	/** Packs the eight ASCII characters of Text (a string literal of length 8) little-endian into a uint64. */
	template <SIZE_T N>
	constexpr uint64 Tag(const char (&Text)[N])
	{
		static_assert(N == 9, "StealthRandom::Tag takes exactly eight ASCII characters");
		uint64 Packed = 0;
		for (SIZE_T Index = 0; Index < 8; ++Index)
		{
			Packed |= static_cast<uint64>(static_cast<uint8>(Text[Index])) << (8 * Index);
		}
		return Packed;
	}
}

static_assert(StealthRandom::Tag("ABCDEFGH") == 0x4847464544434241ull, "Tag packs little-endian");
