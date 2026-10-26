#pragma once

#include "CoreMinimal.h"

/**
 * Ranged-draw helpers (ADR-0006 section 6, Key Interfaces), templated on the stream type so they serve both
 * FIntStream (public) and FSecretStream (server-only). A stream needs `uint32 NextU32()`.
 *
 * Built for the Round & Match Framework intake permutation (story rmf-006, which needed them before the
 * session-travel epic's story 007 adds its exhaustive and chi-square checks). No `%` on a result, no floats.
 */
namespace StealthRandom
{
	/**
	 * Uniform integer in [0, N) by Lemire's multiply-shift with rejection. N == 0 is refused: the misuse
	 * branch returns 0 in every build (ADR-0006 TD-ADR item 9).
	 */
	template <class TStream>
	uint32 UniformBelow(TStream& Stream, uint32 N)
	{
		if (N == 0)
		{
			return 0;
		}
		uint32 X = Stream.NextU32();
		uint64 M = static_cast<uint64>(X) * static_cast<uint64>(N);
		uint32 Low = static_cast<uint32>(M);
		if (Low < N)
		{
			// The one `%` computes the rejection threshold, never the result.
			const uint32 Threshold = (0u - N) % N;
			while (Low < Threshold)
			{
				X = Stream.NextU32();
				M = static_cast<uint64>(X) * static_cast<uint64>(N);
				Low = static_cast<uint32>(M);
			}
		}
		return static_cast<uint32>(M >> 32);
	}

	/**
	 * Uniform integer in [Lo, Hi]. Lo > Hi is refused (returns Lo). A span that wraps to 0 (the full 32-bit
	 * range) returns NextU32() directly.
	 */
	template <class TStream>
	int32 UniformInRange(TStream& Stream, int32 Lo, int32 Hi)
	{
		if (Lo > Hi)
		{
			return Lo;
		}
		const uint32 Span = static_cast<uint32>(Hi) - static_cast<uint32>(Lo) + 1u;
		if (Span == 0)
		{
			return static_cast<int32>(Stream.NextU32());
		}
		return static_cast<int32>(static_cast<uint32>(Lo) + UniformBelow(Stream, Span));
	}

	/**
	 * In-place Fisher-Yates shuffle (Durstenfeld, high index down). The input must already be in its
	 * canonical sorted order (ADR-0006 Guideline 5): the result is then a pure function of the stream.
	 */
	template <class TStream, class T>
	void FisherYates(TStream& Stream, TArrayView<T> Values)
	{
		for (int32 Index = Values.Num() - 1; Index > 0; --Index)
		{
			const int32 Pick = static_cast<int32>(UniformBelow(Stream, static_cast<uint32>(Index + 1)));
			if (Pick != Index)
			{
				::Swap(Values[Index], Values[Pick]);
			}
		}
	}
}
