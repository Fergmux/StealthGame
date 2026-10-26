#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "View/Random/StealthRandomTag.h"

/**
 * FSecretStream: the keyed-hash DRBG for every hidden draw (ADR-0006 section 5), SipHash-2-4 in counter mode.
 *
 *   K         = (SubSeed, KStreamConst)       second key word: one fixed public constant, never the tag
 *   Block(i)  = SipHash(K, 0x01 | u64le(StreamTag) | u64le(i))          mode 0x01: sequential stream
 *   Draw(...) = SipHash(K, 0x02 | u64le(StreamTag) | u64le(i) | ...)    mode 0x02: counter tuple
 *
 * The sub-seed is the stream's secret: never log, trace or replicate it, a key or the stream state
 * (ADR-0006 section 7; FSecretStream is in the ADR-0007 lint (i) identifier list). Plain C++, server only.
 */
struct STEALTHFOUNDATION_API FSecretStream
{
	/**
	 * The fixed public second key word (ADR-0006 section 5 "KStreamConst"; the ADR fixes the shape, not the
	 * value: chosen by the RMF Exit A build, 2026-10-25, recorded in the decision log). Never change it
	 * without replacing the golden vectors (ADR-0006 Guideline 7).
	 */
	static constexpr uint64 KStreamConst = StealthRandom::Tag("SGSTREAM");

	FSecretStream(uint64 InSubSeed, uint64 InStreamTag);

	/** The next 64-bit block of the sequential stream (mode 0x01). */
	uint64 NextU64();

	/** The low 32 bits of the next block (each call consumes one block). */
	uint32 NextU32() { return static_cast<uint32>(NextU64()); }

	/** Counter-tuple random access (mode 0x02); does not advance the sequential counter. */
	uint64 Draw(TConstArrayView<uint64> CounterTuple) const;

private:
	uint64 SubSeed;
	uint64 StreamTag;
	uint64 Counter = 0;
};
