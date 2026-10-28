// Objectives Chain story oc-014 AC1 (AC-OC-09 grid clause [U]): StealthGame.Objectives.RivalSignalReleaseGrid.
// GIVEN a pinned StageSignal sub-seed and team 1 completing Prelim1 at t_c = 100 s (Active at t = 0) THEN team 0's
// RivalPrelimCount becomes 1 exactly at t_rel = 30 * ceil((100 + Delta_1) / 30) s with Delta_1 in [30, 165], never earlier, and
// only at a multiple of 30 s from Active. Runs on the real queue (the production scheduler), in 0.1 s frames and in 1/60 s
// frames, with two pinned sub-seeds (the golden k = 1 sub-seed and one whose t_c + Delta lands just past a tick).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesRivalRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesRivalSignalReleaseGridTest, "StealthGame.Objectives.RivalSignalReleaseGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesRivalSignalReleaseGridTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	// The golden k = 1 sub-seed (Delta 44.172 s) and one whose t_c + Delta lands up to 100 ms past a tick (so it waits ~30 s for
	// the next one: the ceiling, never the floor). The exact-tick boundary is pinned by StealthGame.Objectives.RivalSignalGolden.
	const uint64 Golden = 0xD7705C629EA0163Cull;
	const uint64 OnTick = FindSubSeed([](int32 DeltaMs) { return (100000 + DeltaMs) % 30000 > 0 && (100000 + DeltaMs) % 30000 <= 100; });
	if (!TestTrue(TEXT("a pinned sub-seed with t_c + Delta just past a grid tick exists"), OnTick != 0))
	{
		return false;
	}
	const uint64 Seeds[] = { Golden, OnTick };
	const double Steps[] = { 0.1, 1.0 / 60.0 };
	for (const uint64 Seed : Seeds)
	{
		for (const double Step : Steps)
		{
			// Arrange
			FRivalRig Rig;
			Rig.SecretDraw.PerOrdinal.Add(1, Seed);
			TestTrue(TEXT("the round is Active"), Rig.ToActive());
			const double T0 = Rig.Core().GetActivePhaseStart();
			Rig.RunUntil(T0 + 100.0, Step);
			// Act: team 1 completes Prelim1 at t_c = 100 s (the counted-completion effect in a slot).
			Rig.CompleteStageFast(1);
			const TArray<FObjectivesRivalRelease>& Pending = Rig.Core().GetPendingRivalReleases();
			if (!TestEqual(TEXT("one release pending"), Pending.Num(), 1))
			{
				return false;
			}
			const int32 DeltaMs = Pending[0].DeltaMs;
			const int64 T0Ms = FObjectivesRivalSignal::ToMs(T0);
			const int64 SinceActiveMs = Pending[0].CompletedMs - T0Ms;
			TestTrue(TEXT("t_c is 100 s after Active"), FMath::Abs(SinceActiveMs - 100000) <= 1);
			const int64 Expected = T0Ms + 30000 * ((SinceActiveMs + DeltaMs + 29999) / 30000);
			const double Release = static_cast<double>(Expected) / 1000.0;
			TestTrue(TEXT("Delta_1 in [30, 165] s"), DeltaMs >= 30000 && DeltaMs <= 165000);
			TestEqual(TEXT("t_rel = 30 * ceil((100 + Delta) / 30) s from Active"), Pending[0].ReleaseMs, Expected);
			TestEqual(TEXT("for team 0 (the completer's opponent)"), static_cast<int32>(Pending[0].ViewingTeam), 0);
			TestEqual(TEXT("one TimerFired(StageSignal) on the queue"), Rig.Service->GetScheduledStageSignalCount(), 1);
			// Never earlier: the last frame before t_rel still shows 0.
			Rig.RunUntil(Release - Step, Step);
			TestEqual(TEXT("0 before t_rel"), static_cast<int32>(Rig.RivalCount(0)), 0);
			Rig.RunUntil(Release + Step, Step);
			TestEqual(TEXT("1 at t_rel"), static_cast<int32>(Rig.RivalCount(0)), 1);
			TestEqual(TEXT("team 1's own count is untouched"), static_cast<int32>(Rig.RivalCount(1)), 0);
			if (TestEqual(TEXT("one release resolved"), Rig.Spy.Times.Num(), 1))
			{
				const double ResolvedAt = Rig.Spy.Times[0];
				AddInfo(FString::Printf(TEXT("OC14 seed 0x%016llX step %.4f: Delta %.3f s, t_rel %.3f s from Active, resolved at %.4f s"), Seed, Step,
					DeltaMs / 1000.0, (Expected - T0Ms) / 1000.0, ResolvedAt - T0));
				TestTrue(TEXT("resolved in the first frame at or after t_rel"), ResolvedAt + 1e-9 >= Release && ResolvedAt < Release + Step + 1e-6);
				TestEqual(TEXT("t_rel is a multiple of 30 s from Active"), (Expected - T0Ms) % 30000, static_cast<int64>(0));
			}
		}
	}
	return true;
}

#endif
