// Story tdc-053 (production/epics/tuning-data-config/story-053-fallback-failed.md), GDD AC-6c (CR-6): every
// rung fails (requested preset missing, Default corrupted, and the schema defaults made to break I-1 through
// the substituted-schema seam of story tdc-050: DefenceDurationSec 5 with DefenceCooldownSec 35), so the
// pipeline is Failed (a null frozen set), FStubRound (story tdc-052) receives OnTuningFailed(LoadFailed)
// through a test-local adapter, no OnTuningReady is delivered, and TuningLoadFailed is logged (observed once
// per failed resolve; the GDD fixes no count). Negative control: with the unsubstituted schema the same
// inputs do not end Failed (the schema-defaults rung accepts), so a seam that did nothing would be caught.
// The core has no fault path (no fault sink is wired into Resolve), so there is no teardown fault check.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/StealthValidatedTuning.h"
#include "Server/Testing/StealthLogCapture.h"
#include "Server/Testing/StealthNegativeControl.h"
#include "Server/Testing/StealthStubRound.h"
#include "Tests/Tuning/StealthTuningFakePresetSource.h"

namespace FallbackFailedTestData
{
	/** The service's hand-off, test-local (the real facade is story tdc-066): a result to the Round receivers. */
	void DeliverToRound(const FTuningResolveResult& Result, FStubRound& Round)
	{
		if (Result.IsFailed())
		{
			Round.OnTuningFailed(ETuningFailReason::LoadFailed);
		}
		else
		{
			Round.OnTuningReady(Result.Validated->TuningHash());
		}
	}

	/** A copy of the generated schema whose defaults break I-1 (DefenceDurationSec 5, DefenceCooldownSec 35). */
	TArray<FTuningSchemaRow> InvalidDefaultsSchema()
	{
		TArray<FTuningSchemaRow> Rows(GetTuningSchema().GetData(), GetTuningSchema().Num());
		for (FTuningSchemaRow& Row : Rows)
		{
			if (FCStringAnsi::Strcmp(Row.Key, "DefenceDurationSec") == 0)
			{
				Row.Default = 5.0;
			}
			else if (FCStringAnsi::Strcmp(Row.Key, "DefenceCooldownSec") == 0)
			{
				Row.Default = 35.0;
			}
		}
		return Rows;
	}

	/** Resolves with a missing requested preset and a corrupted Default, against Schema (empty: generated). */
	FTuningResolveResult ResolveAllPresetsBroken(TConstArrayView<FTuningSchemaRow> Schema)
	{
		FStealthTuningFakePresetSource Source;
		Source.AddCorrupted(FTuningPipelineCore::DefaultPresetId(), true);
		FTuningResolveInput Input;
		Input.RequestedPreset = FName(TEXT("Missing"));
		Input.Source = &Source;
		Input.Schema = Schema;
		return FTuningPipelineCore().Resolve(Input);
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FTuningFallbackFailedTest, FStealthLogCaptureTestBase,
	"StealthGame.Tuning.FallbackFailed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningFallbackFailedTest::RunTest(const FString& Parameters)
{
	using namespace FallbackFailedTestData;

	if (!TestTrue(TEXT("the generated schema has rows (not a bootstrap build)"), GetTuningSchema().Num() > 0))
	{
		return false;
	}
	const TArray<FTuningSchemaRow> Invalid = InvalidDefaultsSchema();

	// Every rung fails.
	{
		FStubRound Round;
		Round.EnterPhase(EStealthStubRoundPhase::Preparing);
		Round.BeginResolve();
		FStealthLogCapture Capture;
		const FTuningResolveResult Result = ResolveAllPresetsBroken(Invalid);
		Capture.Stop();
		DeliverToRound(Result, Round);

		TestTrue(TEXT("the pipeline is Failed"), Result.IsFailed());
		TestTrue(TEXT("no frozen set exists"), !Result.Validated.IsValid());
		TestTrue(TEXT("the report holds no accepted rung"), Result.Report.AcceptedRung() == nullptr);
		if (TestEqual(TEXT("three rungs tried"), Result.Report.Rungs.Num(), 3))
		{
			TestEqual(TEXT("rung 0 missing"), static_cast<int32>(Result.Report.Rungs[0].Outcome), static_cast<int32>(ETuningRungOutcome::Missing));
			TestEqual(TEXT("rung 1 (Default) corrupted"), static_cast<int32>(Result.Report.Rungs[1].Outcome), static_cast<int32>(ETuningRungOutcome::Missing));
			TestEqual(TEXT("rung 2 (schema defaults) rejected"), static_cast<int32>(Result.Report.Rungs[2].Outcome), static_cast<int32>(ETuningRungOutcome::Rejected));
		}
		TestTrue(TEXT("the schema defaults were rejected by I-1"), Result.Report.HasFinding("I-1", ETuningCheck::Reject));

		const TArray<FStealthStubRoundCall>& Calls = Round.GetCalls();
		if (TestEqual(TEXT("the stub recorded phase, BeginResolve and exactly one receipt"), Calls.Num(), 3))
		{
			TestEqual(TEXT("the receipt is OnTuningFailed"), static_cast<int32>(Calls[2].Kind), static_cast<int32>(EStealthStubRoundCall::OnTuningFailed));
			TestEqual(TEXT("the reason is LoadFailed"), static_cast<int32>(Calls[2].Reason), static_cast<int32>(ETuningFailReason::LoadFailed));
		}
		TestFalse(TEXT("no OnTuningReady was delivered"), Calls.ContainsByPredicate([](const FStealthStubRoundCall& C) { return C.Kind == EStealthStubRoundCall::OnTuningReady; }));
		TestEqual(TEXT("the stub round returned to Lobby"), static_cast<int32>(Round.GetPhase()), static_cast<int32>(EStealthStubRoundPhase::Lobby));

		const int32 LoadFailedLines = Capture.GetWarnings().FilterByPredicate([](const FString& Line) { return FStealthLogCapture::ContainsToken(Line, TEXT("TuningLoadFailed")); }).Num();
		TestTrue(FString::Printf(TEXT("TuningLoadFailed is logged (observed %d time(s))"), LoadFailedLines), LoadFailedLines >= 1);
		AddInfo(FString::Printf(TEXT("TuningLoadFailed observed %d time(s) for one failed resolve"), LoadFailedLines));
	}

	// Negative control: the same inputs with the unsubstituted schema do not end Failed.
	{
		FStealthLogCapture Quiet;
		FStealthNegativeControl Control(TEXT("UnsubstitutedSchemaDoesNotFail"));
		FStealthNegativeControl::Report(*this, Control.Run([](FStealthCheckContext& Ctx)
		{
			const FTuningResolveResult Result = ResolveAllPresetsBroken(TConstArrayView<FTuningSchemaRow>());
			Ctx.TestTrue(TEXT("pipeline Failed"), Result.IsFailed());
		}));
		// And positively: the unsubstituted ladder lands on the schema defaults.
		const FTuningResolveResult Result = ResolveAllPresetsBroken(TConstArrayView<FTuningSchemaRow>());
		TestTrue(TEXT("unsubstituted: schema defaults accepted at FallbackLevel 2"), !Result.IsFailed() && Result.FallbackLevel == ETuningFallback::SchemaDefaults);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
