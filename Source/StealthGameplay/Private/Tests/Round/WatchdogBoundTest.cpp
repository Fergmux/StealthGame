// Story rmf-035 (production/epics/round-match-framework/story-035-watchdog-bound.md), GDD AC 34a / F6:
//   - T_needed is 122.25 at the F6 defaults with a generated layout (effective watchdog 150, no warning);
//   - with TravelLoadTimeoutSec 120, T_needed is 161.25, the effective watchdog is 161.25 and a warning is raised;
//   - PreparingWatchdogSec = T_needed raises no warning (a tie), T_needed - 0.01 raises one;
//   - Round evaluates F6 once, at freeze (OnTuningReady): before it the configured watchdog bounds the wait, after it the
//     effective one does, counted from Preparing entry.
// Fixture values: GDD F6 variable table (defaults and the "used as the bound" figures), frozen in StealthRoundTestConstants.h.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundFormulas.h"
#include "StealthRoundExitBTestRig.h"

namespace WatchdogBoundTestData
{
	FRoundPreparingTimings FixtureTimings()
	{
		using namespace RoundTestFixtures;
		FRoundPreparingTimings T;
		T.TravelLoadTimeoutSec = F6TravelLoadTimeoutSec;
		T.TravelAckGraceSec = F6TravelAckGraceSec;
		T.ServerLoadBoundSec = F6ServerLoadBoundSec;
		T.LayoutTotalBudgetSec = F6LayoutTotalBudgetSec;
		T.TuningFreezeBoundSec = F6TuningFreezeBoundSec;
		T.SpawnBoundSec = F6SpawnBoundSec;
		T.WorldReadyTimeoutSec = F6WorldReadyTimeoutSec;
		T.StartLeadSec = F6StartLeadSec;
		T.PreparingWatchdogSec = F6PreparingWatchdogSec;
		return T;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundWatchdogBoundTest, "StealthGame.Round.WatchdogBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundWatchdogBoundTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace WatchdogBoundTestData;

	// The defaults are the shipped ones: FRoundPreparingTimings' initialisers equal the frozen F6 fixture.
	{
		const FRoundPreparingTimings Defaults;
		const FRoundPreparingTimings Fixture = FixtureTimings();
		TestTrue(TEXT("the code defaults are the F6 fixture values"), Defaults.TravelLoadTimeoutSec == Fixture.TravelLoadTimeoutSec
			&& Defaults.TravelAckGraceSec == Fixture.TravelAckGraceSec && Defaults.ServerLoadBoundSec == Fixture.ServerLoadBoundSec
			&& Defaults.LayoutTotalBudgetSec == Fixture.LayoutTotalBudgetSec && Defaults.TuningFreezeBoundSec == Fixture.TuningFreezeBoundSec
			&& Defaults.SpawnBoundSec == Fixture.SpawnBoundSec && Defaults.WorldReadyTimeoutSec == Fixture.WorldReadyTimeoutSec
			&& Defaults.StartLeadSec == Fixture.StartLeadSec && Defaults.PreparingWatchdogSec == Fixture.PreparingWatchdogSec);
	}

	// Defaults: T_prep_max = max(60, 20 + 45 + 1 + 15) = 81; T_needed = 81 + 1 + 10 + 0.25 + 30 = 122.25 <= 150.
	const StealthRoundFormulas::FWatchdogBound Default = StealthRoundFormulas::ComputePreparingWatchdog(FixtureTimings());
	TestEqual(TEXT("T_needed at defaults"), Default.NeededSec, F6ExpectedNeededDefaultSec, F6Tolerance);
	TestEqual(TEXT("effective watchdog at defaults is PreparingWatchdogSec"), Default.EffectiveSec, F6PreparingWatchdogSec, F6Tolerance);
	TestFalse(TEXT("no warning at defaults"), Default.bNeededWins);

	// TravelLoadTimeoutSec 120: T_needed = 120 + 1 + 10 + 0.25 + 30 = 161.25 wins, with a warning.
	FRoundPreparingTimings Long = FixtureTimings();
	Long.TravelLoadTimeoutSec = F6TravelLoadTimeoutLongSec;
	const StealthRoundFormulas::FWatchdogBound LongBound = StealthRoundFormulas::ComputePreparingWatchdog(Long);
	TestEqual(TEXT("T_needed with TravelLoadTimeoutSec 120"), LongBound.NeededSec, F6ExpectedNeededLongSec, F6Tolerance);
	TestEqual(TEXT("the effective watchdog is T_needed"), LongBound.EffectiveSec, F6ExpectedNeededLongSec, F6Tolerance);
	TestTrue(TEXT("a warning when T_needed wins"), LongBound.bNeededWins);

	// The boundary: a tie gives no warning, 0.01 below gives one.
	FRoundPreparingTimings Tie = FixtureTimings();
	Tie.PreparingWatchdogSec = Default.NeededSec;
	TestFalse(TEXT("PreparingWatchdogSec = T_needed: no warning"), StealthRoundFormulas::ComputePreparingWatchdog(Tie).bNeededWins);
	FRoundPreparingTimings Below = FixtureTimings();
	Below.PreparingWatchdogSec = Default.NeededSec - F6BoundaryOffsetSec;
	TestTrue(TEXT("PreparingWatchdogSec = T_needed - 0.01: one warning"), StealthRoundFormulas::ComputePreparingWatchdog(Below).bNeededWins);

	// Round evaluates F6 at freeze: the deadline before OnTuningReady is the configured one, after it the effective one.
	{
		FRoundRuleParams Params = FRoundExitBRig::DefaultParams();
		Params.Preparing = Long;
		FRoundExitBRig Rig(Params);
		int32 Warnings = 0;
		Rig.Core->OnWatchdogWarning = [&Warnings](const StealthRoundFormulas::FWatchdogBound&) { ++Warnings; };
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.EnterPreparing();
		const double Entered = Rig.Clock.GetServerTimeSeconds();
		TestEqual(TEXT("before freeze: Preparing entry + PreparingWatchdogSec"), Rig.Core->GetPreparing().GetWatchdogDeadline(), Entered + F6PreparingWatchdogSec, F6Tolerance);
		TestEqual(TEXT("no warning before freeze"), Warnings, 0);
		for (int32 Frame = 0; Frame < 10; ++Frame)
		{
			Rig.Frame();
		}
		TestEqual(TEXT("not evaluated per frame"), Warnings, 0);
		Rig.ScriptPreparingSequence();
		Rig.Core->NotifyTuningReady(PreparingRigTuningHash);
		TestEqual(TEXT("at freeze: Preparing entry + PreparingWatchdogEff"), Rig.Core->GetPreparing().GetWatchdogDeadline(), Entered + F6ExpectedNeededLongSec, F6Tolerance);
		TestEqual(TEXT("one warning at freeze"), Warnings, 1);
		Rig.Core->NotifyTuningReady(PreparingRigTuningHash);
		TestEqual(TEXT("F6 is evaluated once"), Warnings, 1);
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
