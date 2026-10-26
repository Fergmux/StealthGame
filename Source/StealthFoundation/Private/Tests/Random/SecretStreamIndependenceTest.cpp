// Story st-008 (production/epics/session-travel/story-008-secret-stream-counter-independence.md): FSecretStream
// counter draws are independent of interleaved draws (ADR-0006 section 5, CNB-R14). On a fixed sub-seed and tag:
//   - Draw(ordinal, decisionType, decisionIndex) values for one pawn ordinal are identical with and without
//     interleaved draws for other ordinals, other decision types and sequential NextU64 calls;
//   - the sequential mode (0x01) and the counter-tuple mode (0x02) never collide (equal counters, disjoint outputs,
//     and each output equals SipHash over its own mode byte);
//   - the stream tag enters the message after the mode byte while the key's second word is the one fixed public
//     constant KStreamConst (never the tag): each output is recomputed from SipHash24(SubSeed, KStreamConst, message).
// Sub-seed values are compared inside the test only and never printed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Random/StealthSecretStream.h"
#include "Server/Random/StealthSipHash.h"

namespace SecretStreamIndependenceTestData
{
	constexpr uint64 SubSeed = 0x0123456789ABCDEFull;
	const uint64 Tag = StealthRandom::Tag("CNB_TEST");
	const uint64 OtherTag = StealthRandom::Tag("CNB_OTHR");
	constexpr uint64 PawnOrdinal = 7;

	uint64 Expected(uint8 Mode, uint64 InTag, TConstArrayView<uint64> Words)
	{
		TArray<uint8, TInlineAllocator<64>> Message;
		Message.Add(Mode);
		StealthRandom::AppendLittleEndian(Message, InTag, 8);
		for (const uint64 Word : Words)
		{
			StealthRandom::AppendLittleEndian(Message, Word, 8);
		}
		return StealthRandom::SipHash24(SubSeed, FSecretStream::KStreamConst, Message);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRandomSecretStreamIndependenceTest, "StealthGame.Random.SecretStreamIndependence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRandomSecretStreamIndependenceTest::RunTest(const FString& Parameters)
{
	using namespace SecretStreamIndependenceTestData;

	// Arrange: one pawn's draws (3 decision types x 8 indices) taken alone.
	FSecretStream Alone(SubSeed, Tag);
	TArray<uint64> AloneDraws;
	for (uint64 Type = 0; Type < 3; ++Type)
	{
		for (uint64 Index = 0; Index < 8; ++Index)
		{
			const uint64 Tuple[3] = { PawnOrdinal, Type, Index };
			AloneDraws.Add(Alone.Draw(Tuple));
		}
	}

	// Act: the same draws with other pawns' draws, other decision types and sequential blocks interleaved.
	FSecretStream Busy(SubSeed, Tag);
	TArray<uint64> BusyDraws;
	for (uint64 Type = 0; Type < 3; ++Type)
	{
		for (uint64 Index = 0; Index < 8; ++Index)
		{
			for (uint64 Other = 0; Other < 5; ++Other)
			{
				const uint64 Noise[3] = { PawnOrdinal + 1 + Other, (Type + Other) % 4, Index * 3 + Other };
				Busy.Draw(Noise);
				Busy.NextU64();
			}
			const uint64 Tuple[3] = { PawnOrdinal, Type, Index };
			BusyDraws.Add(Busy.Draw(Tuple));
		}
	}

	// Assert: identical, and each equals the documented message under (SubSeed, KStreamConst).
	TestTrue(TEXT("A pawn's counter draws are identical with and without interleaved draws"), AloneDraws == BusyDraws);
	bool bFormula = true;
	int32 Cursor = 0;
	for (uint64 Type = 0; Type < 3; ++Type)
	{
		for (uint64 Index = 0; Index < 8; ++Index)
		{
			const uint64 Words[3] = { PawnOrdinal, Type, Index };
			bFormula &= AloneDraws[Cursor++] == Expected(0x02, Tag, Words);
		}
	}
	TestTrue(TEXT("Draw = SipHash((SubSeed, KStreamConst), 0x02 | u64le(Tag) | tuple)"), bFormula);

	// Modes never collide: Block(i) (0x01) and Draw({i}) (0x02) over the same counters.
	FSecretStream Sequential(SubSeed, Tag);
	TSet<uint64> BlockOutputs;
	bool bBlockFormula = true;
	for (uint64 Counter = 0; Counter < 2000; ++Counter)
	{
		const uint64 Block = Sequential.NextU64();
		const uint64 Words[1] = { Counter };
		bBlockFormula &= Block == Expected(0x01, Tag, Words);
		BlockOutputs.Add(Block);
	}
	TestTrue(TEXT("Block(i) = SipHash((SubSeed, KStreamConst), 0x01 | u64le(Tag) | u64le(i))"), bBlockFormula);
	int32 Collisions = 0;
	const FSecretStream Counters(SubSeed, Tag);
	for (uint64 Counter = 0; Counter < 2000; ++Counter)
	{
		const uint64 Tuple[1] = { Counter };
		Collisions += BlockOutputs.Contains(Counters.Draw(Tuple)) ? 1 : 0;
	}
	TestEqual(TEXT("Sequential and counter-tuple outputs over equal counters never collide"), Collisions, 0);

	// The tag is in the message, not the key: another tag on the same sub-seed is another stream, same key words.
	FSecretStream Tagged(SubSeed, OtherTag);
	const uint64 Words0[1] = { 0 };
	TestTrue(TEXT("Another tag: the message changes (0x01 | u64le(OtherTag) | 0) under the same key"), Tagged.NextU64() == Expected(0x01, OtherTag, Words0));
	FSecretStream First(SubSeed, Tag);
	TestTrue(TEXT("Two tags on one sub-seed give different streams"), First.NextU64() != Expected(0x01, OtherTag, Words0));
	TestEqual(TEXT("KStreamConst is the public constant Tag(\"SGSTREAM\")"), FSecretStream::KStreamConst, StealthRandom::Tag("SGSTREAM"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
