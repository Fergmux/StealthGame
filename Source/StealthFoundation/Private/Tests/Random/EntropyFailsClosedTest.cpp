// Story st-009 (production/epics/session-travel/story-009-os-entropy-and-session-secret.md): OpenSession draws
// ServerSecret and MatchNonce from OS entropy and fails closed (ADR-0006 section 2, Guideline 8).
//   - A failing IEntropySource: OpenSession returns CouldNotHost (the user-facing "could not host" text exists), the
//     EntropyFailed fault reaches the sink, nothing is held (sub-seeds and seeds answer 0), and no weaker source is
//     substituted (bytes the failing source would still give are never read after the failure).
//   - A deterministic source (non-Shipping): ServerSecret (16 bytes) and MatchNonce (8 bytes) are exactly the
//     injected bytes, proven by recomputing F5 sub-seeds from those bytes with SipHash-2-4.
//   - -StealthServerSecret=<32 hex> parses to the secret plus a derived nonce; malformed values are refused.
//   - The production source FOsEntropySource fills from BCryptGenRandom (two fills differ, not all zero), and its
//     source text uses BCRYPT_USE_SYSTEM_PREFERRED_RNG checked with BCRYPT_SUCCESS; no production entropy is a GUID.
// Secrets are compared inside the test only and never printed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Random/StealthSipHash.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"

namespace EntropyFailsClosedTestData
{
	/** A source that fails on its first Fill and counts calls. */
	class FFailingEntropy final : public IEntropySource
	{
	public:
		int32 Calls = 0;
		virtual bool Fill(TArrayView<uint8> Out) override
		{
			++Calls;
			return false;
		}
	};

	uint64 F5SubSeed(const uint8 (&Secret)[16], uint64 Nonce, uint32 Round, uint32 Domain, uint32 Attempt)
	{
		TArray<uint8, TInlineAllocator<64>> Message;
		Message.Add(0x02);
		StealthRandom::AppendLittleEndian(Message, Nonce, 8);
		StealthRandom::AppendLittleEndian(Message, Round, 4);
		StealthRandom::AppendLittleEndian(Message, Domain, 4);
		StealthRandom::AppendLittleEndian(Message, Attempt, 4);
		return StealthRandom::SipHash24(Secret, Message);
	}

	bool LoadProjectFile(const TCHAR* Relative, FString& Out)
	{
		return FFileHelper::LoadFileToString(Out, *FPaths::Combine(FPaths::ProjectDir(), Relative));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRandomEntropyFailsClosedTest, "StealthGame.Random.EntropyFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRandomEntropyFailsClosedTest::RunTest(const FString& Parameters)
{
	using namespace EntropyFailsClosedTestData;

	// 1. Fail closed.
	{
		FStealthRecordingFaultSink Faults;
		FFailingEntropy Failing;
		FStealthServerSeeds Seeds;
		const EStealthSessionOpenResult Result = Seeds.OpenSession(Failing, &Faults);
		TestTrue(TEXT("A failing entropy source: OpenSession returns CouldNotHost"), Result == EStealthSessionOpenResult::CouldNotHost);
		TestFalse(TEXT("The session does not open"), Seeds.IsOpen());
		TestTrue(TEXT("The failure reaches the fault sink (EntropyFailed)"), Faults.GetRecords().Num() == 1 && Faults.GetRecords()[0].Fault == EStealthFault::EntropyFailed);
		TestEqual(TEXT("No weaker source: the failing source was asked once and nothing else filled the secret"), Failing.Calls, 1);
		TestEqual(TEXT("No secret held: GetServerSubSeed answers 0"), Seeds.GetServerSubSeed(ESeedDomain::SpawnOrder, 0), 0ull);
		TestEqual(TEXT("No secret held: no round seed"), Seeds.GetRoundSeedCandidate(1, 0), 0);
		TestFalse(TEXT("The user-facing 'could not host' text exists"), FStealthServerSeeds::GetCouldNotHostText().IsEmpty());
	}

#if !UE_BUILD_SHIPPING
	// 2. Deterministic injection: the secret and nonce are exactly the injected bytes.
	{
		uint8 Secret[16];
		for (int32 Index = 0; Index < 16; ++Index)
		{
			Secret[Index] = static_cast<uint8>(0xA0 + Index);
		}
		const uint64 Nonce = 0x1122334455667788ull;
		TArray<uint8> Bytes(Secret, 16);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Bytes.Add(static_cast<uint8>((Nonce >> (8 * Index)) & 0xFF));
		}
		FDeterministicEntropySource Injected(Bytes);
		FStealthServerSeeds Seeds;
		TestTrue(TEXT("Deterministic source opens"), Seeds.OpenSession(Injected, nullptr) == EStealthSessionOpenResult::Opened);
		Seeds.SetRoundIndex(4);
		bool bExact = true;
		for (uint32 Domain = 1; Domain <= GStealthSeedDomainMax; ++Domain)
		{
			bExact &= Seeds.GetServerSubSeed(static_cast<ESeedDomain>(Domain), 3) == F5SubSeed(Secret, Nonce, 4, Domain, 3);
		}
		TestTrue(TEXT("ServerSecret and MatchNonce are exactly the injected bytes (every domain's F5 sub-seed recomputed)"), bExact);
		FDeterministicEntropySource Short(TConstArrayView<uint8>(Secret, 16));
		FStealthServerSeeds ShortSeeds;
		TestTrue(TEXT("A deterministic source that runs out fails closed too"), ShortSeeds.OpenSession(Short, nullptr) == EStealthSessionOpenResult::CouldNotHost);
	}

	// 3. The command-line override.
	{
		TArray<uint8> Out;
		TestTrue(TEXT("-StealthServerSecret=<32 hex> parses"), FDeterministicEntropySource::FromCommandLine(TEXT("-game -StealthServerSecret=000102030405060708090a0b0c0d0e0f -log"), Out));
		bool bSecretBytes = Out.Num() == 24;
		for (int32 Index = 0; Index < 16 && bSecretBytes; ++Index)
		{
			bSecretBytes &= Out[Index] == Index;
		}
		TestTrue(TEXT("The parsed bytes are the secret then an 8-byte nonce"), bSecretBytes);
		TestFalse(TEXT("A short value is refused"), FDeterministicEntropySource::FromCommandLine(TEXT("-StealthServerSecret=0001"), Out));
		TestFalse(TEXT("A non-hex value is refused"), FDeterministicEntropySource::FromCommandLine(TEXT("-StealthServerSecret=zz0102030405060708090a0b0c0d0e0f"), Out));
		TestFalse(TEXT("An absent switch is refused"), FDeterministicEntropySource::FromCommandLine(TEXT("-game"), Out));
		TestTrue(TEXT("The AC-22 sentinel carries the test-seam prefix"), FString(FDeterministicEntropySource::Sentinel()).StartsWith(TEXT("STEALTH_TEST_SEAM_SENTINEL_")));
	}
#endif

	// 4. The production source.
	{
		FOsEntropySource Os;
		uint8 A[32] = {};
		uint8 B[32] = {};
		TestTrue(TEXT("FOsEntropySource fills 32 bytes"), Os.Fill(TArrayView<uint8>(A, 32)));
		TestTrue(TEXT("FOsEntropySource fills another 32 bytes"), Os.Fill(TArrayView<uint8>(B, 32)));
		bool bAllZero = true;
		for (const uint8 Byte : A)
		{
			bAllZero &= Byte == 0;
		}
		TestFalse(TEXT("The OS bytes are not all zero"), bAllZero);
		TestTrue(TEXT("Two fills differ"), FMemory::Memcmp(A, B, 32) != 0);
		FStealthServerSeeds Seeds;
		TestTrue(TEXT("OpenSession on the OS source opens"), Seeds.OpenSession(Os, nullptr) == EStealthSessionOpenResult::Opened);

		FString Source;
		if (TestTrue(TEXT("StealthServerSeeds.cpp is readable"), LoadProjectFile(TEXT("Source/StealthFoundation/Private/Random/StealthServerSeeds.cpp"), Source)))
		{
			TestTrue(TEXT("Uses BCryptGenRandom"), Source.Contains(TEXT("BCryptGenRandom(nullptr"), ESearchCase::CaseSensitive));
			TestTrue(TEXT("Uses BCRYPT_USE_SYSTEM_PREFERRED_RNG"), Source.Contains(TEXT("BCRYPT_USE_SYSTEM_PREFERRED_RNG"), ESearchCase::CaseSensitive));
			TestTrue(TEXT("Checks BCRYPT_SUCCESS"), Source.Contains(TEXT("BCRYPT_SUCCESS(Status)"), ESearchCase::CaseSensitive));
			TestTrue(TEXT("Includes bcrypt.h the engine's way"), Source.Contains(TEXT("Windows/AllowWindowsPlatformTypes.h"), ESearchCase::CaseSensitive));
		}
		for (const TCHAR* Consumer : { TEXT("Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp"), TEXT("Source/StealthFoundation/Private/Session/StealthSessionService.cpp") })
		{
			FString Text;
			if (TestTrue(FString::Printf(TEXT("%s is readable"), Consumer), LoadProjectFile(Consumer, Text)))
			{
				TestFalse(FString::Printf(TEXT("%s has no GUID entropy source"), Consumer), Text.Contains(TEXT("EntropySource final"), ESearchCase::CaseSensitive) || Text.Contains(TEXT("FGuidEntropy"), ESearchCase::CaseSensitive));
				TestTrue(FString::Printf(TEXT("%s opens the secret from FOsEntropySource"), Consumer), Text.Contains(TEXT("FOsEntropySource"), ESearchCase::CaseSensitive));
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
