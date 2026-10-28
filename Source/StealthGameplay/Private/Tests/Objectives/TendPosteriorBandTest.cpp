// Objectives Chain story oc-017 AC2 (AC-OC-21 unit part [U]; F3): StealthGame.Objectives.TendPosteriorBand.
// GIVEN R_full 0.40/min (F1, T_round 9 min), NpcCount 50, completion share c 0.75 and r_N 0.0046 THEN
// P_full = R_full / (R_full + N_npc * r_N * c) is in [0.65, 0.75], and stays there for T_round 7 and 11 min; GIVEN Crowd's
// registered CrowdDecoyTendIntervalSec (13,125 s, read through the schema registry's defaults, not hardcoded) THEN the computed
// P_full is in band; a regression to 18,750 s gives 0.77 and fails (negative control). The inverse returns the rate it came from.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesMetrics.h"
#include "Server/StealthTuningSubstructs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesTendPosteriorBandTest, "StealthGame.Objectives.TendPosteriorBand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesTendPosteriorBandTest::RunTest(const FString& Parameters)
{
	constexpr double NFull = 3.6; // F1's full-length tends per round
	constexpr int32 NpcCount = 50;
	constexpr double Share = 0.75;

	// The stated inputs.
	const double P9 = FObjectivesMetrics::FullTendPosterior(0.40, NpcCount, 0.0046, Share);
	TestTrue(*FString::Printf(TEXT("R_full 0.40, r_N 0.0046: P_full %.4f in [0.65, 0.75]"), P9), FObjectivesMetrics::IsInPosteriorBand(P9));
	for (const double TRound : { 7.0, 9.0, 11.0 })
	{
		const double P = FObjectivesMetrics::FullTendPosterior(NFull / TRound, NpcCount, 0.0046, Share);
		AddInfo(FString::Printf(TEXT("OC17 T_round %.0f min: R_full %.3f, P_full %.4f"), TRound, NFull / TRound, P));
		TestTrue(*FString::Printf(TEXT("T_round %.0f min stays in band (%.4f)"), TRound, P), FObjectivesMetrics::IsInPosteriorBand(P));
	}

	// Crowd's registered decoy interval (the registry's default, not a literal here).
	const double Interval = FCrowdTuning_Server().CrowdDecoyTendIntervalSec;
	const double Rate = FObjectivesMetrics::DecoyRatePerNpcPerMinute(Interval);
	const double PRegistered = FObjectivesMetrics::FullTendPosterior(NFull / 9.0, NpcCount, Rate, Share);
	AddInfo(FString::Printf(TEXT("OC17 registered CrowdDecoyTendIntervalSec %.0f s: r_N %.5f per NPC-minute, P_full %.4f"), Interval, Rate, PRegistered));
	TestEqual(TEXT("the registered interval is 13,125 s"), Interval, 13125.0);
	TestTrue(TEXT("the registered rate gives P_full in band"), FObjectivesMetrics::IsInPosteriorBand(PRegistered));

	// Negative control: the old 18,750 s interval gives 0.77 and fails.
	const double PRegressed = FObjectivesMetrics::FullTendPosterior(NFull / 9.0, NpcCount, FObjectivesMetrics::DecoyRatePerNpcPerMinute(18750.0), Share);
	TestTrue(*FString::Printf(TEXT("control: 18,750 s gives about 0.77 (%.4f)"), PRegressed), FMath::IsNearlyEqual(PRegressed, 0.77, 0.005));
	TestFalse(TEXT("control: and fails the band"), FObjectivesMetrics::IsInPosteriorBand(PRegressed));

	// The inverse: r_N for P_full returns the rate the posterior came from; band edges are inclusive.
	const double Back = FObjectivesMetrics::DecoyRateForPosterior(NFull / 9.0, NpcCount, Share, PRegistered);
	TestTrue(TEXT("the inverse returns r_N"), FMath::IsNearlyEqual(Back, Rate, 1e-9));
	TestTrue(TEXT("0.65 is in band"), FObjectivesMetrics::IsInPosteriorBand(0.65));
	TestTrue(TEXT("0.75 is in band"), FObjectivesMetrics::IsInPosteriorBand(0.75));
	TestFalse(TEXT("0.7501 is not"), FObjectivesMetrics::IsInPosteriorBand(0.7501));
	TestFalse(TEXT("0.6499 is not"), FObjectivesMetrics::IsInPosteriorBand(0.6499));
	return true;
}

#endif
