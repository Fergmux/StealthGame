// Story rmf-036 (production/epics/round-match-framework/story-036-spawn-order-uniform.md), GDD AC 29 / F5, ADR-0006:
//   with D = 10 (5v5, no NPCs), the fixed fixture secret S1 through the fixed-secret seam and 1,000 round starts, the
//   spawn rank of each player slot is uniform on 1..10: chi-square with 9 degrees of freedom below 27.88 (alpha 0.001)
//   for every slot. Secrets and the threshold are fixture constants fixed before the first run.
// How the 1,000 starts differ (Open item): by RoundIndex (1..1000), which ADR-0006 section 3 folds into every sub-seed
// (SipHash(ServerSecret, 0x02 | MatchNonce | RoundIndex | Domain | Attempt)); Attempt is LayoutAttempt 0 throughout.
// Deterministic: no random seed, no wall clock. No sub-seed is printed (ADR-0006 section 7).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundFormulas.h"
#include "Server/Testing/StealthQueueTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSpawnOrderUniformTest, "StealthGame.Round.SpawnOrderUniform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundSpawnOrderUniformTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	// Arrange: the server seeds opened from the fixed fixture secret.
	FStealthFakeEntropySource Entropy(SecretS1);
	FStealthServerSeeds Seeds;
	TestTrue(TEXT("the fixture secret opens the seeds"), Seeds.Open(Entropy));

	// Act: Tally[slot][rank - 1] over 1,000 starts.
	TArray<TArray<int32>> Tally;
	Tally.SetNum(SpawnOrderD);
	for (TArray<int32>& Row : Tally)
	{
		Row.Init(0, SpawnOrderD);
	}
	TArray<int32> Sigma;
	for (int32 Start = 0; Start < SpawnOrderStarts; ++Start)
	{
		Seeds.SetRoundIndex(static_cast<uint32>(Start + 1));
		StealthRoundFormulas::ComputeSpawnOrder(SpawnOrderD, Seeds.GetServerSubSeed(ESeedDomain::SpawnOrder, 0), Sigma);
		for (int32 Position = 0; Position < Sigma.Num(); ++Position)
		{
			++Tally[Sigma[Position]][Position];
		}
	}

	// Assert: every slot's rank histogram passes chi-square (9 dof) at the fixed critical value.
	const double Expected = static_cast<double>(SpawnOrderStarts) / SpawnOrderD;
	for (int32 Slot = 0; Slot < SpawnOrderD; ++Slot)
	{
		double ChiSquare = 0.0;
		for (int32 Rank = 0; Rank < SpawnOrderD; ++Rank)
		{
			const double Delta = Tally[Slot][Rank] - Expected;
			ChiSquare += Delta * Delta / Expected;
		}
		TestTrue(FString::Printf(TEXT("slot %d: chi-square %.2f < %.2f"), Slot, ChiSquare, SpawnOrderChiSquareCritical), ChiSquare < SpawnOrderChiSquareCritical);
	}

	// Failure mode (edge case): a slot that never takes some rank has a large chi-square and fails.
	{
		TArray<int32> Degenerate;
		Degenerate.Init(0, SpawnOrderD);
		Degenerate[0] = SpawnOrderStarts; // always first
		double ChiSquare = 0.0;
		for (const int32 Count : Degenerate)
		{
			const double Delta = Count - Expected;
			ChiSquare += Delta * Delta / Expected;
		}
		TestTrue(TEXT("a slot with missing ranks fails the bound"), ChiSquare >= SpawnOrderChiSquareCritical);
	}
	return true;
}

#endif
