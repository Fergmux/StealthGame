#include "Server/Random/StealthServerSeeds.h"

#include "Server/Random/StealthSipHash.h"
#include "Server/StealthFaultSink.h"

bool FStealthServerSeeds::Open(IEntropySource& Entropy, IStealthFaultSink* FaultSink)
{
	Close();
	uint8 Secret[16] = {};
	uint8 Nonce[8] = {};
	if (!Entropy.Fill(TArrayView<uint8>(Secret, 16)) || !Entropy.Fill(TArrayView<uint8>(Nonce, 8)))
	{
		FMemory::Memzero(Secret, sizeof(Secret));
		if (FaultSink != nullptr)
		{
			FaultSink->Report(EStealthFault::EntropyFailed, FName(TEXT("StealthServerSeeds")), 0);
		}
		return false;
	}
	FMemory::Memcpy(ServerSecret, Secret, sizeof(ServerSecret));
	MatchNonce = 0;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		MatchNonce |= static_cast<uint64>(Nonce[Index]) << (8 * Index);
	}
	FMemory::Memzero(Secret, sizeof(Secret));
	bOpen = true;
	return true;
}

void FStealthServerSeeds::Close()
{
	FMemory::Memzero(ServerSecret, sizeof(ServerSecret));
	MatchNonce = 0;
	bOpen = false;
}

uint64 FStealthServerSeeds::GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const
{
	if (!bOpen)
	{
		return 0;
	}
	TArray<uint8, TInlineAllocator<64>> Message;
	Message.Add(0x02);
	StealthRandom::AppendLittleEndian(Message, MatchNonce, 8);
	StealthRandom::AppendLittleEndian(Message, RoundIndex, 4);
	StealthRandom::AppendLittleEndian(Message, static_cast<uint32>(Domain), 4);
	StealthRandom::AppendLittleEndian(Message, Attempt, 4);
	return StealthRandom::SipHash24(ServerSecret, Message);
}
