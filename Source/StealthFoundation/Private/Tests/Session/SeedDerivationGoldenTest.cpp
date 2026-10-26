// Story st-010 (production/epics/session-travel/story-010-seed-derivation-golden-vectors.md): round seed and sub-seed
// derivation match Session F5 (GDD AC 38 first part, SeedDerivationGolden [U]; ADR-0006 sections 3 and 6).
//   - Mix32(123456789, 1, 0) = 2107646860, (1, 1) = 523860287, (2, 0) = 313478722 (also static_asserted in
//     StealthMix32.h), with uint32 operands and logical shifts; 0 maps to 1.
//   - Over a fake 99-round match on a fixed test secret (IEntropySource hook, no OS entropy, no wall clock), Seed stays
//     in [1, 2^31 - 1] and never repeats; the seed is F5's (SipHash(Secret, 0x01 | Nonce | Round | Draw) mod
//     (2^31 - 1)) + 1, recomputed here; a used seed and a golden seed are redrawn with Draw + 1.
//   - ServerSubSeed is unchanged when only Seed changes.
//   - PublicSubSeed refuses every domain except Layout (0, a SeedMisuse fault and an ensure) and equals Mix32 for Layout.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Random/StealthRoundSeedDrawer.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Random/StealthSipHash.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "View/Random/StealthMix32.h"

namespace SeedDerivationGoldenTestData
{
	constexpr uint32 GoldenSeed = 123456789u;
	constexpr int32 MatchRounds = 99;

	/** The fixed test secret 10..1f and nonce bytes 20..27 (never OS entropy). */
	TArray<uint8> FixtureBytes()
	{
		TArray<uint8> Bytes;
		for (int32 Index = 0; Index < 24; ++Index)
		{
			Bytes.Add(static_cast<uint8>(0x10 + Index));
		}
		return Bytes;
	}

	int32 F5Seed(const TArray<uint8>& Bytes, uint32 Round, uint32 Draw)
	{
		uint8 Secret[16];
		for (int32 Index = 0; Index < 16; ++Index)
		{
			Secret[Index] = Bytes[Index];
		}
		TArray<uint8, TInlineAllocator<64>> Message;
		Message.Add(0x01);
		for (int32 Index = 16; Index < 24; ++Index)
		{
			Message.Add(Bytes[Index]); // MatchNonce, little-endian as injected
		}
		StealthRandom::AppendLittleEndian(Message, Round, 4);
		StealthRandom::AppendLittleEndian(Message, Draw, 4);
		return static_cast<int32>((StealthRandom::SipHash24(Secret, Message) % 0x7FFFFFFFull) + 1u);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionSeedDerivationGoldenTest, "StealthGame.Session.SeedDerivationGolden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionSeedDerivationGoldenTest::RunTest(const FString& Parameters)
{
	using namespace SeedDerivationGoldenTestData;

	// Mix32 golden vectors (runtime as well as static_assert).
	TestEqual(TEXT("Mix32(Seed, 1, 0) = 2107646860"), StealthRandom::Mix32(GoldenSeed, 1u, 0u), 2107646860u);
	TestEqual(TEXT("Mix32(Seed, 1, 1) = 523860287"), StealthRandom::Mix32(GoldenSeed, 1u, 1u), 523860287u);
	TestEqual(TEXT("Mix32(Seed, 2, 0) = 313478722"), StealthRandom::Mix32(GoldenSeed, 2u, 0u), 313478722u);
	TestTrue(TEXT("Mix32 results are 31-bit and never 0"), StealthRandom::Mix32(0u, 0u, 0u) >= 1u && StealthRandom::Mix32(0xFFFFFFFFu, 7u, 9u) <= 0x7FFFFFFFu);

	// Arrange: a fixed secret through the IEntropySource hook.
	const TArray<uint8> Bytes = FixtureBytes();
	FDeterministicEntropySource Entropy(Bytes);
	FStealthRecordingFaultSink Faults;
	FStealthServerSeeds Seeds;
	if (!TestTrue(TEXT("Fixture secret opens"), Seeds.Open(Entropy, &Faults)))
	{
		return false;
	}
	FStealthRoundSeedDrawer Drawer(Seeds);

	// Act: a fake 99-round match.
	TSet<int32> Seen;
	bool bInRange = true;
	bool bFormula = true;
	const uint64 SubSeedBefore = Seeds.GetServerSubSeed(ESeedDomain::ObjectiveSites, 2);
	for (uint32 Round = 1; Round <= static_cast<uint32>(MatchRounds); ++Round)
	{
		const int32 Seed = Drawer.DrawRoundSeed(Round);
		bInRange &= Seed >= 1;
		bFormula &= Seed == F5Seed(Bytes, Round, 0) || Seen.Contains(F5Seed(Bytes, Round, 0));
		Seen.Add(Seed);
	}

	// Assert.
	TestTrue(TEXT("Every seed is in [1, 2^31 - 1]"), bInRange);
	TestEqual(TEXT("99 rounds draw 99 distinct seeds (never repeats)"), Seen.Num(), MatchRounds);
	TestTrue(TEXT("Each seed is F5's (SipHash(Secret, 0x01 | Nonce | Round | Draw) mod (2^31 - 1)) + 1"), bFormula);
	TestEqual(TEXT("ServerSubSeed is unchanged when only Seed changes"), Seeds.GetServerSubSeed(ESeedDomain::ObjectiveSites, 2), SubSeedBefore);

	// A used seed is redrawn: a second start of round 1 (an aborted start) takes Draw 1.
	const int32 Again = Drawer.DrawRoundSeed(1);
	TestEqual(TEXT("A repeated round index redraws with Draw + 1"), Again, F5Seed(Bytes, 1, 1));
	TestFalse(TEXT("The redrawn seed was not used before"), Seen.Contains(Again));

	// A golden seed is never drawn for a live round.
	FStealthRoundSeedDrawer GoldenDrawer(Seeds);
	const int32 Golden[] = { F5Seed(Bytes, 200, 0), F5Seed(Bytes, 200, 1) };
	GoldenDrawer.SetGoldenSeeds(Golden);
	TestEqual(TEXT("Seeds in the golden regression set are skipped (Draw 0 and 1 golden -> Draw 2)"), GoldenDrawer.DrawRoundSeed(200), F5Seed(Bytes, 200, 2));

	// PublicSubSeed: Layout only.
	TestEqual(TEXT("PublicSubSeed(Layout, 0) = Mix32(Seed, 1, 0)"), Seeds.PublicSubSeed(static_cast<int32>(GoldenSeed), ESeedDomain::Layout, 0), 2107646860u);
	FStealthSeedMisuseExpectScope ExpectedAsserts; // the expected-error hook: the misuse assert is counted, not fired
	int32 Refused = 0;
	for (uint32 Domain = 2; Domain <= GStealthSeedDomainMax; ++Domain)
	{
		Refused += Seeds.PublicSubSeed(static_cast<int32>(GoldenSeed), static_cast<ESeedDomain>(Domain), 0) == 0u ? 1 : 0;
	}
	TestEqual(TEXT("PublicSubSeed refuses (returns 0 for) every non-Layout domain"), Refused, static_cast<int32>(GStealthSeedDomainMax) - 1);
	TestEqual(TEXT("Each refusal raised the misuse assert"), ExpectedAsserts.Count(EStealthSeedMisuse::NonLayoutPublicDomain), static_cast<int32>(GStealthSeedDomainMax) - 1);
	TestEqual(TEXT("Each refusal reached the fault sink as SeedMisuse"), Faults.GetRecords().FilterByPredicate([](const FStealthFaultRecord& R) { return R.Fault == EStealthFault::SeedMisuse; }).Num(), static_cast<int32>(GStealthSeedDomainMax) - 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
