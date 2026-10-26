// Story rmf-037 (production/epics/round-match-framework/story-037-spawn-order-ignores-playerness.md), GDD AC 55 / R23 (a), F5:
//   - with the same secret and seed and the player flags of the spawn list flipped, sigma is identical: the P6 sequence
//     is run twice through the real Round core with D = 8 (2v2 with 4 NPCs, then 4v4 with no NPCs: every slot's player
//     flag differs between the runs) and the slot order the spawner receives is the same;
//   - a different secret with the same seed (RoundIndex, attempt) gives a different sigma (S1 vs S2 at D = 10, a pair
//     known to differ).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundFormulas.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "StealthRoundExitBTestRig.h"

namespace SpawnOrderIgnoresPlayernessTestData
{
	/** The slot order P6 spawned in, for a Team0 v Team1 layout with NpcCount NPCs. */
	TArray<int32> RunP6(int32 Team0, int32 Team1, int32 NpcCount, TArray<bool>& OutPlayerFlags)
	{
		FRoundRuleParams Params = FRoundExitBRig::DefaultParams();
		Params.NpcCount = NpcCount;
		FRoundExitBRig Rig(Params);
		Rig.OpenLobby();
		Rig.SetLayout(Team0, Team1);
		Rig.EnterPreparing();
		Rig.ScriptPreparingSequence();
		Rig.Core->NotifyTuningReady(RoundTestFixtures::PreparingRigTuningHash);
		Rig.Core->NotifyLayoutReady(1, Rig.Core->GetPreparing().GetSeed());
		Rig.AckAll(false);
		Rig.Frame();
		TArray<int32> Slots;
		OutPlayerFlags.Init(false, Team0 + Team1 + NpcCount);
		for (const FRoundSpawnRecord& Record : Rig.Core->GetPreparing().GetSpawnRecords())
		{
			Slots.Add(Record.Entry.Slot);
			OutPlayerFlags[Record.Entry.Slot] = Record.Entry.IsPlayer();
		}
		return Slots;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSpawnOrderIgnoresPlayernessTest, "StealthGame.Round.SpawnOrderIgnoresPlayerness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundSpawnOrderIgnoresPlayernessTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace SpawnOrderIgnoresPlayernessTestData;

	// Same secret and seed (the scripted SpawnOrder sub-seed), every player flag flipped.
	TArray<bool> FlagsA;
	TArray<bool> FlagsB;
	const TArray<int32> SigmaA = RunP6(Layout2v2.Team0, Layout2v2.Team1, PreparingRigNpcCount, FlagsA);
	const TArray<int32> SigmaB = RunP6(Layout4v4.Team0, Layout4v4.Team1, 0, FlagsB);
	if (TestEqual(TEXT("both lists have D = 8 entries"), SigmaA.Num(), SigmaB.Num()))
	{
		int32 Flipped = 0;
		for (int32 Slot = 0; Slot < FlagsA.Num(); ++Slot)
		{
			Flipped += FlagsA[Slot] != FlagsB[Slot] ? 1 : 0;
		}
		TestEqual(TEXT("the second list differs in player-ness from the first at the NPC slots"), Flipped, PreparingRigNpcCount);
		TestTrue(TEXT("sigma is identical with the player flags flipped"), SigmaA == SigmaB);
	}
	{
		// Every flag flipped (not one): the flags only ride along as payload, so the permutation is the pure F5 function.
		TArray<int32> Pure;
		StealthRoundFormulas::ComputeSpawnOrder(SigmaA.Num(), PreparingRigSpawnSubSeed, Pure);
		TestTrue(TEXT("the spawned order is F5's sigma for (D, sub-seed)"), Pure == SigmaA);
	}

	// A different secret with the same seed gives a different sigma.
	FStealthFakeEntropySource EntropyS1(SecretS1);
	FStealthFakeEntropySource EntropyS2(SecretS2);
	FStealthServerSeeds SeedsS1;
	FStealthServerSeeds SeedsS2;
	TestTrue(TEXT("S1 opens"), SeedsS1.Open(EntropyS1));
	TestTrue(TEXT("S2 opens"), SeedsS2.Open(EntropyS2));
	SeedsS1.SetRoundIndex(1);
	SeedsS2.SetRoundIndex(1);
	TArray<int32> SigmaS1;
	TArray<int32> SigmaS2;
	StealthRoundFormulas::ComputeSpawnOrder(SpawnOrderD, SeedsS1.GetServerSubSeed(ESeedDomain::SpawnOrder, 0), SigmaS1);
	StealthRoundFormulas::ComputeSpawnOrder(SpawnOrderD, SeedsS2.GetServerSubSeed(ESeedDomain::SpawnOrder, 0), SigmaS2);
	TestFalse(TEXT("a different secret with the same seed gives a different sigma"), SigmaS1 == SigmaS2);
	return true;
}

#endif
