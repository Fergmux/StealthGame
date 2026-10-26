#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/**
 * SipHash-2-4 (Aumasson & Bernstein), 128-bit key, 64-bit output: ADR-0006 section 3's `H64` and the block
 * function of FSecretStream. In-project C++ on uint64 add/rotate/xor only, identical on every platform.
 *
 * Server-only (ADR-0007 guard): keys are server secrets. The published reference vectors are the
 * session-travel epic's golden fixture (story st-005); the Round seam test (rmf-002) checks the first two.
 * Never log, trace or replicate a key or a result derived from a secret key (ADR-0006 section 7).
 */
namespace StealthRandom
{
	/** SipHash-2-4 with the key given as its two little-endian 64-bit words (K0 = key bytes 0-7). */
	STEALTHFOUNDATION_API uint64 SipHash24(uint64 K0, uint64 K1, TConstArrayView<uint8> Message);

	/** SipHash-2-4 with a 16-byte key. */
	STEALTHFOUNDATION_API uint64 SipHash24(const uint8 (&Key)[16], TConstArrayView<uint8> Message);

	/** Appends Value as Bytes little-endian bytes to Out (ADR-0006 section 3: built byte by byte, never memcpy). */
	inline void AppendLittleEndian(TArray<uint8, TInlineAllocator<64>>& Out, uint64 Value, int32 Bytes)
	{
		for (int32 Index = 0; Index < Bytes; ++Index)
		{
			Out.Add(static_cast<uint8>((Value >> (8 * Index)) & 0xFF));
		}
	}
}
