#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

struct FStealthServerTuning;

/**
 * One key of an F-2 hash input: its name, type tag and effective value. Value carries bool (0/1),
 * int32, enum and float values; SoftPath carries a SoftRef value (type tag 4) and is ignored for
 * every other tag. Key and SoftPath are views: the caller keeps them alive for the hash call.
 */
struct FTuningHashEntry
{
	/** ASCII key, 1 to 255 bytes. */
	FAnsiStringView Key;
	/** StealthTuningTypeTag value. */
	uint8 TypeTag = StealthTuningTypeTag::Float;
	/** The effective value (F-1 has already run, so a float is never NaN). */
	double Value = 0.0;
	/** SoftRef only: FSoftObjectPath::GetAssetPath().ToString(), validated ASCII by the validator; empty = null. */
	FStringView SoftPath;
};

/**
 * The canonical tuning hash (GDD F-2, CR-7; ADR-0004 section 5; story tdc-045): the project's own
 * FNV-1a-64 (StealthFnv1a64.h) over the byte stream
 *   u32le(SchemaVersion) || for each key, sorted by byte-wise case-sensitive memcmp of its ASCII name:
 *   u8(len) || ascii(name) || u8(typeTag) || canon(value)
 * with canon: bool 1 byte (0/1); int32 and enum widened to int64, 8 bytes little-endian; float 4 bytes
 * IEEE-754 little-endian with -0.0 mapped to +0.0; SoftRef (ADR-0004 amendment 2026-10-25b) the
 * ASCII-lowercased path as u16le(byte length) || bytes (a null path is u16le(0)).
 * TuningHash covers every key and is never replicated; ClientHash covers the public keys only.
 * Endianness is explicit, so the result never depends on the host. Never GetTypeHash,
 * UE::HashStringFNV1a, CityHash or a struct memory image. Plain C++, no UWorld.
 */
class STEALTHFOUNDATION_API FTuningHasher
{
public:
	/** TuningHash (bPublicOnly false) or ClientHash (true) of a server face over the generated schema. */
	static uint64 Hash(const FStealthServerTuning& Values, int32 SchemaVersion, bool bPublicOnly);

	/** The same over an injected schema view (fixtures, tests; every row needs its Get accessor). */
	static uint64 Hash(TConstArrayView<FTuningSchemaRow> Schema, const FStealthServerTuning& Values, int32 SchemaVersion, bool bPublicOnly);

	/** FNV-1a-64 of the F-2 stream of explicit entries (the golden fixture's form). Input order is irrelevant. */
	static uint64 HashEntries(int32 SchemaVersion, TConstArrayView<FTuningHashEntry> Entries);

	/** The F-2 byte stream itself (sorted), for byte-level assertions. */
	static TArray<uint8> BuildStream(int32 SchemaVersion, TConstArrayView<FTuningHashEntry> Entries);

	/** Appends one key's u8(len) || ascii || u8(typeTag) || canon(value) to Out. */
	static void AppendEntry(TArray<uint8>& Out, const FTuningHashEntry& Entry);
};
