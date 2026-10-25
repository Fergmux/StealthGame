#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/**
 * The project's own FNV-1a-64 over bytes (ADR-0004 section 5; stories tdc-043 and tdc-045). Used by
 * the schema fingerprint and by FTuningHasher. Deliberately not GetTypeHash (not stable by
 * contract), not UE::HashStringFNV1a (a string API that widens each character) and never a struct
 * memory image. Parameters are the standard FNV-1a-64 ones (offset basis 14695981039346656037,
 * prime 1099511628211; FNV specification, Fowler/Noll/Vo).
 */
namespace StealthFnv1a64
{
	inline constexpr uint64 OffsetBasis = 14695981039346656037ull;
	inline constexpr uint64 Prime = 1099511628211ull;

	/** Feeds Bytes into State byte by byte. */
	inline void Update(uint64& State, const uint8* Bytes, int64 Count)
	{
		for (int64 Index = 0; Index < Count; ++Index)
		{
			State ^= static_cast<uint64>(Bytes[Index]);
			State *= Prime;
		}
	}

	/** FNV-1a-64 of a whole byte buffer. */
	inline uint64 Hash(TConstArrayView<uint8> Bytes)
	{
		uint64 State = OffsetBasis;
		Update(State, Bytes.GetData(), Bytes.Num());
		return State;
	}
}
