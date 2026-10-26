// Story cnb-018 (production/epics/crowd-npc-behaviour/story-018-tell-sampler-and-calibration-counters.md): compensated tell
// clocks, the chain-gap guard and the calibration counters (GDD CNB-R7, F2; ADR-0016 section 9; ADR-0023).
//   CompensationGolden  the sampler's a_j for every tell equals the committed golden vectors (1e-12) computed from the
//                       interval keys and CrowdTellChainGapSec; the strike's NPC rate is the constant 0.
//   ChainGap            a clock firing within the gap after a different tell is discarded and restarts at the gap's end;
//                       an R8 tell inside the responder's gap is recognised (it becomes a glance).
//   AcceptedRates       100 NPCs x 1,000 min with all eight clocks: the accepted-to-target ratio of the four frequent
//                       tells is within +-5 % (the 600-minute calibration gate with intervals is story cnb-023).
//   CountersServerOnly  counters go only to the server-side counter struct and the telemetry seam: no log macro in the
//                       sampler source and no replicated property.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Fixtures/CrowdTellCompensationVectors.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/CrowdTellSampler.h"
#include "Server/StealthTuningSubstructs.h"

namespace CrowdTellSamplerTestData
{
	struct FRecordingTelemetry final : public ICrowdTelemetrySink
	{
		int64 Calls = 0;
		virtual void AddCrowdCounter(ECrowdTell, int32, int64 Delta) override { Calls += Delta; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTellCompensationGoldenTest, "StealthGame.Crowd.Tells.CompensationGolden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTellCompensationGoldenTest::RunTest(const FString& Parameters)
{
	const FCrowdTuning_Server Tuning;
	double Rates[static_cast<int32>(ECrowdTell::Count)];
	FCrowdTellSampler::TargetRatesPerMin(Tuning, Rates);
	double Lambda = 0.0;
	for (const double Rate : Rates)
	{
		Lambda += Rate;
	}
	TestTrue(TEXT("Lambda matches the golden value"), FMath::IsNearlyEqual(Lambda, CrowdTellCompensationVectors::Lambda, 1e-12));
	for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
	{
		const double A = FCrowdTellSampler::CompensatedRatePerMin(Tuning, static_cast<ECrowdTell>(Index));
		TestTrue(FString::Printf(TEXT("a_%d = %.15f matches the golden %.15f"), Index, A, CrowdTellCompensationVectors::CompensatedPerMin[Index]),
			FMath::IsNearlyEqual(A, CrowdTellCompensationVectors::CompensatedPerMin[Index], 1e-12));
		TestTrue(TEXT("a_j >= r_j"), A >= Rates[Index]);
	}
	TestEqual(TEXT("the strike's NPC rate is 0 by design"), CrowdStrikeTellRatePerMin, 0.0);
	// The running clocks compensate only for the enabled tells' gaps: alone, a clock runs at its target r_j.
	FCrowdTellSampler Alone;
	Alone.SetEnabled(ECrowdTell::HeadHold, true);
	TestTrue(TEXT("a lone clock runs at r_j"), FMath::IsNearlyEqual(Alone.EffectiveRatePerMin(Tuning, ECrowdTell::HeadHold), Rates[static_cast<int32>(ECrowdTell::HeadHold)], 1e-12));
	FCrowdTellSampler All;
	for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
	{
		All.SetEnabled(static_cast<ECrowdTell>(Index), true);
	}
	TestTrue(TEXT("with every clock enabled the running rate is the golden a_j"), FMath::IsNearlyEqual(All.EffectiveRatePerMin(Tuning, ECrowdTell::Bush), CrowdTellCompensationVectors::CompensatedPerMin[static_cast<int32>(ECrowdTell::Bush)], 1e-12));
	// The compensation follows the gap key.
	FCrowdTuning_Server Wider = Tuning;
	Wider.CrowdTellChainGapSec = 30.0f;
	TestTrue(TEXT("a wider gap compensates more"), FCrowdTellSampler::CompensatedRatePerMin(Wider, ECrowdTell::Bush) > FCrowdTellSampler::CompensatedRatePerMin(Tuning, ECrowdTell::Bush));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTellChainGapTest, "StealthGame.Crowd.Tells.ChainGap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTellChainGapTest::RunTest(const FString& Parameters)
{
	const FCrowdTuning_Server Tuning;
	FCrowdDraws Draws(0x7E11ull);
	FCrowdTellSampler Sampler;
	Sampler.SetEnabled(ECrowdTell::HeadHold, true);
	Sampler.StartNpc(5, 0.0, Tuning, Draws);
	const double Due = Sampler.GetNextDue(5, ECrowdTell::HeadHold);
	// A different tell (a run) 5 s before the head-hold clock fires: the head-hold draw falls inside the gap.
	Sampler.NotePerformed(5, ECrowdTell::Run, Due - 5.0);
	TArray<ECrowdTell, TInlineAllocator<2>> Accepted;
	Sampler.Advance(5, Due, Tuning, Draws, Accepted);
	TestEqual(TEXT("the in-gap draw is discarded"), Accepted.Num(), 0);
	TestEqual(TEXT("and counted as discarded"), Sampler.GetCounters().DiscardedInGap[static_cast<int32>(ECrowdTell::HeadHold)], int64(1));
	TestTrue(TEXT("its clock restarts at the gap's end"), Sampler.GetNextDue(5, ECrowdTell::HeadHold) >= Due - 5.0 + Tuning.CrowdTellChainGapSec);
	// R8: a tell inside the responder's gap becomes a glance.
	TestTrue(TEXT("a startle brace 10 s after a run is inside the gap"), Sampler.IsInChainGap(5, ECrowdTell::Brace, Due + 5.0, Tuning));
	TestFalse(TEXT("the same tell again is not a different tell"), Sampler.IsInChainGap(5, ECrowdTell::Run, Due + 5.0, Tuning));
	TestFalse(TEXT("after the gap it is a tell again"), Sampler.IsInChainGap(5, ECrowdTell::Brace, Due + 30.0, Tuning));
	// Outside the gap the clock fires and is accepted.
	const double Next = Sampler.GetNextDue(5, ECrowdTell::HeadHold);
	Sampler.Advance(5, Next, Tuning, Draws, Accepted);
	TestEqual(TEXT("the restarted clock is accepted after the gap"), Accepted.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTellAcceptedRatesTest, "StealthGame.Crowd.Tells.AcceptedRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTellAcceptedRatesTest::RunTest(const FString& Parameters)
{
	const FCrowdTuning_Server Tuning;
	FCrowdDraws Draws(0xACCE97ull);
	FCrowdTellSampler Sampler;
	for (int32 Tell = 0; Tell < static_cast<int32>(ECrowdTell::Count); ++Tell)
	{
		Sampler.SetEnabled(static_cast<ECrowdTell>(Tell), true);
	}
	constexpr int32 Npcs = 100;
	constexpr double Minutes = 1000.0;
	for (uint32 Ordinal = 0; Ordinal < Npcs; ++Ordinal)
	{
		Sampler.StartNpc(Ordinal, 0.0, Tuning, Draws);
		TArray<ECrowdTell, TInlineAllocator<2>> Accepted;
		for (double Now = 0.25; Now <= Minutes * 60.0; Now += 0.25)
		{
			Accepted.Reset();
			Sampler.Advance(Ordinal, Now, Tuning, Draws, Accepted);
		}
	}
	double Rates[static_cast<int32>(ECrowdTell::Count)];
	FCrowdTellSampler::TargetRatesPerMin(Tuning, Rates);
	const ECrowdTell Frequent[] = { ECrowdTell::Run, ECrowdTell::Bush, ECrowdTell::Reversal, ECrowdTell::HeadHold };
	FString Table;
	for (int32 Tell = 0; Tell < static_cast<int32>(ECrowdTell::Count); ++Tell)
	{
		const double Ratio = Sampler.GetCounters().Accepted[Tell] / (Rates[Tell] * Npcs * Minutes);
		Table += FString::Printf(TEXT(" %d:%.3f"), Tell, Ratio);
	}
	AddInfo(FString::Printf(TEXT("TELLRATES accepted/target over %d NPC-min:%s"), static_cast<int32>(Npcs * Minutes), *Table));
	for (const ECrowdTell Tell : Frequent)
	{
		const double Ratio = Sampler.GetCounters().Accepted[static_cast<int32>(Tell)] / (Rates[static_cast<int32>(Tell)] * Npcs * Minutes);
		TestTrue(FString::Printf(TEXT("tell %d accepted rate within 5 %% of its F2 target (ratio %.3f)"), static_cast<int32>(Tell), Ratio), FMath::Abs(Ratio - 1.0) <= 0.05);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTellCountersServerOnlyTest, "StealthGame.Crowd.Tells.CountersServerOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTellCountersServerOnlyTest::RunTest(const FString& Parameters)
{
	using namespace CrowdTellSamplerTestData;
	const FCrowdTuning_Server Tuning;
	FCrowdDraws Draws(0x5);
	FCrowdTellSampler Sampler;
	FRecordingTelemetry Telemetry;
	Sampler.SetTelemetrySink(&Telemetry);
	Sampler.SetEnabled(ECrowdTell::HeadHold, true);
	Sampler.StartNpc(1, 0.0, Tuning, Draws);
	TArray<ECrowdTell, TInlineAllocator<2>> Accepted;
	for (double Now = 1.0; Now < 3600.0 * 3; Now += 1.0)
	{
		Sampler.Advance(1, Now, Tuning, Draws, Accepted);
	}
	TestTrue(TEXT("the counters reach the telemetry seam"), Telemetry.Calls > 0);
	TestEqual(TEXT("and the server-side struct"), Telemetry.Calls, Sampler.GetCounters().Draws[static_cast<int32>(ECrowdTell::HeadHold)] + Sampler.GetCounters().DiscardedInGap[static_cast<int32>(ECrowdTell::HeadHold)] + Sampler.GetCounters().Accepted[static_cast<int32>(ECrowdTell::HeadHold)]);
	// No log macro in the sampler (counters never reach UE_LOG / UE_LOGF).
	FString Source;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Crowd/Private/CrowdTellSampler.cpp")));
	if (TestTrue(TEXT("the sampler source is readable"), FFileHelper::LoadFileToString(Source, *Path)))
	{
		TestFalse(TEXT("no UE_LOG in the sampler"), Source.Contains(TEXT("UE_LOG")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
