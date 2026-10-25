// Story tdc-049 (production/epics/tuning-data-config/story-049-safe-range-warn-only.md), GDD AC-26 (CR-15.5):
// a value inside its hard range but outside its safe range is kept, logs exactly one safe-range warning naming
// the key, and the preset is accepted. The F-1 stage is checked in isolation, and (story tdc-050 landed in the
// same batch) the acceptance is also asserted end to end through Resolve(): FallbackLevel 0, NpcCount 60.
// Edge cases: a value exactly on a safe bound gives no warning; a value outside the hard range still clamps
// with its clamp warning only (no second, safe-range warning for the clamped value: one warning per key).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/StealthValidatedTuning.h"
#include "Server/Testing/StealthLogCapture.h"
#include "Tests/Tuning/StealthTuningFakePresetSource.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FTuningSafeRangeWarnOnlyTest, FStealthLogCaptureTestBase,
	"StealthGame.Tuning.SafeRangeWarnOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningSafeRangeWarnOnlyTest::RunTest(const FString& Parameters)
{
	const TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();
	if (!TestTrue(TEXT("the generated schema has rows (not a bootstrap build)"), Schema.Num() > 0))
	{
		return false;
	}
	const FTuningF1Options NoFlag;

	// F-1 stage: NpcCount 60 (hard 6-100, safe 45-50) is kept with one safe-range warning naming the key.
	{
		FStealthServerTuning Values;
		Values.Crowd_Server.NpcCount = 60;
		FStealthLogCapture Capture;
		FTuningF1Report Report;
		FTuningPipelineCore::RunF1(Schema, Values, NoFlag, Report);
		Capture.Stop();
		TestEqual(TEXT("NpcCount 60 stays 60"), Values.Crowd_Server.NpcCount, 60);
		TestEqual(TEXT("F-1 changed nothing"), Report.Changes.Num(), 0);
		if (TestEqual(TEXT("one safe-range note"), Report.SafeRange.Num(), 1))
		{
			TestEqual(TEXT("the note names NpcCount"), FString(Report.SafeRange[0].Key), FString(TEXT("NpcCount")));
			TestEqual(TEXT("the note keeps the value 60"), Report.SafeRange[0].Value, 60.0);
		}
		TestEqual(TEXT("exactly one warning in total"), Capture.GetWarnings().Num(), 1);
		const FStealthLogMatchResult Match = Capture.ExpectExactlyOneWarning(TEXT("NpcCount"), TEXT("60"), TEXT("safe"));
		TestTrue(Match.Describe(), Match.Passed());
	}

	// Exactly on a safe bound (45 and 50): no warning.
	for (const int32 OnBound : { 45, 50 })
	{
		FStealthServerTuning Values;
		Values.Crowd_Server.NpcCount = OnBound;
		FStealthLogCapture Capture;
		FTuningF1Report Report;
		FTuningPipelineCore::RunF1(Schema, Values, NoFlag, Report);
		Capture.Stop();
		TestEqual(FString::Printf(TEXT("NpcCount %d on a safe bound: no note"), OnBound), Report.SafeRange.Num(), 0);
		TestEqual(FString::Printf(TEXT("NpcCount %d on a safe bound: no warning"), OnBound), Capture.GetWarnings().Num(), 0);
	}

	// Outside the hard range: still clamped (story tdc-046 behaviour), with the clamp warning only.
	{
		FStealthServerTuning Values;
		Values.Crowd_Server.NpcCount = 0;
		FStealthLogCapture Capture;
		FTuningF1Report Report;
		FTuningPipelineCore::RunF1(Schema, Values, NoFlag, Report);
		Capture.Stop();
		TestEqual(TEXT("NpcCount 0 clamps to 6"), Values.Crowd_Server.NpcCount, 6);
		TestEqual(TEXT("one clamp change"), Report.Changes.Num(), 1);
		TestEqual(TEXT("no safe-range note for the clamped value (6 is outside safe, judged by the clamp warning only)"), Report.SafeRange.Num(), 0);
		TestEqual(TEXT("exactly one warning (the clamp)"), Capture.GetWarnings().Num(), 1);
		const FStealthLogMatchResult Match = Capture.ExpectExactlyOneWarning(TEXT("NpcCount"), TEXT("0"), TEXT("6"));
		TestTrue(Match.Describe(), Match.Passed());
	}

	// End to end: a non-selectable preset with NpcCount 60 is accepted at FallbackLevel 0 with the value kept.
	{
		FStealthServerTuning Preset;
		Preset.Crowd_Server.NpcCount = 60;
		FStealthTuningFakePresetSource Source;
		Source.Add(FName(TEXT("Wide")), Preset, false);
		Source.Add(FTuningPipelineCore::DefaultPresetId(), FStealthServerTuning(), true);
		FTuningResolveInput Input;
		Input.RequestedPreset = FName(TEXT("Wide"));
		Input.Source = &Source;

		FStealthLogCapture Capture;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		Capture.Stop();
		if (TestFalse(TEXT("the pipeline did not fail"), Result.IsFailed()))
		{
			TestEqual(TEXT("FallbackLevel 0"), static_cast<int32>(Result.FallbackLevel), static_cast<int32>(ETuningFallback::Requested));
			TestEqual(TEXT("the requested preset is active"), Result.PresetId.ToString(), FString(TEXT("Wide")));
			TestEqual(TEXT("NpcCount stays 60 in the frozen set"), Result.Validated->Values().Crowd_Server.NpcCount, 60);
		}
		TestEqual(TEXT("one rung tried"), Result.Report.Rungs.Num(), 1);
		TestEqual(TEXT("exactly one warning in the whole resolve (the safe-range note)"), Capture.GetWarnings().Num(), 1);
		const FStealthLogMatchResult Match = Capture.ExpectExactlyOneWarning(TEXT("NpcCount"), TEXT("60"), TEXT("safe"));
		TestTrue(Match.Describe(), Match.Passed());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
