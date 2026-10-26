// Story st-006 (production/epics/session-travel/story-006-pcg32-int-stream-reference-vectors.md): FIntStream reproduces
// the reference PCG32 outputs (ADR-0006 section 6 and Validation Criteria). FIntStream(42, 54) is seeded as the
// reference pcg32_srandom_r(initstate 42, initseq 54) of pcg32-demo, whose first six 32-bit outputs are the published
// golden fixture. The implementation is integer-only (uint64/uint32 with defined wrap, logical shifts, the rotation
// written (32u - rot) & 31u, no float): the constexpr evaluation below proves the same arithmetic at compile time.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/Random/StealthIntStream.h"

namespace Pcg32VectorsTestData
{
	/** pcg32-demo, round 1, 32-bit outputs for pcg32_srandom_r(42, 54). */
	constexpr uint32 Expected[6] = { 0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu };

	/** The first output, evaluated by the compiler (integer-only arithmetic, no float, no platform dependence). */
	constexpr uint32 FirstAtCompileTime()
	{
		FIntStream Stream(42u, 54u);
		return Stream.NextU32();
	}
	static_assert(FirstAtCompileTime() == 0xa15c02b7u, "PCG32 reference vector 0 at compile time");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRandomPcg32VectorsTest, "StealthGame.Random.Pcg32Vectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRandomPcg32VectorsTest::RunTest(const FString& Parameters)
{
	using namespace Pcg32VectorsTestData;

	// Arrange: the reference seeding.
	FIntStream Stream(42u, 54u);

	// Act + assert: the published outputs, in order.
	for (int32 Index = 0; Index < 6; ++Index)
	{
		TestEqual(FString::Printf(TEXT("PCG32 reference output %d"), Index), Stream.NextU32(), Expected[Index]);
	}

	// Copies continue the same sequence (the state is plain integers); a different initseq is a different stream.
	FIntStream A(42u, 54u);
	A.NextU32();
	FIntStream B = A;
	TestEqual(TEXT("A copied stream continues identically"), A.NextU32(), B.NextU32());
	FIntStream Other(42u, 55u);
	TestNotEqual(TEXT("A different initseq selects a different stream"), Other.NextU32(), Expected[0]);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
