// Story oc-001 (production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md), AC2 / AC-OC-22
// RivalSignalBound: I-OC2 on W_eff = (Max - Min) + Q (GDD F4) through the REAL pipeline.
//   defaults 30 / 165 / Q 30   W_eff 165, LR_sig <= 2.4 at lambda_c 0.0033, no finding
//   40 / 150 / Q 30            W_eff 140: Warn (the warn clause)
//   60 / 100 / Q 20            W_eff 60: Reject (the reject clause); the ladder falls back to Default, no key repaired
//   120 / 70 / Q 30            Max < Min + 30: Reject
// Edge cases: W_eff exactly 100 warns (not rejected); Max = Min + 30 exactly passes the spread clause; Q 0 is clamped to the
// hard minimum 10 by F-1 before the invariant sees it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesTuningTestSupport.h"
#include "Server/StealthObjectivesInvariants.h"

namespace ObjectivesRivalSignalBoundTestData
{
	struct FCase
	{
		const TCHAR* Name;
		float Min;
		float Max;
		float Q;
		ETuningCheck Expected;
		double WindowSec;
	};

	const FCase Cases[] = {
		{ TEXT("defaults 30/165/30"), 30.0f, 165.0f, 30.0f, ETuningCheck::Pass, 165.0 },
		{ TEXT("40/150/30"), 40.0f, 150.0f, 30.0f, ETuningCheck::Warn, 140.0 },
		{ TEXT("60/100/20"), 60.0f, 100.0f, 20.0f, ETuningCheck::Reject, 60.0 },
		{ TEXT("120/70/30"), 120.0f, 70.0f, 30.0f, ETuningCheck::Reject, -20.0 },
		{ TEXT("W_eff exactly 100 (30/100/30)"), 30.0f, 100.0f, 30.0f, ETuningCheck::Warn, 100.0 },
		{ TEXT("Max = Min + 30 exactly (40/70/90)"), 40.0f, 70.0f, 90.0f, ETuningCheck::Warn, 120.0 },
	};

	constexpr double LambdaMinPerSec = 0.0033;
	constexpr double DefaultLikelihoodRatio = 2.38;
	constexpr double RatioTolerance = 0.01;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectivesRivalSignalBoundTest, FStealthLogCaptureTestBase,
	"StealthGame.Objectives.RivalSignalBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesRivalSignalBoundTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTuningTest;
	using namespace ObjectivesRivalSignalBoundTestData;

	for (const FCase& Case : Cases)
	{
		const FLocalPresetSource Source = MakeSource([&Case](FStealthServerTuning& V)
		{
			V.Objectives_Server.RivalSignalDelayMinSec = Case.Min;
			V.Objectives_Server.RivalSignalDelayMaxSec = Case.Max;
			V.Objectives_Server.RivalSignalReleaseQuantumSec = Case.Q;
		});
		TestEqual(FString::Printf(TEXT("%s: W_eff"), Case.Name), StealthObjectivesInvariants::EffectiveWindowSec(Source.RequestedValues), Case.WindowSec, 1e-6);
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = Resolve(Source, Capture);
		const FTuningRungReport* Rung = RequestedRung(Resolved);
		if (!TestNotNull(FString::Printf(TEXT("%s: the requested rung was tried"), Case.Name), Rung))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s: I-OC2 Warn findings"), Case.Name), CountFindings(Rung, "I-OC2", ETuningCheck::Warn), Case.Expected == ETuningCheck::Warn ? 1 : 0);
		TestEqual(FString::Printf(TEXT("%s: I-OC2 Reject findings"), Case.Name), CountFindings(Rung, "I-OC2", ETuningCheck::Reject), Case.Expected == ETuningCheck::Reject ? 1 : 0);
		if (Case.Expected == ETuningCheck::Reject)
		{
			// TDC CR-6: the preset is rejected as a whole and the ladder takes Default; nothing is repaired.
			TestEqual(FString::Printf(TEXT("%s: rejected, Default is used"), Case.Name), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::Default));
			TestEqual(FString::Printf(TEXT("%s: the requested rung is Rejected"), Case.Name), static_cast<int32>(Rung->Outcome), static_cast<int32>(ETuningRungOutcome::Rejected));
			TestEqual(FString::Printf(TEXT("%s: no F-1 change to the delay keys (no repair)"), Case.Name),
				ChangesOf(Rung, "RivalSignalDelayMinSec").Num() + ChangesOf(Rung, "RivalSignalDelayMaxSec").Num() + ChangesOf(Rung, "RivalSignalReleaseQuantumSec").Num(), 0);
			const FTuningInvariantFinding* Finding = Rung->Findings.FindByPredicate([](const FTuningInvariantFinding& F) { return FCStringAnsi::Strcmp(F.Id, "I-OC2") == 0; });
			TestTrue(FString::Printf(TEXT("%s: the reject clause is named"), Case.Name), Finding != nullptr && Finding->Clause != nullptr && FCStringAnsi::Strcmp(Finding->Clause, StealthObjectivesInvariants::IOc2RejectClause) == 0);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s: accepted at the requested rung"), Case.Name), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::Requested));
		}
		if (Case.Expected == ETuningCheck::Warn)
		{
			const FTuningInvariantFinding* Finding = Rung->Findings.FindByPredicate([](const FTuningInvariantFinding& F) { return FCStringAnsi::Strcmp(F.Id, "I-OC2") == 0; });
			TestTrue(FString::Printf(TEXT("%s: the warn clause is named"), Case.Name), Finding != nullptr && Finding->Clause != nullptr && FCStringAnsi::Strcmp(Finding->Clause, StealthObjectivesInvariants::IOc2WarnClause) == 0);
			TestEqual(FString::Printf(TEXT("%s: one warning names I-OC2"), Case.Name), WarningsNaming(Capture, TEXT("I-OC2")), 1);
		}
	}

	// F4 at the defaults: LR_sig = 2.38 <= 2.4 at lambda_c,min 0.0033.
	const FStealthServerTuning Defaults;
	const double Ratio = StealthObjectivesInvariants::SignalLikelihoodRatio(StealthObjectivesInvariants::EffectiveWindowSec(Defaults), LambdaMinPerSec);
	TestEqual(TEXT("LR_sig at the defaults (GDD F4 example 2.38)"), Ratio, DefaultLikelihoodRatio, RatioTolerance);
	TestTrue(TEXT("LR_sig at the defaults is within the F4 bound 2.4"), Ratio <= StealthObjectivesInvariants::SignalLikelihoodRatioMax);

	// Edge: Q 0 is clamped by F-1 to the hard minimum 10 before I-OC2 reads it (30/165/10: W_eff 145 warns, not 135).
	{
		const FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V) { V.Objectives_Server.RivalSignalReleaseQuantumSec = 0.0f; });
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = Resolve(Source, Capture);
		const TArray<FTuningF1Change> Changes = ChangesOf(RequestedRung(Resolved), "RivalSignalReleaseQuantumSec");
		if (TestEqual(TEXT("Q 0 is clamped by F-1"), Changes.Num(), 1))
		{
			TestEqual(TEXT("Q clamps to the hard minimum 10"), Changes[0].Effective, 10.0);
		}
		TestEqual(TEXT("then I-OC2 sees W_eff 145 and warns"), CountFindings(RequestedRung(Resolved), "I-OC2", ETuningCheck::Warn), 1);
	}
	return true;
}

#endif
