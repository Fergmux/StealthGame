// Story rmf-002 (production/epics/round-match-framework/story-002-test-seam-fixed-server-secret.md):
// GDD test seam 2. A fixed ServerSecret, injected through IEntropySource (ADR-0006 Guideline 8), gives the same
// sub-seeds on every derivation, and a different fixture secret gives different sub-seeds for every purpose.
// Sub-seed values are compared inside the test only and never printed (a failing message names the purpose).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Random/StealthSipHash.h"
#include "Server/Testing/StealthQueueTestRig.h"

namespace FixedSecretSeamTestData
{
	/** Attempts compared per domain (0, a layout attempt, the tick of a 3 s hold, a large tick). */
	constexpr uint32 Attempts[] = { 0, 1, 180, 4000000 };
	constexpr uint32 RoundIndex = 2;

	/** SipHash-2-4 reference implementation vectors for key 00..0f (messages: empty, 00): primitive sanity only. */
	constexpr uint64 SipHashVectorEmpty = 0x726fdb47dd0e0e31ull;
	constexpr uint64 SipHashVectorOneByte = 0x74f839c593dc67fdull;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundFixedSecretSeamTest, "StealthGame.Round.FixedSecretSeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundFixedSecretSeamTest::RunTest(const FString& Parameters)
{
	using namespace FixedSecretSeamTestData;
	FStealthRecordingFaultSink Faults;

	// The derivation runs on the real SipHash-2-4: check it against the reference vectors first.
	{
		uint8 Key[16];
		for (int32 Index = 0; Index < 16; ++Index)
		{
			Key[Index] = static_cast<uint8>(Index);
		}
		const uint8 OneByte[1] = { 0 };
		TestTrue(TEXT("SipHash-2-4 reference vector (empty message)"), StealthRandom::SipHash24(Key, TConstArrayView<uint8>()) == SipHashVectorEmpty);
		TestTrue(TEXT("SipHash-2-4 reference vector (message 00)"), StealthRandom::SipHash24(Key, TConstArrayView<uint8>(OneByte, 1)) == SipHashVectorOneByte);
	}

	// Arrange: two independent derivations from S1 and one from S2, each opened through a fake entropy source.
	FStealthFakeEntropySource EntropyS1a(RoundTestFixtures::SecretS1);
	FStealthFakeEntropySource EntropyS1b(RoundTestFixtures::SecretS1);
	FStealthFakeEntropySource EntropyS2(RoundTestFixtures::SecretS2);
	FStealthServerSeeds SeedsS1a, SeedsS1b, SeedsS2;
	TestTrue(TEXT("S1 (first) opens"), SeedsS1a.Open(EntropyS1a, &Faults));
	TestTrue(TEXT("S1 (second) opens"), SeedsS1b.Open(EntropyS1b, &Faults));
	TestTrue(TEXT("S2 opens"), SeedsS2.Open(EntropyS2, &Faults));
	SeedsS1a.SetRoundIndex(RoundIndex);
	SeedsS1b.SetRoundIndex(RoundIndex);
	SeedsS2.SetRoundIndex(RoundIndex);

	// Act + assert: every domain (Round's SeqOrder 11, TieRank 12, SpawnOrder 6 among them) at every attempt.
	int32 Compared = 0;
	for (uint32 DomainValue = 1; DomainValue <= GStealthSeedDomainMax; ++DomainValue)
	{
		const ESeedDomain Domain = static_cast<ESeedDomain>(DomainValue);
		for (const uint32 Attempt : Attempts)
		{
			const uint64 A = SeedsS1a.GetServerSubSeed(Domain, Attempt);
			const uint64 B = SeedsS1b.GetServerSubSeed(Domain, Attempt);
			const uint64 C = SeedsS2.GetServerSubSeed(Domain, Attempt);
			TestTrue(FString::Printf(TEXT("domain %u attempt %u: S1 identical across derivations"), DomainValue, Attempt), A == B);
			TestTrue(FString::Printf(TEXT("domain %u attempt %u: S2 differs from S1"), DomainValue, Attempt), A != C);
			TestTrue(FString::Printf(TEXT("domain %u attempt %u: a derived sub-seed is non-zero"), DomainValue, Attempt), A != 0);
			++Compared;
		}
	}
	TestEqual(TEXT("every domain and attempt compared"), Compared, static_cast<int32>(GStealthSeedDomainMax) * static_cast<int32>(UE_ARRAY_COUNT(Attempts)));

	// The purposes are separated: SeqOrder, TieRank and SpawnOrder never share a sub-seed for one attempt.
	TestTrue(TEXT("SeqOrder and TieRank differ for one tick"), SeedsS1a.GetServerSubSeed(ESeedDomain::SeqOrder, 180) != SeedsS1a.GetServerSubSeed(ESeedDomain::TieRank, 180));
	TestTrue(TEXT("SeqOrder and SpawnOrder differ for one attempt"), SeedsS1a.GetServerSubSeed(ESeedDomain::SeqOrder, 1) != SeedsS1a.GetServerSubSeed(ESeedDomain::SpawnOrder, 1));
	// The round index is part of the derivation.
	FStealthFakeEntropySource EntropyS1c(RoundTestFixtures::SecretS1);
	FStealthServerSeeds SeedsS1NextRound;
	SeedsS1NextRound.Open(EntropyS1c, &Faults);
	SeedsS1NextRound.SetRoundIndex(RoundIndex + 1);
	TestTrue(TEXT("a different round index gives a different sub-seed"), SeedsS1NextRound.GetServerSubSeed(ESeedDomain::SpawnOrder, 1) != SeedsS1a.GetServerSubSeed(ESeedDomain::SpawnOrder, 1));

	// Edge: a failing entropy source never opens and yields no sub-seed (never a weak substitute).
	FStealthFakeEntropySource Failing(RoundTestFixtures::SecretS1);
	Failing.bFail = true;
	FStealthServerSeeds SeedsFailed;
	TestFalse(TEXT("a failing entropy source does not open"), SeedsFailed.Open(Failing, &Faults));
	TestTrue(TEXT("no sub-seed without a secret"), SeedsFailed.GetServerSubSeed(ESeedDomain::SeqOrder, 1) == 0);

	const FStealthFaultTeardownResult Teardown = Faults.EvaluateTeardown({ { EStealthFault::EntropyFailed } });
	TestTrue(Teardown.Describe(), Teardown.IsClean());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
