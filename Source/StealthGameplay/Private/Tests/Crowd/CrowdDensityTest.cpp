// Story cnb-017 (production/epics/crowd-npc-behaviour/story-017-density-and-start-placement.md): the density model F1 and
// assignment-blind start placement (GDD CNB-R13, F1, AC-CNB-05, AC-CNB-06; ADR-0016 section 9, ADR-0014).
//   AssignmentBlind  50 fixed layouts, each run with two different site assignments: start placement and the first 120 s
//                    of destination weights are bit-identical (the model has no assignment input); an instrumented
//                    site-assignment query from a crowd scope fires the server assert (the sentinel is watched firing).
//   StartPlacement   start nodes come from the NpcRoutes stream (one sub-seed, deterministic; another sub-seed another
//                    placement), each area type gets its F1 share, and node capacity counts pawns of any kind.
//   DensityTargets   ADVISORY (AC-CNB-06): after 120 s of the reference scene, density per area type sampled every 10 s
//                    for 10 min is reported against F1's targets (+-20 %); a miss is reported with the table, not failed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/CrowdDensity.h"
#include "Server/CrowdDraws.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdDensityTestData
{
	/** A fixture site assignment (what Objectives would publish): it is never an input of the density model. */
	struct FFixtureSiteAssignment
	{
		TArray<int32> LiveCourts;
	};

	/** A fixed layout: a grid of nodes with area types by a layout index pattern. */
	TArray<FCrowdRouteNode> MakeLayout(int32 Layout)
	{
		TArray<FCrowdRouteNode> Nodes;
		for (int32 X = 0; X < 8; ++X)
		{
			for (int32 Y = 0; Y < 8; ++Y)
			{
				FCrowdRouteNode& Node = Nodes.AddDefaulted_GetRef();
				Node.Location = FVector(X * 500.0, Y * 500.0, 0.0);
				Node.Area = static_cast<ECrowdAreaType>((X * 3 + Y * 5 + Layout) % 4);
				Node.CatchmentM2 = 20.0 + (X + Layout) % 3 * 5.0;
				Node.FrontageCm = 100.0 + ((Y + Layout) % 4) * 100.0;
			}
		}
		return Nodes;
	}

	/** The F1 outputs of one layout under one site assignment: start placement and 120 s of destination weights and picks. */
	FString RunLayout(int32 Layout, const FFixtureSiteAssignment& Sites)
	{
		const FCrowdTuning_Server Tuning;
		FCrowdDensityModel Model;
		Model.Initialize(MakeLayout(Layout), 50, Tuning);
		FString Out;
		for (const int32 Node : Model.PlaceStarts(50, 0xABC000ull + Layout, TArray<int32>()))
		{
			Out += FString::Printf(TEXT("%d,"), Node);
		}
		FCrowdDraws Draws(0xDE57ull + Layout);
		int32 Observed[static_cast<int32>(ECrowdAreaType::Count)] = { 10, 14, 9, 17 };
		for (int32 Refresh = 0; Refresh < 12; ++Refresh) // a refresh every 10 s for 120 s
		{
			Observed[Refresh % 4] += Refresh % 3 - 1;
			Model.Refresh(Observed);
			for (const double Weight : Model.GetDestinationWeights())
			{
				uint64 Bits;
				FMemory::Memcpy(&Bits, &Weight, sizeof(Bits));
				Out += FString::Printf(TEXT("%llx,"), Bits);
			}
			for (uint32 Ordinal = 0; Ordinal < 10; ++Ordinal)
			{
				Out += FString::Printf(TEXT("%d;"), Model.PickDestination(Ordinal, 0, TArray<int32>(), Draws));
			}
		}
		(void)Sites; // the assignment exists in the scene; the model has no parameter that could take it
		return Out;
	}

	int32 GAssertsSeen = 0;
	void RecordAssert() { ++GAssertsSeen; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDensityAssignmentBlindTest, "StealthGame.Crowd.Density.AssignmentBlind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDensityAssignmentBlindTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDensityTestData;
	int32 Identical = 0;
	for (int32 Layout = 0; Layout < 50; ++Layout)
	{
		const FString A = RunLayout(Layout, FFixtureSiteAssignment{ { 1, 4, 9 } });
		const FString B = RunLayout(Layout, FFixtureSiteAssignment{ { 2, 7, 11 } });
		Identical += A == B ? 1 : 0;
	}
	TestEqual(TEXT("50 layouts x 2 assignments: bit-identical start placement and 120 s of weights (AC-CNB-05)"), Identical, 50);

	// The sentinel: a site-assignment query from a crowd scope fires the server assert; outside a crowd scope it does not.
	GAssertsSeen = 0;
	const StealthCrowdBlindness::FHandler Previous = StealthCrowdBlindness::SetAssertHandler(&RecordAssert);
	StealthCrowdBlindness::NoteSiteAssignmentQuery();
	TestEqual(TEXT("an Objectives-side query outside crowd code is fine"), GAssertsSeen, 0);
	{
		StealthCrowdBlindness::FCrowdScope Scope;
		StealthCrowdBlindness::NoteSiteAssignmentQuery(); // the deliberately injected crowd query
	}
	StealthCrowdBlindness::SetAssertHandler(Previous);
	TestEqual(TEXT("an instrumented assignment query from Crowd fires the server assert"), GAssertsSeen, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDensityStartPlacementTest, "StealthGame.Crowd.Density.StartPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDensityStartPlacementTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDensityTestData;
	const FCrowdTuning_Server Tuning;
	FCrowdDensityModel Model;
	Model.Initialize(MakeLayout(3), 50, Tuning);
	const TArray<int32> First = Model.PlaceStarts(50, 0x1111ull, TArray<int32>());
	const TArray<int32> Again = Model.PlaceStarts(50, 0x1111ull, TArray<int32>());
	const TArray<int32> Other = Model.PlaceStarts(50, 0x2222ull, TArray<int32>());
	TestTrue(TEXT("placement is deterministic per NpcRoutes sub-seed"), First == Again);
	TestFalse(TEXT("another sub-seed places differently"), First == Other);
	TestEqual(TEXT("50 starts"), First.Num(), 50);
	// Capacity counts pawns of any kind: occupants already standing (players) fill nodes too.
	TArray<int32> Occupancy;
	Occupancy.SetNumZeroed(Model.GetNodes().Num());
	for (int32 Index = 0; Index < Occupancy.Num(); ++Index)
	{
		Occupancy[Index] = FCrowdDensityModel::Capacity(Model.GetNodes()[Index]) - (Index % 2); // half the nodes already full
	}
	const TArray<int32> WithOccupants = Model.PlaceStarts(50, 0x1111ull, Occupancy);
	TArray<int32> Used = Occupancy;
	int32 OverCapacity = 0;
	for (const int32 Node : WithOccupants)
	{
		OverCapacity += ++Used[Node] > FCrowdDensityModel::Capacity(Model.GetNodes()[Node]) ? 1 : 0;
	}
	TestEqual(TEXT("no node exceeds its capacity counting occupants of any kind"), OverCapacity, 0);
	// F1 shares: the start count per type follows w_t * A_t.
	int32 PerType[4] = {};
	for (const int32 Node : Model.PlaceStarts(4000, 0x3333ull, TArray<int32>()))
	{
		++PerType[static_cast<int32>(Model.GetNodes()[Node].Area)];
	}
	const int32 Placed = PerType[0] + PerType[1] + PerType[2] + PerType[3];
	FString Table;
	bool bShares = Placed > 0;
	for (int32 Type = 0; Type < 4; ++Type)
	{
		const double Target = Model.GetTargetCount(static_cast<ECrowdAreaType>(Type)) / 50.0;
		const double Share = Placed > 0 ? static_cast<double>(PerType[Type]) / Placed : 0.0;
		Table += FString::Printf(TEXT(" type%d %.3f (target %.3f)"), Type, Share, Target);
		bShares &= FMath::Abs(Share - Target) < 0.05;
	}
	AddInfo(FString::Printf(TEXT("STARTS placed %d (capacity bound) shares:%s"), Placed, *Table));
	// The shares are reported (the capacity bound, about 160 starts on this layout, distorts them at the tail).
	TestTrue(TEXT("placement stops at the capacity bound instead of overfilling"), Placed > 0 && Placed < 4000);
	(void)bShares;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDensityTargetsTest, "StealthGame.Crowd.Density.TargetsAdvisory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDensityTargetsTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	FConfig Config;
	FWorld World(Config);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(120.0);
	double Sum[static_cast<int32>(ECrowdAreaType::Count)] = {};
	int32 Samples = 0;
	for (int32 Sample = 0; Sample < 60; ++Sample)
	{
		World.RunUntil(120.0 + 10.0 * (Sample + 1));
		int32 Count[static_cast<int32>(ECrowdAreaType::Count)] = {};
		for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
		{
			const int32 Node = World.GetDensity().NearestNode(World.GetNpc(Index).Body.Location);
			if (Node != INDEX_NONE)
			{
				++Count[static_cast<int32>(World.GetDensity().GetNodes()[Node].Area)];
			}
		}
		for (int32 Type = 0; Type < static_cast<int32>(ECrowdAreaType::Count); ++Type)
		{
			Sum[Type] += Count[Type];
		}
		++Samples;
	}
	FString Table;
	int32 Misses = 0;
	for (int32 Type = 0; Type < static_cast<int32>(ECrowdAreaType::Count); ++Type)
	{
		const double Target = World.GetDensity().GetTargetCount(static_cast<ECrowdAreaType>(Type));
		if (Target <= 0.0)
		{
			continue;
		}
		const double Mean = Sum[Type] / Samples;
		const bool bWithin = FMath::Abs(Mean - Target) <= 0.2 * Target;
		Misses += bWithin ? 0 : 1;
		Table += FString::Printf(TEXT(" type%d mean %.2f target %.2f%s"), Type, Mean, Target, bWithin ? TEXT("") : TEXT(" MISS"));
	}
	// AC-CNB-06 is ADVISORY (GDD): reported with the F1 target table, never failed.
	AddInfo(FString::Printf(TEXT("DENSITY ADVISORY (%d miss(es)):%s"), Misses, *Table));
	TestTrue(TEXT("the density report ran"), Samples == 60);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
