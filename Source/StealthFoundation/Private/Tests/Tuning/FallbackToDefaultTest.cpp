// Story tdc-050 (production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md), GDD AC-6a (CR-6):
// a requested preset violating I-1 (DefenceDurationSec 5, DefenceCooldownSec 35: 35 < 8 x 5) is rejected as a
// whole, Default is active and FallbackLevel is 1. Also: the report names I-1; none of the rejected preset's
// other keys leaks into the active values (no partial application); a missing requested preset lands on the
// same rung; a substituted schema view is honoured (the seam story tdc-053 uses); the frozen set carries the
// F-2 hashes of the active values (story tdc-045 hasher). The fault sink is not wired into the core (Resolve
// has no fault path; TuningSnapshotFault is the service's, story tdc-079), so there is no teardown check here.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningHasher.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/StealthTuningVersion.h"
#include "Server/StealthValidatedTuning.h"
#include "Server/Testing/StealthLogCapture.h"
#include "Tests/Tuning/StealthTuningFakePresetSource.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FTuningFallbackToDefaultTest, FStealthLogCaptureTestBase,
	"StealthGame.Tuning.FallbackToDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningFallbackToDefaultTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the generated schema has rows (not a bootstrap build)"), GetTuningSchema().Num() > 0))
	{
		return false;
	}
	const FName Requested(TEXT("Aggressive"));
	const FName DefaultId = FTuningPipelineCore::DefaultPresetId();

	// The requested preset breaks I-1 and also differs from Default in two other keys (inside the safe ranges).
	FStealthServerTuning Bad;
	Bad.Assassination_Public.DefenceDurationSec = 5.0f;
	Bad.Assassination_Public.DefenceCooldownSec = 35.0f;
	Bad.Objectives_Public.ObjectiveInteractSec = 4.0f;
	Bad.Round_Server.TargetRoundMinutes = 10.0f;
	const FStealthServerTuning Default;

	FStealthTuningFakePresetSource Source;
	Source.Add(Requested, Bad, false);
	Source.Add(DefaultId, Default, true);

	// AC-6a.
	{
		FTuningResolveInput Input;
		Input.RequestedPreset = Requested;
		Input.Source = &Source;
		FStealthLogCapture Capture;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		Capture.Stop();

		if (!TestFalse(TEXT("the pipeline did not fail"), Result.IsFailed()))
		{
			return false;
		}
		TestEqual(TEXT("FallbackLevel is 1"), static_cast<int32>(Result.FallbackLevel), static_cast<int32>(ETuningFallback::Default));
		TestEqual(TEXT("Default is active"), Result.PresetId.ToString(), DefaultId.ToString());
		TestEqual(TEXT("the frozen set's PresetId is Default"), Result.Validated->PresetId().ToString(), DefaultId.ToString());
		TestEqual(TEXT("the frozen set records FallbackLevel 1"), static_cast<int32>(Result.Validated->FallbackLevel()), 1);
		TestEqual(TEXT("Revision is 0"), Result.Validated->Revision(), 0);
		TestEqual(TEXT("SchemaVersion is TuningSchemaVersion"), Result.Validated->SchemaVersion(), TuningSchemaVersion);

		if (TestEqual(TEXT("two rungs tried"), Result.Report.Rungs.Num(), 2))
		{
			const FTuningRungReport& First = Result.Report.Rungs[0];
			TestEqual(TEXT("rung 0 is the requested preset"), First.PresetId.ToString(), Requested.ToString());
			TestEqual(TEXT("rung 0 rejected"), static_cast<int32>(First.Outcome), static_cast<int32>(ETuningRungOutcome::Rejected));
			TestEqual(TEXT("rung 1 accepted"), static_cast<int32>(Result.Report.Rungs[1].Outcome), static_cast<int32>(ETuningRungOutcome::Accepted));
			TestTrue(TEXT("the report names I-1 as the rejection"), Result.Report.HasFinding("I-1", ETuningCheck::Reject));
			TestEqual(TEXT("I-1 is the only rejection of rung 0"),
				First.Findings.FilterByPredicate([](const FTuningInvariantFinding& F) { return F.Result == ETuningCheck::Reject; }).Num(), 1);
		}
		TestTrue(TEXT("AcceptedRung is Default"), Result.Report.AcceptedRung() != nullptr && Result.Report.AcceptedRung()->PresetId == DefaultId);

		// No partial application: every active value equals Default's, none the rejected preset's.
		for (const FTuningSchemaRow& Row : GetTuningSchema())
		{
			TestEqual(FString::Printf(TEXT("%s equals Default"), *FString(Row.Key)), Row.Get(Result.Validated->Values()), Row.Get(Default));
		}
		TestEqual(TEXT("TuningHash is the F-2 hash of the active values"), Result.Validated->TuningHash(), FTuningHasher::Hash(Default, TuningSchemaVersion, false));
		TestEqual(TEXT("ClientHash is the F-2 public hash of the active values"), Result.Validated->ClientHash(), FTuningHasher::Hash(Default, TuningSchemaVersion, true));

		const TArray<FString> Rejections = Capture.GetWarnings().FilterByPredicate([](const FString& Line) { return FStealthLogCapture::ContainsToken(Line, TEXT("I-1")); });
		TestEqual(TEXT("one warning names I-1"), Rejections.Num(), 1);
		TestEqual(TEXT("the source was asked for the requested preset, then Default"), Source.GetLoadCalls().Num(), 2);
	}

	// A missing requested preset takes the same rung.
	{
		FTuningResolveInput Input;
		Input.RequestedPreset = FName(TEXT("NoSuchPreset"));
		Input.Source = &Source;
		FStealthLogCapture Capture;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		Capture.Stop();
		TestFalse(TEXT("missing: the pipeline did not fail"), Result.IsFailed());
		TestEqual(TEXT("missing: FallbackLevel is 1"), static_cast<int32>(Result.FallbackLevel), 1);
		TestEqual(TEXT("missing: Default is active"), Result.PresetId.ToString(), DefaultId.ToString());
		TestTrue(TEXT("missing: rung 0 recorded as Missing"), Result.Report.Rungs.Num() == 2 && Result.Report.Rungs[0].Outcome == ETuningRungOutcome::Missing);
	}

	// A valid requested preset is used as is (FallbackLevel 0), so the ladder is not always falling.
	{
		FStealthServerTuning Good = Bad;
		Good.Assassination_Public.DefenceCooldownSec = 40.0f;
		FStealthTuningFakePresetSource GoodSource;
		GoodSource.Add(Requested, Good, false);
		GoodSource.Add(DefaultId, Default, true);
		FTuningResolveInput Input;
		Input.RequestedPreset = Requested;
		Input.Source = &GoodSource;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		TestTrue(TEXT("valid: FallbackLevel 0"), !Result.IsFailed() && Result.FallbackLevel == ETuningFallback::Requested);
		TestTrue(TEXT("valid: the requested values are active"), !Result.IsFailed() && Result.Validated->Values().Assassination_Public.DefenceCooldownSec == 40.0f);
	}

	// A substituted schema view is honoured: with DefenceCooldownSec's hard minimum raised to 40 in the
	// substituted view only, F-1 clamps the requested 35 to 40, I-1 then passes and the requested preset is
	// active at FallbackLevel 0 (with the real schema the same preset is rejected, above).
	{
		TArray<FTuningSchemaRow> Substituted(GetTuningSchema().GetData(), GetTuningSchema().Num());
		FTuningSchemaRow* Cooldown = Substituted.FindByPredicate([](const FTuningSchemaRow& Row) { return FCStringAnsi::Strcmp(Row.Key, "DefenceCooldownSec") == 0; });
		if (TestNotNull(TEXT("DefenceCooldownSec row"), Cooldown))
		{
			Cooldown->HardMin = 40.0;
			FTuningResolveInput Input;
			Input.RequestedPreset = Requested;
			Input.Source = &Source;
			Input.Schema = Substituted;
			FStealthLogCapture Capture;
			const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
			Capture.Stop();
			TestTrue(TEXT("substituted schema: the requested preset passes at FallbackLevel 0"), !Result.IsFailed() && Result.FallbackLevel == ETuningFallback::Requested);
			TestTrue(TEXT("substituted schema: cooldown clamped to the substituted minimum 40"),
				!Result.IsFailed() && Result.Validated->Values().Assassination_Public.DefenceCooldownSec == 40.0f);
			const FStealthLogMatchResult Clamp = Capture.ExpectExactlyOneWarning(TEXT("DefenceCooldownSec"), TEXT("35"), TEXT("40"));
			TestTrue(Clamp.Describe(), Clamp.Passed());
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
