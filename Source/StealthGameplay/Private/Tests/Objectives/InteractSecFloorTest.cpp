// Story oc-001 (production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md), AC1 / AC-OC-11
// InteractSecFloor, through the REAL pipeline (F-1 then the production invariant table):
//   A  ObjectiveInteractSec 1.0 -> the hard floor 2.0 with exactly one clamp warning (F-1; no Objectives finding).
//   B  CrowdAmbientTouchMaxSec 1.8 and ObjectiveInteractSec 2.0 -> exactly one I-OC1 Warn; the value stays 2.0 (never repaired).
// Edge cases: 2.0 exactly gives no floor warning; 1.99 floors to 2.0 with a warning; the I-OC1 boundary is inclusive
// (2.0 >= 1.5 + 0.5 passes), read from the TDC owner-invariant mechanism (story tdc-055: rows are ">=" as the GDD writes them).
// The floored cases lower Crowd's CrowdDecoyEarlyReleaseMaxSec to 1.9 so Crowd's own I-CNB-6 (< ObjectiveInteractSec) does
// not reject the preset first (a value inside its hard range; it adds one Crowd safe-range note, which is not counted here).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesTuningTestSupport.h"
#include "Server/StealthObjectivesInvariants.h"

namespace ObjectivesInteractSecFloorTestData
{
	constexpr float BelowFloorSec = 1.0f;
	constexpr float JustBelowFloorSec = 1.99f;
	constexpr float FloorSec = 2.0f;
	constexpr float LongTouchSec = 1.8f;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectivesInteractSecFloorTest, FStealthLogCaptureTestBase,
	"StealthGame.Objectives.InteractSecFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesInteractSecFloorTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTuningTest;
	using namespace ObjectivesInteractSecFloorTestData;

	// A: 1.0 floors to 2.0 with one warning.
	for (const float Authored : { BelowFloorSec, JustBelowFloorSec })
	{
		const FLocalPresetSource Source = MakeSource([Authored](FStealthServerTuning& V)
		{
			V.Objectives_Public.ObjectiveInteractSec = Authored;
			V.Crowd_Server.CrowdDecoyEarlyReleaseMaxSec = CrowdEarlyReleaseMaxBelowFloorSec;
		});
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = Resolve(Source, Capture);
		const FTuningRungReport* Rung = RequestedRung(Resolved);
		TestEqual(FString::Printf(TEXT("%g: the requested preset is accepted"), Authored), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::Requested));
		const TArray<FTuningF1Change> Changes = ChangesOf(Rung, "ObjectiveInteractSec");
		if (TestEqual(FString::Printf(TEXT("%g: F-1 changed ObjectiveInteractSec once"), Authored), Changes.Num(), 1))
		{
			TestEqual(TEXT("the effective value is the hard floor 2.0"), Changes[0].Effective, static_cast<double>(FloorSec));
			TestEqual(TEXT("the change is a clamp"), static_cast<int32>(Changes[0].Action), static_cast<int32>(ETuningF1Action::Clamped));
		}
		TestEqual(FString::Printf(TEXT("%g: exactly one warning names ObjectiveInteractSec"), Authored), WarningsNaming(Capture, TEXT("ObjectiveInteractSec")), 1);
		if (TestTrue(TEXT("a frozen set exists"), Resolved.Validated.IsValid()))
		{
			TestEqual(TEXT("the frozen set holds 2.0"), Resolved.Validated->Values().Objectives_Public.ObjectiveInteractSec, FloorSec);
		}
		for (const ANSICHAR* Id : ObjectivesIds)
		{
			TestEqual(FString::Printf(TEXT("%g: no %hs finding (2.0 >= 1.5 + 0.5; 1.0 <= 2.0 - 1.0)"), Authored, Id),
				CountFindings(Rung, Id, ETuningCheck::Warn) + CountFindings(Rung, Id, ETuningCheck::Reject), 0);
		}
	}

	// Edge: 2.0 exactly is kept with no floor warning.
	{
		const FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V)
		{
			V.Objectives_Public.ObjectiveInteractSec = FloorSec;
			V.Crowd_Server.CrowdDecoyEarlyReleaseMaxSec = CrowdEarlyReleaseMaxBelowFloorSec;
		});
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = Resolve(Source, Capture);
		const FTuningRungReport* Rung = RequestedRung(Resolved);
		TestEqual(TEXT("2.0 exactly: no F-1 change (no floor warning)"), ChangesOf(Rung, "ObjectiveInteractSec").Num(), 0);
		// The one warning naming the key is the safe-range advisory (2.0 is below the safe 2.9), never a clamp.
		const bool bSafeNote = Rung != nullptr && Rung->F1.SafeRange.ContainsByPredicate([](const FTuningSafeRangeNote& N) { return FCStringAnsi::Strcmp(N.Key, "ObjectiveInteractSec") == 0; });
		TestTrue(TEXT("2.0 exactly: only the safe-range advisory is recorded"), bSafeNote);
		TestEqual(TEXT("2.0 exactly: one warning names the key (the advisory)"), WarningsNaming(Capture, TEXT("ObjectiveInteractSec")), 1);
	}

	// B: a longer ambient touch makes 2.0 too short: one I-OC1 Warn, the value is not repaired.
	{
		const FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V)
		{
			V.Objectives_Public.ObjectiveInteractSec = FloorSec;
			V.Crowd_Server.CrowdAmbientTouchMaxSec = LongTouchSec;
			V.Crowd_Server.CrowdDecoyEarlyReleaseMaxSec = CrowdEarlyReleaseMaxBelowFloorSec;
		});
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = Resolve(Source, Capture);
		const FTuningRungReport* Rung = RequestedRung(Resolved);
		TestEqual(TEXT("B: the preset is kept (a warning, not a reject)"), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::Requested));
		TestEqual(TEXT("B: exactly one I-OC1 Warn"), CountFindings(Rung, "I-OC1", ETuningCheck::Warn), 1);
		TestEqual(TEXT("B: exactly one warning names I-OC1"), WarningsNaming(Capture, TEXT("I-OC1")), 1);
		if (TestTrue(TEXT("B: a frozen set exists"), Resolved.Validated.IsValid()))
		{
			TestEqual(TEXT("B: ObjectiveInteractSec stays 2.0"), Resolved.Validated->Values().Objectives_Public.ObjectiveInteractSec, FloorSec);
			TestEqual(TEXT("B: CrowdAmbientTouchMaxSec stays 1.8"), Resolved.Validated->Values().Crowd_Server.CrowdAmbientTouchMaxSec, LongTouchSec);
		}
	}

	// The I-OC1 boundary on the pure check: exactly touch + 0.5 passes, just under warns.
	{
		FStealthServerTuning V;
		V.Crowd_Server.CrowdAmbientTouchMaxSec = 1.5f;
		V.Objectives_Public.ObjectiveInteractSec = 2.0f;
		TestEqual(TEXT("I-OC1 at 2.0 vs 1.5 + 0.5 passes (inclusive)"), static_cast<int32>(StealthObjectivesInvariants::CheckIOc1(V)), static_cast<int32>(ETuningCheck::Pass));
		V.Objectives_Public.ObjectiveInteractSec = 1.99f;
		TestEqual(TEXT("I-OC1 at 1.99 warns"), static_cast<int32>(StealthObjectivesInvariants::CheckIOc1(V)), static_cast<int32>(ETuningCheck::Warn));
	}
	return true;
}

#endif
