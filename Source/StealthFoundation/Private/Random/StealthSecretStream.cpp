#include "Server/Random/StealthSecretStream.h"

#include "Server/Random/StealthSipHash.h"

FSecretStream::FSecretStream(uint64 InSubSeed, uint64 InStreamTag)
	: SubSeed(InSubSeed)
	, StreamTag(InStreamTag)
{
}

uint64 FSecretStream::NextU64()
{
	TArray<uint8, TInlineAllocator<64>> Message;
	Message.Add(0x01);
	StealthRandom::AppendLittleEndian(Message, StreamTag, 8);
	StealthRandom::AppendLittleEndian(Message, Counter, 8);
	++Counter;
	return StealthRandom::SipHash24(SubSeed, KStreamConst, Message);
}

uint64 FSecretStream::Draw(TConstArrayView<uint64> CounterTuple) const
{
	TArray<uint8, TInlineAllocator<64>> Message;
	Message.Add(0x02);
	StealthRandom::AppendLittleEndian(Message, StreamTag, 8);
	for (const uint64 Value : CounterTuple)
	{
		StealthRandom::AppendLittleEndian(Message, Value, 8);
	}
	return StealthRandom::SipHash24(SubSeed, KStreamConst, Message);
}
