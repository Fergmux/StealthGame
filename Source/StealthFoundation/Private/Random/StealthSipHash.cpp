#include "Server/Random/StealthSipHash.h"

namespace
{
	FORCEINLINE uint64 Rotl(uint64 X, int32 B)
	{
		return (X << B) | (X >> (64 - B));
	}

	FORCEINLINE void SipRound(uint64& V0, uint64& V1, uint64& V2, uint64& V3)
	{
		V0 += V1; V1 = Rotl(V1, 13); V1 ^= V0; V0 = Rotl(V0, 32);
		V2 += V3; V3 = Rotl(V3, 16); V3 ^= V2;
		V0 += V3; V3 = Rotl(V3, 21); V3 ^= V0;
		V2 += V1; V1 = Rotl(V1, 17); V1 ^= V2; V2 = Rotl(V2, 32);
	}

	FORCEINLINE uint64 ReadLittleEndian64(const uint8* Bytes)
	{
		uint64 Value = 0;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Value |= static_cast<uint64>(Bytes[Index]) << (8 * Index);
		}
		return Value;
	}
}

uint64 StealthRandom::SipHash24(uint64 K0, uint64 K1, TConstArrayView<uint8> Message)
{
	uint64 V0 = 0x736f6d6570736575ull ^ K0;
	uint64 V1 = 0x646f72616e646f6dull ^ K1;
	uint64 V2 = 0x6c7967656e657261ull ^ K0;
	uint64 V3 = 0x7465646279746573ull ^ K1;

	const int32 Length = Message.Num();
	const int32 FullBlocks = Length / 8;
	const uint8* Data = Message.GetData();
	for (int32 Block = 0; Block < FullBlocks; ++Block)
	{
		const uint64 M = ReadLittleEndian64(Data + Block * 8);
		V3 ^= M;
		SipRound(V0, V1, V2, V3);
		SipRound(V0, V1, V2, V3);
		V0 ^= M;
	}

	// Final block: the remaining bytes, with the message length (mod 256) in the top byte.
	uint64 Last = static_cast<uint64>(Length & 0xFF) << 56;
	const int32 Tail = Length - FullBlocks * 8;
	for (int32 Index = 0; Index < Tail; ++Index)
	{
		Last |= static_cast<uint64>(Data[FullBlocks * 8 + Index]) << (8 * Index);
	}
	V3 ^= Last;
	SipRound(V0, V1, V2, V3);
	SipRound(V0, V1, V2, V3);
	V0 ^= Last;

	V2 ^= 0xFF;
	SipRound(V0, V1, V2, V3);
	SipRound(V0, V1, V2, V3);
	SipRound(V0, V1, V2, V3);
	SipRound(V0, V1, V2, V3);
	return V0 ^ V1 ^ V2 ^ V3;
}

uint64 StealthRandom::SipHash24(const uint8 (&Key)[16], TConstArrayView<uint8> Message)
{
	return SipHash24(ReadLittleEndian64(Key), ReadLittleEndian64(Key + 8), Message);
}
