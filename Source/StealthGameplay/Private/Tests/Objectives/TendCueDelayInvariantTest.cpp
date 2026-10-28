// Story oc-001 (production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md), AC3 / AC-OC-32
// TendCueDelay invariant clauses, through the REAL pipeline:
//   case 1  TendCueDelaySec 0.5 with CrowdDecoyEarlyReleaseMinSec 0.8: 0.5 + 0.25 = 0.75 < 0.8, I-OC3 rejects the preset,
//           nothing is repaired (the ladder takes Default)
//   case 2  TendCueDelaySec 2.5 with ObjectiveInteractSec 3.0: the hard range clamps it to 2.0 with one warning, then I-OC3
//           passes (2.0 <= 3.0 - 1.0). The order is clamp, then invariant: an unclamped 2.5 would have rejected.
// Edge: 0.55 is the exact 0.80 boundary (passes); the upper clause fails at 2.0 with ObjectiveInteractSec 2.9 (2.0 > 1.9).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesTuningTestSupport.h"
#include "Server/StealthObjectivesInvariants.h"

namespace ObjectivesTendCueDelayTestData
{
	constexpr float ShortCueSec = 0.5f;
	constexpr float BoundaryCueSec = 0.55f;
	constexpr float DecoyEarlyMinSec = 0.8f;
	constexpr float LongCueSec = 2.5f;
	constexpr float CueHardMaxSec = 2.0f;
	constexpr float HoldSec = 3.0f;
	constexpr float ShortHoldSec = 2.9f;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectivesTendCueDelayInvariantTest, FStealthLogCaptureTestBase,
	"StealthGame.Objectives.TendCueDelayInvariant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesTendCueDelayInvariantTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTuningTest;
	using namespace ObjectivesTendCueDelayTestData;

	// Case 1: rejected, no repair.
	{
		const FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V)
		{
			V.Objectives_Public.TendCueDelaySec = ShortCueSec;
			V.Crowd_Server.CrowdDecoyEarlyReleaseMinSec = DecoyEarlyMinSec;
		});
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = Resolve(Source, Capture);
		const FTuningRungReport* Rung = RequestedRung(Resolved);
		TestEqual(TEXT("case 1: I-OC3 rejects"), CountFindings(Rung, "I-OC3", ETuningCheck::Reject), 1);
		TestEqual(TEXT("case 1: the preset is rejected and Default is used"), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::Default));
		TestEqual(TEXT("case 1: no F-1 change to TendCueDelaySec (0.5 is inside 0.5-2.0; nothing is repaired)"), ChangesOf(Rung, "TendCueDelaySec").Num(), 0);
		if (TestTrue(TEXT("case 1: a frozen set exists"), Resolved.Validated.IsValid()))
		{
			TestEqual(TEXT("case 1: the active set is Default's 1.0, not a repaired 0.55"), Resolved.Validated->Values().Objectives_Public.TendCueDelaySec, 1.0f);
		}
	}

	// Case 2: clamped to 2.0 with one warning, then I-OC3 passes.
	{
		const FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V)
		{
			V.Objectives_Public.TendCueDelaySec = LongCueSec;
			V.Objectives_Public.ObjectiveInteractSec = HoldSec;
		});
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = Resolve(Source, Capture);
		const FTuningRungReport* Rung = RequestedRung(Resolved);
		const TArray<FTuningF1Change> Changes = ChangesOf(Rung, "TendCueDelaySec");
		if (TestEqual(TEXT("case 2: F-1 clamped TendCueDelaySec once"), Changes.Num(), 1))
		{
			TestEqual(TEXT("case 2: clamped to the hard maximum 2.0"), Changes[0].Effective, static_cast<double>(CueHardMaxSec));
		}
		TestEqual(TEXT("case 2: exactly one clamp warning (2.5 -> 2)"), Capture.CountWarnings(TEXT("TendCueDelaySec"), TEXT("2.5"), TEXT("2")), 1);
		TestEqual(TEXT("case 2: exactly one warning names TendCueDelaySec"), WarningsNaming(Capture, TEXT("TendCueDelaySec")), 1);
		TestEqual(TEXT("case 2: I-OC3 passes after the clamp"), CountFindings(Rung, "I-OC3", ETuningCheck::Reject), 0);
		TestEqual(TEXT("case 2: accepted at the requested rung"), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::Requested));

		// The order matters: the pure check on the unclamped value rejects (2.5 > 3.0 - 1.0).
		FStealthServerTuning Unclamped;
		Unclamped.Objectives_Public.TendCueDelaySec = LongCueSec;
		Unclamped.Objectives_Public.ObjectiveInteractSec = HoldSec;
		TestEqual(TEXT("case 2: the unclamped 2.5 would reject (clamp, then invariant)"), static_cast<int32>(StealthObjectivesInvariants::CheckIOc3(Unclamped)), static_cast<int32>(ETuningCheck::Reject));
	}

	// Edges on the pure check.
	{
		FStealthServerTuning V;
		V.Crowd_Server.CrowdDecoyEarlyReleaseMinSec = DecoyEarlyMinSec;
		V.Objectives_Public.TendCueDelaySec = BoundaryCueSec;
		TestEqual(TEXT("0.55 + 0.25 = 0.80 >= 0.8 passes (exact boundary)"), static_cast<int32>(StealthObjectivesInvariants::CheckIOc3(V)), static_cast<int32>(ETuningCheck::Pass));
		V.Objectives_Public.TendCueDelaySec = CueHardMaxSec;
		V.Objectives_Public.ObjectiveInteractSec = ShortHoldSec;
		TestEqual(TEXT("2.0 > 2.9 - 1.0 rejects (upper clause)"), static_cast<int32>(StealthObjectivesInvariants::CheckIOc3(V)), static_cast<int32>(ETuningCheck::Reject));
	}
	return true;
}

#endif
