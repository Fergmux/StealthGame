// Story st-005 (production/epics/session-travel/story-005-siphash24-reference-vectors.md): SipHash-2-4 passes the
// reference implementation's published test vectors (ADR-0006 section 3 and Validation Criteria). Key 00..0f, messages
// of length 0..63 bytes (00, 00 01, ..., 00..3e); expected outputs are vectors_sip64 of the reference vectors.h, read
// as little-endian uint64 (vector 0 is bytes 31 0e 0e dd 47 db 6f 72). The fixture is committed here; changing the
// primitive replaces these vectors in the same change (ADR-0006 Guideline 7). Also checks the 16-byte-key overload
// against the two-word overload, and that messages are built byte by byte (AppendLittleEndian).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Random/StealthSipHash.h"

namespace SipHashVectorsTestData
{
	/** vectors_sip64[0..63] (Aumasson and Bernstein reference implementation), as little-endian uint64. */
	constexpr uint64 Expected[64] = {
		0x726fdb47dd0e0e31ull, 0x74f839c593dc67fdull, 0x0d6c8009d9a94f5aull, 0x85676696d7fb7e2dull,
		0xcf2794e0277187b7ull, 0x18765564cd99a68dull, 0xcbc9466e58fee3ceull, 0xab0200f58b01d137ull,
		0x93f5f5799a932462ull, 0x9e0082df0ba9e4b0ull, 0x7a5dbbc594ddb9f3ull, 0xf4b32f46226bada7ull,
		0x751e8fbc860ee5fbull, 0x14ea5627c0843d90ull, 0xf723ca908e7af2eeull, 0xa129ca6149be45e5ull,
		0x3f2acc7f57c29bdbull, 0x699ae9f52cbe4794ull, 0x4bc1b3f0968dd39cull, 0xbb6dc91da77961bdull,
		0xbed65cf21aa2ee98ull, 0xd0f2cbb02e3b67c7ull, 0x93536795e3a33e88ull, 0xa80c038ccd5ccec8ull,
		0xb8ad50c6f649af94ull, 0xbce192de8a85b8eaull, 0x17d835b85bbb15f3ull, 0x2f2e6163076bcfadull,
		0xde4daaaca71dc9a5ull, 0xa6a2506687956571ull, 0xad87a3535c49ef28ull, 0x32d892fad841c342ull,
		0x7127512f72f27cceull, 0xa7f32346f95978e3ull, 0x12e0b01abb051238ull, 0x15e034d40fa197aeull,
		0x314dffbe0815a3b4ull, 0x027990f029623981ull, 0xcadcd4e59ef40c4dull, 0x9abfd8766a33735cull,
		0x0e3ea96b5304a7d0ull, 0xad0c42d6fc585992ull, 0x187306c89bc215a9ull, 0xd4a60abcf3792b95ull,
		0xf935451de4f21df2ull, 0xa9538f0419755787ull, 0xdb9acddff56ca510ull, 0xd06c98cd5c0975ebull,
		0xe612a3cb9ecba951ull, 0xc766e62cfcadaf96ull, 0xee64435a9752fe72ull, 0xa192d576b245165aull,
		0x0a8787bf8ecb74b2ull, 0x81b3e73d20b49b6full, 0x7fa8220ba3b2eceaull, 0x245731c13ca42499ull,
		0xb78dbfaf3a8d83bdull, 0xea1ad565322a1a0bull, 0x60e61c23a3795013ull, 0x6606d7e446282b93ull,
		0x6ca4ecb15c5f91e1ull, 0x9f626da15c9625f3ull, 0xe51b38608ef25f57ull, 0x958a324ceb064572ull,
	};

	/** The SipHash paper's Appendix A example: the 15-byte message 00..0e under key 00..0f. */
	constexpr uint64 PaperAppendixA = 0xa129ca6149be45e5ull;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRandomSipHashVectorsTest, "StealthGame.Random.SipHashVectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRandomSipHashVectorsTest::RunTest(const FString& Parameters)
{
	using namespace SipHashVectorsTestData;

	// Arrange: key 00..0f and the message buffer 00..3e.
	uint8 Key[16];
	for (int32 Index = 0; Index < 16; ++Index)
	{
		Key[Index] = static_cast<uint8>(Index);
	}
	uint8 Message[64];
	for (int32 Index = 0; Index < 64; ++Index)
	{
		Message[Index] = static_cast<uint8>(Index);
	}

	// Act + assert: every prefix length 0..63.
	int32 Matched = 0;
	for (int32 Length = 0; Length < 64; ++Length)
	{
		const uint64 Hash = StealthRandom::SipHash24(Key, TConstArrayView<uint8>(Message, Length));
		if (TestEqual(FString::Printf(TEXT("SipHash-2-4 reference vector %d (message length %d)"), Length, Length), Hash, Expected[Length]))
		{
			++Matched;
		}
	}
	TestEqual(TEXT("All 64 reference vectors match"), Matched, 64);
	TestEqual(TEXT("Paper Appendix A vector (15-byte message)"), StealthRandom::SipHash24(Key, TConstArrayView<uint8>(Message, 15)), PaperAppendixA);

	// The two-word key overload is the same function (K0 = key bytes 0-7 little-endian).
	TestEqual(TEXT("Two-word key overload matches the 16-byte key overload"),
		StealthRandom::SipHash24(0x0706050403020100ull, 0x0f0e0d0c0b0a0908ull, TConstArrayView<uint8>(Message, 37)), Expected[37]);

	// Messages are built byte by byte, little-endian (never by memcpy of a struct).
	TArray<uint8, TInlineAllocator<64>> Built;
	StealthRandom::AppendLittleEndian(Built, 0x0706050403020100ull, 8);
	StealthRandom::AppendLittleEndian(Built, 0x0b0a0908u, 4);
	TestEqual(TEXT("AppendLittleEndian builds 00..0b"), StealthRandom::SipHash24(Key, Built), Expected[12]);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
