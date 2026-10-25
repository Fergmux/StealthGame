// Story tdc-051 (production/epics/tuning-data-config/story-051-fallback-to-schema.md), GDD AC-6b (CR-6):
// (1) the requested preset fails and the injectable loader returns a corrupted Default: the schema defaults
// apply with FallbackLevel 2; (2) Default itself is requested and corrupted: the repeated Default rung is
// skipped, FallbackLevel is 2 and PresetId is SchemaDefaults. "Corrupted" is the ITuningPresetSource
// definition (the source produces no value set). In both cases every active value equals the generated
// table's Default column (not any asset) and the report records the fallback.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/StealthValidatedTuning.h"
#include "Server/Testing/StealthLogCapture.h"
#include "Tests/Tuning/StealthTuningFakePresetSource.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FTuningFallbackToSchemaTest, FStealthLogCaptureTestBase,
	"StealthGame.Tuning.FallbackToSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningFallbackToSchemaTest::RunTest(const FString& Parameters)
{
	const TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();
	if (!TestTrue(TEXT("the generated schema has rows (not a bootstrap build)"), Schema.Num() > 0))
	{
		return false;
	}
	const FName DefaultId = FTuningPipelineCore::DefaultPresetId();
	const FName SchemaId = FTuningPipelineCore::SchemaDefaultsPresetId();

	const auto ExpectSchemaDefaults = [this, Schema, SchemaId](const TCHAR* Case, const FTuningResolveResult& Result)
	{
		if (!TestFalse(FString::Printf(TEXT("%s: the pipeline did not fail"), Case), Result.IsFailed()))
		{
			return;
		}
		TestEqual(FString::Printf(TEXT("%s: FallbackLevel 2"), Case), static_cast<int32>(Result.FallbackLevel), static_cast<int32>(ETuningFallback::SchemaDefaults));
		TestEqual(FString::Printf(TEXT("%s: PresetId SchemaDefaults"), Case), Result.PresetId.ToString(), SchemaId.ToString());
		TestEqual(FString::Printf(TEXT("%s: frozen PresetId SchemaDefaults"), Case), Result.Validated->PresetId().ToString(), SchemaId.ToString());
		TestEqual(FString::Printf(TEXT("%s: frozen FallbackLevel 2"), Case), static_cast<int32>(Result.Validated->FallbackLevel()), 2);
		for (const FTuningSchemaRow& Row : Schema)
		{
			TestEqual(FString::Printf(TEXT("%s: %s equals the schema default"), Case, *FString(Row.Key)), Row.Get(Result.Validated->Values()), Row.Default);
		}
		const FTuningRungReport* Accepted = Result.Report.AcceptedRung();
		TestTrue(FString::Printf(TEXT("%s: the report's accepted rung is the schema defaults"), Case),
			Accepted != nullptr && Accepted->Level == ETuningFallback::SchemaDefaults && Accepted->PresetId == SchemaId);
	};

	// (1) The requested preset fails (I-1) and Default is corrupted.
	{
		FStealthServerTuning Bad;
		Bad.Assassination_Public.DefenceDurationSec = 5.0f;
		Bad.Assassination_Public.DefenceCooldownSec = 35.0f;
		FStealthTuningFakePresetSource Source;
		Source.Add(FName(TEXT("Aggressive")), Bad, false);
		Source.AddCorrupted(DefaultId, true);
		FTuningResolveInput Input;
		Input.RequestedPreset = FName(TEXT("Aggressive"));
		Input.Source = &Source;
		FStealthLogCapture Capture;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		Capture.Stop();
		ExpectSchemaDefaults(TEXT("requested fails, Default corrupted"), Result);
		if (TestEqual(TEXT("three rungs recorded"), Result.Report.Rungs.Num(), 3))
		{
			TestEqual(TEXT("rung 0 rejected"), static_cast<int32>(Result.Report.Rungs[0].Outcome), static_cast<int32>(ETuningRungOutcome::Rejected));
			TestEqual(TEXT("rung 1 (Default) missing/corrupted"), static_cast<int32>(Result.Report.Rungs[1].Outcome), static_cast<int32>(ETuningRungOutcome::Missing));
			TestEqual(TEXT("rung 1 is Default"), Result.Report.Rungs[1].PresetId.ToString(), DefaultId.ToString());
		}
		TestEqual(TEXT("one fallback warning names SchemaDefaults"),
			Capture.GetWarnings().FilterByPredicate([](const FString& Line) { return Line.Contains(TEXT("Tuning fallback")) && Line.Contains(TEXT("SchemaDefaults")); }).Num(), 1);
	}

	// (2) Default itself is requested and corrupted: the repeated rung is skipped.
	{
		FStealthTuningFakePresetSource Source;
		Source.AddCorrupted(DefaultId, true);
		FTuningResolveInput Input;
		Input.RequestedPreset = DefaultId;
		Input.Source = &Source;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		ExpectSchemaDefaults(TEXT("Default requested and corrupted"), Result);
		TestEqual(TEXT("Default loaded once (no repeated rung)"), Source.GetLoadCalls().Num(), 1);
		if (TestEqual(TEXT("two rungs recorded"), Result.Report.Rungs.Num(), 2))
		{
			TestEqual(TEXT("rung 0 is Default at level 1"), static_cast<int32>(Result.Report.Rungs[0].Level), static_cast<int32>(ETuningFallback::Default));
		}
	}

	// Same with the request spelled in another case (FName is case-insensitive, CR-4): still one Default load.
	{
		FStealthTuningFakePresetSource Source;
		Source.AddCorrupted(DefaultId, true);
		FTuningResolveInput Input;
		Input.RequestedPreset = FName(TEXT("default"));
		Input.Source = &Source;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		TestTrue(TEXT("'default' requested: FallbackLevel 2"), !Result.IsFailed() && Result.FallbackLevel == ETuningFallback::SchemaDefaults);
		TestEqual(TEXT("'default' requested: Default loaded once"), Source.GetLoadCalls().Num(), 1);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
