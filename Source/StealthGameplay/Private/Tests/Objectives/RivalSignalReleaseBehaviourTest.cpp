// Objectives Chain story oc-015 AC1 (AC-OC-09 [U]): StealthGame.Objectives.RivalSignalReleaseBehaviour.
// GIVEN team 1's second completion draws a delay that releases it first, or on the same tick, THEN team 0's count rises 0 -> 1
// -> 2, or 0 -> 2 in one drain, and never falls; the view carries only the integer; a release due after the latch is never
// published; with bRivalSignalEnabled false, or in a 1v0 dev round, nothing is published and no StageSignal timer is scheduled.
// The seed pairs are found once in the pinned sub-seed list (SplitMix64 of 1..20000) and reported in the run's info lines.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesRivalRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesRivalSignalReleaseBehaviourTest, "StealthGame.Objectives.RivalSignalReleaseBehaviour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesRivalSignalReleaseBehaviourTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	// Team 1 completes Prelim1 at Active + 10 s (k = 1) and Prelim2 at Active + 20 s (k = 2).
	// Pair A: k = 1 releases on the 180 s tick (t_c + Delta in (150, 180]), k = 2 on the 60 s tick: the second releases first.
	const uint64 LateFirst = FindSubSeed([](int32 D) { return 10000 + D > 150000 && 10000 + D <= 180000; });
	const uint64 EarlySecond = FindSubSeed([](int32 D) { return 20000 + D > 30000 && 20000 + D <= 60000; });
	// Pair B: both on the 90 s tick.
	const uint64 SameTick1 = FindSubSeed([](int32 D) { return 10000 + D > 60000 && 10000 + D <= 90000; });
	const uint64 SameTick2 = FindSubSeed([](int32 D) { return 20000 + D > 60000 && 20000 + D <= 90000 && D != 0; });
	AddInfo(FString::Printf(TEXT("OC15 pinned pairs: A (0x%016llX, 0x%016llX), B (0x%016llX, 0x%016llX)"), LateFirst, EarlySecond, SameTick1, SameTick2));
	if (!TestTrue(TEXT("the pinned pairs exist"), LateFirst != 0 && EarlySecond != 0 && SameTick1 != 0 && SameTick2 != 0))
	{
		return false;
	}

	auto Run = [this](uint64 Seed1, uint64 Seed2, const TCHAR* Label, TArray<int32>& OutCounts, TArray<double>& OutResolveTimes, int32& OutDrainsWithTwo)
	{
		FRivalRig Rig;
		UObjectiveTeamViewComponent* Team0View = Rig.ViewTargets.Add(1, 0);
		Rig.ViewTargets.Add(2, 1);
		Rig.SecretDraw.PerOrdinal.Add(1, Seed1);
		Rig.SecretDraw.PerOrdinal.Add(2, Seed2);
		Rig.ToActive();
		const double T0 = Rig.Core().GetActivePhaseStart();
		Rig.RunUntil(T0 + 10.0);
		Rig.CompleteStageFast(1);
		Rig.RunUntil(T0 + 20.0);
		Rig.CompleteStageFast(1);
		int32 Last = 0;
		int32 Falls = 0;
		OutDrainsWithTwo = 0;
		while (Rig.Q.Clock.GetServerTimeSeconds() < T0 + 200.0)
		{
			const int32 Before = Rig.RivalCount(0);
			Rig.Q.Tick(0.1);
			const int32 After = Rig.RivalCount(0);
			if (After != Before)
			{
				OutCounts.Add(After);
				OutDrainsWithTwo += After - Before == 2 ? 1 : 0;
			}
			Falls += After < Last ? 1 : 0;
			Last = After;
		}
		OutResolveTimes = Rig.Spy.Times;
		for (double& Time : OutResolveTimes)
		{
			Time -= T0;
		}
		TestEqual(*FString::Printf(TEXT("%s: the count never falls"), Label), Falls, 0);
		TestEqual(*FString::Printf(TEXT("%s: team 0's component holds the final count"), Label), static_cast<int32>(Team0View->GetView().RivalPrelimCount), 2);
		TestEqual(*FString::Printf(TEXT("%s: two StageSignalReleased entries"), Label), Rig.Logged(ERoundLogType::StageSignalReleased), 2);
	};

	TArray<int32> CountsA;
	TArray<double> TimesA;
	int32 TwoA = 0;
	Run(LateFirst, EarlySecond, TEXT("A"), CountsA, TimesA, TwoA);
	TestEqual(TEXT("A: 0 -> 1 -> 2"), FString::JoinBy(CountsA, TEXT(","), [](int32 C) { return FString::FromInt(C); }), FString(TEXT("1,2")));
	TestTrue(TEXT("A: the second completion's release came first (60 s tick), then 180 s"), TimesA.Num() == 2 && FMath::IsNearlyEqual(TimesA[0], 60.0, 0.11) && FMath::IsNearlyEqual(TimesA[1], 180.0, 0.11));

	TArray<int32> CountsB;
	TArray<double> TimesB;
	int32 TwoB = 0;
	Run(SameTick1, SameTick2, TEXT("B"), CountsB, TimesB, TwoB);
	TestEqual(TEXT("B: 0 -> 2 in one drain"), FString::JoinBy(CountsB, TEXT(","), [](int32 C) { return FString::FromInt(C); }), FString(TEXT("2")));
	TestEqual(TEXT("B: one drain raised it by two"), TwoB, 1);
	TestTrue(TEXT("B: both resolved on the 90 s tick"), TimesB.Num() == 2 && FMath::IsNearlyEqual(TimesB[0], 90.0, 0.11) && TimesB[0] == TimesB[1]);

	// The view carries only the integer: FObjectiveTeamView's rival field is one uint8 (no id, site, time or order).
	const FProperty* RivalField = FObjectiveTeamView::StaticStruct()->FindPropertyByName(TEXT("RivalPrelimCount"));
	TestTrue(TEXT("RivalPrelimCount is a single uint8"), RivalField != nullptr && RivalField->GetCPPType() == TEXT("uint8"));

	// A release due after the latch is never published (the queue's Rejected_Latched).
	{
		FRivalRig Rig;
		Rig.SecretDraw.PerOrdinal.Add(1, LateFirst);
		Rig.ToActive();
		const double T0 = Rig.Core().GetActivePhaseStart();
		Rig.RunUntil(T0 + 10.0);
		Rig.CompleteStageFast(1);
		Rig.Q.Queue->Latch(FQueueKey(999999, 1)); // Round's TryTerminate (a win elsewhere) before the release is due
		Rig.RunUntil(T0 + 200.0);
		TestEqual(TEXT("latched: nothing published"), static_cast<int32>(Rig.RivalCount(0)), 0);
		TestEqual(TEXT("latched: the resolver never ran"), Rig.Spy.Payloads.Num(), 0);
		TestEqual(TEXT("latched: no StageSignalReleased"), Rig.Logged(ERoundLogType::StageSignalReleased), 0);
		TestTrue(TEXT("latched: the queue logged Rejected_Latched for the timer"), Rig.LoggedOf(ERoundLogType::Rejected_Latched).ContainsByPredicate([](const FRoundLogEntry& E)
		{
			return E.Data[0] == static_cast<uint32>(EQueueItemType::TimerFired);
		}));
	}

	// bRivalSignalEnabled false: no draw, no timer, nothing published.
	{
		FRivalRig Rig([](FObjectivesServiceInit& Init) { Init.ServerTuning.bRivalSignalEnabled = false; });
		Rig.ToActive();
		Rig.Service->SetTuning(Rig.Init.PublicTuning, Rig.Init.ServerTuning);
		const double T0 = Rig.Core().GetActivePhaseStart();
		Rig.CompleteStageFast(1);
		Rig.CompleteStageFast(0);
		Rig.RunUntil(T0 + 400.0, 1.0);
		TestEqual(TEXT("disabled: no StageSignal timer scheduled"), Rig.Service->GetScheduledStageSignalCount(), 0);
		TestEqual(TEXT("disabled: no secret draw"), Rig.SecretDraw.Reads.Num(), 0);
		TestTrue(TEXT("disabled: both counts stay 0"), Rig.RivalCount(0) == 0 && Rig.RivalCount(1) == 0);
	}

	// A 1v0 dev round: forced off even with bRivalSignalEnabled true.
	{
		FRivalRig Rig;
		Rig.ViewTargets.Add(1, 0); // team 0 has a member, team 1 none
		Rig.ToActive();
		const double T0 = Rig.Core().GetActivePhaseStart();
		Rig.CompleteStageFast(0);
		Rig.CompleteStageFast(0);
		Rig.RunUntil(T0 + 400.0, 1.0);
		TestTrue(TEXT("1v0: the round is flagged out of the stall metrics"), Rig.Service->IsExcludedFromStallMetrics());
		TestEqual(TEXT("1v0: no StageSignal timer scheduled"), Rig.Service->GetScheduledStageSignalCount(), 0);
		TestTrue(TEXT("1v0: nothing published"), Rig.RivalCount(0) == 0 && Rig.RivalCount(1) == 0 && Rig.Logged(ERoundLogType::StageSignalReleased) == 0);
		TestEqual(TEXT("1v0: one ObjectivePacing exclusion entry"), Rig.Logged(ERoundLogType::ObjectivePacing), 1);
	}
	return true;
}

#endif
