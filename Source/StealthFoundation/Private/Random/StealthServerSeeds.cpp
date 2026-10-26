#include "Server/Random/StealthServerSeeds.h"

#include "HAL/PlatformTLS.h"
#include "Misc/Parse.h"
#include "Server/Random/StealthSipHash.h"
#include "Server/StealthFaultSink.h"
#include "View/Random/StealthMix32.h"

#if PLATFORM_WINDOWS
// ADR-0006 section 2: the engine's way of including a Windows SDK header (WindowsConfigRules.cpp pattern).
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "StealthServerSeeds"

namespace StealthServerSeedsPrivate
{
	const FName FaultSource(TEXT("StealthServerSeeds"));

	/** FPublicOnlyScope depth of the calling thread (thread_local: covers a Generator stage on a worker thread). */
	thread_local int32 GPublicOnlyDepth = 0;

	/** The F5 modulus of the round seed: 2^31 - 1. */
	constexpr uint64 SeedModulus = 0x7FFFFFFFull;

	const TCHAR* MisuseName(EStealthSeedMisuse Misuse)
	{
		switch (Misuse)
		{
		case EStealthSeedMisuse::NoAuthority: return TEXT("no authority");
		case EStealthSeedMisuse::NotGameThread: return TEXT("not the game thread");
		case EStealthSeedMisuse::InsidePublicOnlyScope: return TEXT("inside FPublicOnlyScope");
		case EStealthSeedMisuse::NonLayoutPublicDomain: return TEXT("PublicSubSeed refuses every domain except Layout");
		default: return TEXT("?");
		}
	}
}

// ---- Entropy ----------------------------------------------------------------------------------------------------

bool FOsEntropySource::Fill(TArrayView<uint8> Out)
{
	if (Out.Num() == 0)
	{
		return true;
	}
#if PLATFORM_WINDOWS
	const NTSTATUS Status = BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(Out.GetData()), static_cast<ULONG>(Out.Num()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	return BCRYPT_SUCCESS(Status);
#else
	// ADR-0024 (Linux getrandom) is not implemented yet: fail closed rather than fall back to a weaker source.
	return false;
#endif
}

#if !UE_BUILD_SHIPPING
bool FDeterministicEntropySource::Fill(TArrayView<uint8> Out)
{
	if (Cursor + Out.Num() > Bytes.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Out.Num(); ++Index)
	{
		Out[Index] = Bytes[Cursor++];
	}
	return true;
}

bool FDeterministicEntropySource::FromCommandLine(const TCHAR* CommandLine, TArray<uint8>& Out)
{
	Out.Reset();
	FString Hex;
	if (CommandLine == nullptr || !FParse::Value(CommandLine, TEXT("StealthServerSecret="), Hex) || Hex.Len() != 32)
	{
		return false;
	}
	uint8 Secret[16];
	for (int32 Index = 0; Index < 16; ++Index)
	{
		const TCHAR High = Hex[2 * Index];
		const TCHAR Low = Hex[2 * Index + 1];
		if (!FChar::IsHexDigit(High) || !FChar::IsHexDigit(Low))
		{
			return false;
		}
		Secret[Index] = static_cast<uint8>((FParse::HexDigit(High) << 4) | FParse::HexDigit(Low));
	}
	// The nonce follows from the secret under a fixed public key, so one switch reproduces a whole session.
	const uint64 Nonce = StealthRandom::SipHash24(0x4543414E4F4E5354ull /* "TSNONACE" */, 0ull, TConstArrayView<uint8>(Secret, 16));
	Out.Append(Secret, 16);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Out.Add(static_cast<uint8>((Nonce >> (8 * Index)) & 0xFF));
	}
	// Reference the sentinel so the linker keeps it in Development (the AC-22 scan's positive control).
	return Sentinel() != nullptr;
}

const TCHAR* FDeterministicEntropySource::Sentinel()
{
	static const TCHAR* volatile Marker = TEXT("STEALTH_TEST_SEAM_SENTINEL_SERVERSECRET_OVERRIDE");
	return Marker;
}
#endif

// ---- Expected-error hook ----------------------------------------------------------------------------------------

#if WITH_STEALTH_TEST_SEAMS
namespace StealthServerSeedsPrivate
{
	std::atomic<FStealthSeedMisuseExpectScope*> GExpectScope{ nullptr };
}

FStealthSeedMisuseExpectScope::FStealthSeedMisuseExpectScope()
{
	for (std::atomic<int32>& Slot : Counts)
	{
		Slot.store(0);
	}
	Previous = StealthServerSeedsPrivate::GExpectScope.exchange(this);
}

FStealthSeedMisuseExpectScope::~FStealthSeedMisuseExpectScope()
{
	StealthServerSeedsPrivate::GExpectScope.store(Previous);
}

int32 FStealthSeedMisuseExpectScope::Count(EStealthSeedMisuse Misuse) const
{
	const int32 Index = static_cast<int32>(Misuse);
	return Index >= 0 && Index < 8 ? Counts[Index].load() : 0;
}

int32 FStealthSeedMisuseExpectScope::Total() const
{
	int32 Sum = 0;
	for (const std::atomic<int32>& Slot : Counts)
	{
		Sum += Slot.load();
	}
	return Sum;
}

bool FStealthSeedMisuseExpectScope::Consume(EStealthSeedMisuse Misuse)
{
	FStealthSeedMisuseExpectScope* Scope = StealthServerSeedsPrivate::GExpectScope.load();
	const int32 Index = static_cast<int32>(Misuse);
	if (Scope == nullptr || Index < 0 || Index >= 8)
	{
		return false;
	}
	Scope->Counts[Index].fetch_add(1);
	return true;
}
#endif

// ---- FPublicOnlyScope -------------------------------------------------------------------------------------------

FPublicOnlyScope::FPublicOnlyScope()
{
	++StealthServerSeedsPrivate::GPublicOnlyDepth;
}

FPublicOnlyScope::~FPublicOnlyScope()
{
	--StealthServerSeedsPrivate::GPublicOnlyDepth;
}

bool FPublicOnlyScope::IsActive()
{
	return StealthServerSeedsPrivate::GPublicOnlyDepth > 0;
}

// ---- FStealthServerSeeds ----------------------------------------------------------------------------------------

bool FStealthServerSeeds::Open(IEntropySource& Entropy, IStealthFaultSink* InFaultSink)
{
	Close();
	if (InFaultSink != nullptr)
	{
		FaultSink = InFaultSink;
	}
	uint8 Secret[16] = {};
	uint8 Nonce[8] = {};
	if (!Entropy.Fill(TArrayView<uint8>(Secret, 16)) || !Entropy.Fill(TArrayView<uint8>(Nonce, 8)))
	{
		FMemory::Memzero(Secret, sizeof(Secret));
		FMemory::Memzero(Nonce, sizeof(Nonce));
		if (FaultSink != nullptr)
		{
			FaultSink->Report(EStealthFault::EntropyFailed, StealthServerSeedsPrivate::FaultSource, 0);
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
	FMemory::Memzero(Nonce, sizeof(Nonce));
	bOpen = true;
	return true;
}

EStealthSessionOpenResult FStealthServerSeeds::OpenSession(IEntropySource& Entropy, IStealthFaultSink* InFaultSink)
{
	return Open(Entropy, InFaultSink) ? EStealthSessionOpenResult::Opened : EStealthSessionOpenResult::CouldNotHost;
}

FText FStealthServerSeeds::GetCouldNotHostText()
{
	return LOCTEXT("CouldNotHost", "Could not host: the game could not start a secure session on this machine.");
}

void FStealthServerSeeds::Close()
{
	FMemory::Memzero(ServerSecret, sizeof(ServerSecret));
	MatchNonce = 0;
	bOpen = false;
}

void FStealthServerSeeds::ReportMisuse(EStealthSeedMisuse Misuse, ESeedDomain Domain) const
{
	// Plain branch in every configuration (check/ensure report nothing in Shipping): the fault sink first.
	if (FaultSink != nullptr)
	{
		FaultSink->Report(EStealthFault::SeedMisuse, StealthServerSeedsPrivate::FaultSource,
			(static_cast<int64>(Misuse) << 32) | static_cast<int64>(static_cast<uint32>(Domain)));
	}
#if WITH_STEALTH_TEST_SEAMS
	if (FStealthSeedMisuseExpectScope::Consume(Misuse))
	{
		return; // a test expects this assert (the expected-error hook)
	}
#endif
#if !UE_BUILD_SHIPPING
	// The R-G2.2 "server assert": tests see it through the expected-error hook. The domain only, never a secret.
	ensureAlwaysMsgf(false, TEXT("GetServerSubSeed/PublicSubSeed misuse (%s) for seed domain %u"),
		StealthServerSeedsPrivate::MisuseName(Misuse), static_cast<uint32>(Domain));
#endif
}

bool FStealthServerSeeds::RefuseSecretCall(ESeedDomain Domain) const
{
	if (AuthorityQuery && !AuthorityQuery())
	{
		ReportMisuse(EStealthSeedMisuse::NoAuthority, Domain);
		return true;
	}
	if (!IsInGameThread())
	{
		ReportMisuse(EStealthSeedMisuse::NotGameThread, Domain);
		return true;
	}
	if (FPublicOnlyScope::IsActive())
	{
		ReportMisuse(EStealthSeedMisuse::InsidePublicOnlyScope, Domain);
		return true;
	}
	return false;
}

uint64 FStealthServerSeeds::GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const
{
	if (RefuseSecretCall(Domain) || !bOpen)
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

int32 FStealthServerSeeds::GetRoundSeedCandidate(uint32 InRoundIndex, uint32 Draw) const
{
	if (RefuseSecretCall(ESeedDomain::Layout) || !bOpen)
	{
		return 0;
	}
	TArray<uint8, TInlineAllocator<64>> Message;
	Message.Add(0x01);
	StealthRandom::AppendLittleEndian(Message, MatchNonce, 8);
	StealthRandom::AppendLittleEndian(Message, InRoundIndex, 4);
	StealthRandom::AppendLittleEndian(Message, Draw, 4);
	const uint64 Hash = StealthRandom::SipHash24(ServerSecret, Message);
	return static_cast<int32>((Hash % StealthServerSeedsPrivate::SeedModulus) + 1u);
}

uint32 FStealthServerSeeds::PublicSubSeed(int32 Seed, ESeedDomain Domain, uint32 Attempt) const
{
	if (Domain != ESeedDomain::Layout)
	{
		ReportMisuse(EStealthSeedMisuse::NonLayoutPublicDomain, Domain);
		return 0;
	}
	return StealthRandom::Mix32(static_cast<uint32>(Seed), static_cast<uint32>(Domain), Attempt);
}

#undef LOCTEXT_NAMESPACE
