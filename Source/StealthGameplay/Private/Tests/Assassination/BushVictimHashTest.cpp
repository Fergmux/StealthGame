// Story asn-006, AC2 / AC-ASN-39 BushVictimHash (the GDD's stable name; the pick is the BushSpotPick stream, not a hash).
// A bush with two reachable non-ally occupants and 1,000 strikes (keys (tick i, Seq 1)) drawn from the BushSpotPick
// stream with a fixed injected sub-seed (BushPickSeed): exactly one victim per strike, each occupant picked 450-550 times.
// A third occupant failing F1 (dz 100 > 75) is never picked, and the run still consumes exactly one draw per strike, the
// same as the two-occupant run; a bush with one or zero reachable occupants also draws once. The sub-seed spy sees only
// GetServerSubSeed(BushSpotPick, LayoutAttempt) and never a RoundSeed draw.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

namespace BushVictimHashTestData
{
	constexpr uint64 BushPickSeed = 0xB05B'1C4E'2026'1027ull;
	constexpr int32 Strikes = 1000;
	constexpr int32 MinPicks = 450;
	constexpr int32 MaxPicks = 550;
	const FVector BushCenter(170.0, 0.0, 0.0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationBushVictimHashTest, "StealthGame.Assassination.BushVictimHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationBushVictimHashTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace BushVictimHashTestData;

	for (const bool bWithUnreachable : { false, true })
	{
		const TCHAR* Name = bWithUnreachable ? TEXT("3 occupants, 1 out of reach") : TEXT("2 occupants");
		FRig R(TEXT("BushVictimHash"));
		R.Seeds.Seed = BushPickSeed;
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.AddBush(1, BushCenter, 60.0);
		APawn* O1 = R.Spawn(BushCenter + FVector(0.0, 20.0, 0.0));
		APawn* O2 = R.Spawn(BushCenter + FVector(0.0, -20.0, 0.0), 1);
		R.Conceal(O1, 1);
		R.Conceal(O2, 1);
		APawn* High = nullptr;
		if (bWithUnreachable)
		{
			High = R.Spawn(BushCenter + FVector(10.0, 0.0, 100.0));
			R.Conceal(High, 1);
		}
		int32 Picks1 = 0, Picks2 = 0, PicksHigh = 0, Draws = 0, NoVictim = 0;
		const FVictimResolverDeps Deps = R.Core->MakeResolverDeps();
		for (int32 Strike = 0; Strike < Strikes; ++Strike)
		{
			FVictimRequest Request;
			Request.Attacker = R.Facts(A);
			Request.WorldPoint = BushCenter;
			Request.Key = FQueueKey(10000 + Strike, 1);
			const FVictimResolution Res = FVictimResolverCore::Resolve(R.Params, Deps, Request);
			Picks1 += Res.Victim == O1 ? 1 : 0;
			Picks2 += Res.Victim == O2 ? 1 : 0;
			PicksHigh += High != nullptr && Res.Victim == High ? 1 : 0;
			NoVictim += Res.Victim == nullptr ? 1 : 0;
			Draws += Res.BushDraws;
			if (Strike == 0)
			{
				TestEqual(FString::Printf(TEXT("%s: two survive the F1 filter"), Name), Res.ReachableCount, 2);
			}
		}
		AddInfo(FString::Printf(TEXT("BUSHPICK %s: O1 %d, O2 %d, out-of-reach %d, draws %d"), Name, Picks1, Picks2, PicksHigh, Draws));
		TestEqual(FString::Printf(TEXT("%s: exactly one victim per strike"), Name), Picks1 + Picks2 + PicksHigh, Strikes);
		TestEqual(FString::Printf(TEXT("%s: never no victim"), Name), NoVictim, 0);
		TestTrue(FString::Printf(TEXT("%s: O1 picked %d times (450-550)"), Name, Picks1), Picks1 >= MinPicks && Picks1 <= MaxPicks);
		TestTrue(FString::Printf(TEXT("%s: O2 picked %d times (450-550)"), Name, Picks2), Picks2 >= MinPicks && Picks2 <= MaxPicks);
		TestEqual(FString::Printf(TEXT("%s: the out-of-reach occupant is never picked"), Name), PicksHigh, 0);
		TestEqual(FString::Printf(TEXT("%s: one draw per strike"), Name), Draws, Strikes);
		TestTrue(FString::Printf(TEXT("%s: only BushSpotPick sub-seeds were asked"), Name), R.Seeds.Asked.Num() == Strikes
			&& !R.Seeds.Asked.ContainsByPredicate([](ESeedDomain D) { return D != ESeedDomain::BushSpotPick; }));
		TestEqual(FString::Printf(TEXT("%s: never a RoundSeed draw"), Name), R.Seeds.RoundSeedCalls, 0);
	}

	// One draw also with one or zero reachable occupants.
	{
		FRig R(TEXT("BushVictimHashFew"));
		R.Seeds.Seed = BushPickSeed;
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.AddBush(1, BushCenter, 60.0);
		APawn* Only = R.Spawn(BushCenter);
		R.Conceal(Only, 1);
		FVictimRequest Request;
		Request.Attacker = R.Facts(A);
		Request.WorldPoint = BushCenter;
		Request.Key = FQueueKey(10000, 1);
		const FVictimResolution One = FVictimResolverCore::Resolve(R.Params, R.Core->MakeResolverDeps(), Request);
		TestTrue(TEXT("one reachable: picked, one draw"), One.Victim == Only && One.BushDraws == 1);
		R.Bushes.Bushes[0].Occupants.Reset();
		const FVictimResolution None = FVictimResolverCore::Resolve(R.Params, R.Core->MakeResolverDeps(), Request);
		TestTrue(TEXT("none reachable: a whiff, still one draw"), None.Victim == nullptr && None.BushDraws == 1 && None.BushId == 1);
	}
	return true;
}

#endif
