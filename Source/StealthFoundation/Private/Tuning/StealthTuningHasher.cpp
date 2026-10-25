// Story tdc-045: the F-2 canonical tuning hash (see Server/StealthTuningHasher.h).

#include "Server/StealthTuningHasher.h"

#include "Server/StealthFnv1a64.h"
#include "Server/StealthServerTuning.h"

#include <cmath>
#include <cstring>

namespace
{
	void AppendLittleEndian(TArray<uint8>& Out, uint64 Value, int32 ByteCount)
	{
		for (int32 Index = 0; Index < ByteCount; ++Index)
		{
			Out.Add(static_cast<uint8>((Value >> (8 * Index)) & 0xFF));
		}
	}

	TArray<FTuningHashEntry> SortedByKey(TConstArrayView<FTuningHashEntry> Entries)
	{
		TArray<FTuningHashEntry> Sorted(Entries.GetData(), Entries.Num());
		Sorted.Sort([](const FTuningHashEntry& A, const FTuningHashEntry& B)
		{
			const int32 Common = FMath::Min(A.Key.Len(), B.Key.Len());
			const int32 Cmp = Common > 0 ? std::memcmp(A.Key.GetData(), B.Key.GetData(), Common) : 0;
			return Cmp != 0 ? Cmp < 0 : A.Key.Len() < B.Key.Len();
		});
		return Sorted;
	}
}

void FTuningHasher::AppendEntry(TArray<uint8>& Out, const FTuningHashEntry& Entry)
{
	Out.Add(static_cast<uint8>(Entry.Key.Len()));
	Out.Append(reinterpret_cast<const uint8*>(Entry.Key.GetData()), Entry.Key.Len());
	Out.Add(Entry.TypeTag);
	switch (Entry.TypeTag)
	{
	case StealthTuningTypeTag::Bool:
		Out.Add(Entry.Value != 0.0 ? 1 : 0);
		break;
	case StealthTuningTypeTag::Int:
	case StealthTuningTypeTag::Enum:
		AppendLittleEndian(Out, static_cast<uint64>(static_cast<int64>(std::llround(Entry.Value))), 8);
		break;
	case StealthTuningTypeTag::SoftRef:
	{
		TArray<uint8> Path;
		for (const TCHAR Char : Entry.SoftPath)
		{
			// The validator rejects a non-ASCII path before hashing; package names are case-insensitive.
			const TCHAR Ascii = (Char >= TEXT('A') && Char <= TEXT('Z')) ? static_cast<TCHAR>(Char - TEXT('A') + TEXT('a')) : Char;
			Path.Add(static_cast<uint8>(Ascii & 0x7F));
		}
		AppendLittleEndian(Out, static_cast<uint64>(Path.Num()), 2);
		Out.Append(Path);
		break;
	}
	case StealthTuningTypeTag::Float:
	default:
	{
		float AsFloat = static_cast<float>(Entry.Value);
		if (AsFloat == 0.0f)
		{
			AsFloat = 0.0f; // -0.0 hashes as +0.0
		}
		uint32 Bits = 0;
		std::memcpy(&Bits, &AsFloat, sizeof(Bits));
		AppendLittleEndian(Out, Bits, 4);
		break;
	}
	}
}

TArray<uint8> FTuningHasher::BuildStream(int32 SchemaVersion, TConstArrayView<FTuningHashEntry> Entries)
{
	TArray<uint8> Stream;
	AppendLittleEndian(Stream, static_cast<uint32>(SchemaVersion), 4);
	for (const FTuningHashEntry& Entry : SortedByKey(Entries))
	{
		AppendEntry(Stream, Entry);
	}
	return Stream;
}

uint64 FTuningHasher::HashEntries(int32 SchemaVersion, TConstArrayView<FTuningHashEntry> Entries)
{
	return StealthFnv1a64::Hash(BuildStream(SchemaVersion, Entries));
}

uint64 FTuningHasher::Hash(TConstArrayView<FTuningSchemaRow> Schema, const FStealthServerTuning& Values, int32 SchemaVersion, bool bPublicOnly)
{
	TArray<FTuningHashEntry> Entries;
	Entries.Reserve(Schema.Num());
	for (const FTuningSchemaRow& Row : Schema)
	{
		if ((bPublicOnly && !Row.bPublic) || Row.Get == nullptr)
		{
			continue;
		}
		FTuningHashEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Key = FAnsiStringView(Row.Key);
		Entry.TypeTag = Row.TypeTag;
		Entry.Value = Row.Get(Values);
	}
	return HashEntries(SchemaVersion, Entries);
}

uint64 FTuningHasher::Hash(const FStealthServerTuning& Values, int32 SchemaVersion, bool bPublicOnly)
{
	return Hash(GetTuningSchema(), Values, SchemaVersion, bPublicOnly);
}
