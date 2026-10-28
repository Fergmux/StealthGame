// Story nkp-020 (production/epics/npc-kill-penalty-informants/story-020-penalty-tuning-warnings-and-residual-models.md),
// AC1 / AC-NKP-18 (Penalty CR-11; TDC rows I-NKP-1 and W-NKP-2 to W-NKP-5), Config/Data. Every case is validated through the
// REAL pipeline (FTuningPipelineCore::Resolve over the production invariant table) from a preset built in code: the project
// holds no checked-in preset data for tests (the Foundation tuning tests build presets the same way), so the fixtures are
// the named authoring functions below.
//   I-NKP-1  NpcKillStrikeLimit 1: a non-selectable test preset (bTestAllowsStrikeLimitOne) loads with a warning; a public
//            preset (HostSelectable or Default) is rejected, and the ladder falls back. Through the catalog the flag cannot
//            reach such a preset (the clamp raises 1 to 2); the source here forges the combination to prove the Reject rule.
//   W-NKP-2  InformantRespawnDelaySec 25 with RespawnStarvationSec 10 (45 > 40) warns; the boundary 20 (40) does not.
//   W-NKP-3  StrikeOutReleaseGraceSec 1.5 with AttackAttemptLockoutSec 1.75 warns.
//   W-NKP-4  D 5, DeathFallSec 2.0, NoPawnFadeSec 1.5 (floor 4.7) loads without the warning; the predicate called with the
//            injected out-of-range D = 4.0 returns Warn (the guard cannot fire through the clamped hard ranges).
//   W-NKP-5  ExileCardTimeoutSec 1.0 with ExileBeatSec 0.8 warns (needs 0.40 + 0.35 + 0.8 + 0.3 = 1.85).
// RespawnStarvationSec and NoPawnFadeSec are pending owner keys (not schema rows), so the cases that vary them inject the
// input through StealthPenaltyInvariants::Evaluate; the pipeline reads their registered defaults (10 and 0.5).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/StealthPenaltyInvariants.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthValidatedTuning.h"
#include "Server/Testing/StealthLogCapture.h"

namespace PenaltyTuningWarningsTestData
{
	// Preset names.
	const FName TestPresetId(TEXT("StrikeLimitOneTest"));
	const FName HostPresetId(TEXT("HostSelectableStrikeLimitOne"));

	// Fixture values (the AC's boundary values).
	constexpr int32 StrikeLimitOne = 1;
	constexpr float DowntimeRespawnDelaySec = 25.0f;
	constexpr float DowntimeBoundaryRespawnDelaySec = 20.0f;
	constexpr float ShortGraceSec = 1.5f;
	constexpr float LockoutSec = 1.75f;
	constexpr float FloorRespawnDelaySec = 5.0f;
	constexpr float HardMaxDeathFallSec = 2.0f;
	constexpr double HardMaxNoPawnFadeSec = 1.5;
	constexpr double InjectedOutOfRangeDelaySec = 4.0;
	constexpr float ShortCardTimeoutSec = 1.0f;
	constexpr float DefaultBeatSec = 0.8f;

	/** A test-local preset source: a requested preset, a clean Default, and an optional Default override. */
	class FLocalPresetSource : public ITuningPresetSource
	{
	public:
		FStealthServerTuning RequestedValues;
		FStealthServerTuning DefaultValues;
		bool bRequestedPublic = false;
		bool bRequestedFlag = false;
		bool bDefaultFlag = false;
		FName RequestedId;

		virtual TOptional<FStealthServerTuning> Load(FName PresetId, bool& bOutIsPublic) const override
		{
			if (PresetId == FTuningPipelineCore::DefaultPresetId())
			{
				bOutIsPublic = true;
				return DefaultValues;
			}
			bOutIsPublic = bRequestedPublic;
			return PresetId == RequestedId ? TOptional<FStealthServerTuning>(RequestedValues) : TOptional<FStealthServerTuning>();
		}

		virtual FTuningF1Options GetF1Options(FName PresetId) const override
		{
			FTuningF1Options Options;
			Options.bTestAllowsStrikeLimitOne = PresetId == FTuningPipelineCore::DefaultPresetId() ? bDefaultFlag : bRequestedFlag;
			return Options;
		}
	};

	/** Resolves Source's requested preset (or Default) and stops the capture so only this resolve's warnings count. */
	FTuningResolveResult ResolveWith(const FLocalPresetSource& Source, FName Requested, FStealthLogCapture& Capture)
	{
		FTuningResolveInput Input;
		Input.RequestedPreset = Requested;
		Input.Source = &Source;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		Capture.Stop();
		return Result;
	}

	/** Warnings naming Id as a whole token. */
	int32 WarningsNaming(const FStealthLogCapture& Capture, const ANSICHAR* Id)
	{
		const FString IdText(Id);
		return Capture.GetWarnings().FilterByPredicate([&IdText](const FString& Line) { return FStealthLogCapture::ContainsToken(Line, IdText); }).Num();
	}

	/** A source whose requested preset applies Author on top of the C++ defaults (a non-public, unflagged preset). */
	FLocalPresetSource MakeSource(TFunctionRef<void(FStealthServerTuning&)> Author)
	{
		FLocalPresetSource Source;
		Source.RequestedId = TestPresetId;
		Author(Source.RequestedValues);
		return Source;
	}

	/** True when the accepted requested rung recorded Result for Id. */
	bool RequestedHas(const FTuningResolveResult& Resolved, const ANSICHAR* Id, ETuningCheck Result)
	{
		return Resolved.Report.Rungs.Num() > 0 && Resolved.FallbackLevel == ETuningFallback::Requested && Resolved.Report.HasFinding(Id, Result);
	}

	const ANSICHAR* const PenaltyIds[] = { "I-NKP-1", "W-NKP-2", "W-NKP-3", "W-NKP-4", "W-NKP-5" };
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPenaltyTuningWarningsTest, FStealthLogCaptureTestBase,
	"StealthGame.Tuning.StrikeLimitOneWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyTuningWarningsTest::RunTest(const FString& Parameters)
{
	using namespace PenaltyTuningWarningsTestData;

	// The five rows are registered once each in the production table, owner Penalty.
	for (const ANSICHAR* Id : PenaltyIds)
	{
		int32 Found = 0;
		for (const FTuningInvariant& Row : GetTuningInvariants())
		{
			if (FCStringAnsi::Strcmp(Row.Id, Id) == 0)
			{
				++Found;
				TestTrue(FString::Printf(TEXT("%hs owner is Penalty"), Id), FCStringAnsi::Strcmp(Row.Owner, "Penalty") == 0);
			}
		}
		TestEqual(FString::Printf(TEXT("%hs is registered once in the preset-load table"), Id), Found, 1);
	}

	// Control: the schema-struct defaults raise none of the Penalty rows (15 + 20 = 35 <= 40; 2.0 >= 1.75; 2.9 <= 15; 2.0 >= 1.85).
	{
		FLocalPresetSource Source;
		Source.RequestedId = TestPresetId;
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = ResolveWith(Source, TestPresetId, Capture);
		TestEqual(TEXT("control: the requested preset is accepted"), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::Requested));
		for (const ANSICHAR* Id : PenaltyIds)
		{
			TestFalse(FString::Printf(TEXT("control: no %hs finding"), Id), Resolved.Report.HasFinding(Id, ETuningCheck::Warn) || Resolved.Report.HasFinding(Id, ETuningCheck::Reject));
			TestEqual(FString::Printf(TEXT("control: no warning names %hs"), Id), WarningsNaming(Capture, Id), 0);
		}
	}

	// I-NKP-1, non-selectable test preset: L = 1 loads with exactly one warning.
	{
		FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V) { V.Penalty_Public.NpcKillStrikeLimit = StrikeLimitOne; });
		Source.bRequestedFlag = true;
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = ResolveWith(Source, TestPresetId, Capture);
		TestTrue(TEXT("L = 1 on a test preset is accepted at the requested rung with I-NKP-1 as a Warn"), RequestedHas(Resolved, "I-NKP-1", ETuningCheck::Warn));
		TestFalse(TEXT("L = 1 on a test preset is not a Reject"), Resolved.Report.HasFinding("I-NKP-1", ETuningCheck::Reject));
		TestEqual(TEXT("one warning names I-NKP-1"), WarningsNaming(Capture, "I-NKP-1"), 1);
		if (TestTrue(TEXT("the pipeline produced a frozen set"), Resolved.Validated.IsValid()))
		{
			TestEqual(TEXT("the test preset keeps L = 1"), Resolved.Validated->Values().Penalty_Public.NpcKillStrikeLimit, StrikeLimitOne);
		}
	}

	// I-NKP-1, host-selectable preset: rejected as a whole, the ladder falls to Default.
	{
		FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V) { V.Penalty_Public.NpcKillStrikeLimit = StrikeLimitOne; });
		Source.RequestedId = HostPresetId;
		Source.bRequestedPublic = true;
		Source.bRequestedFlag = true;
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = ResolveWith(Source, HostPresetId, Capture);
		TestTrue(TEXT("L = 1 on a host-selectable preset records I-NKP-1 as a Reject"), Resolved.Report.HasFinding("I-NKP-1", ETuningCheck::Reject));
		TestEqual(TEXT("the host-selectable preset is rejected and Default is used"), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::Default));
		if (TestTrue(TEXT("the rejected rung is recorded first"), Resolved.Report.Rungs.Num() > 0))
		{
			TestEqual(TEXT("rung 0 outcome is Rejected"), static_cast<int32>(Resolved.Report.Rungs[0].Outcome), static_cast<int32>(ETuningRungOutcome::Rejected));
		}
		if (TestTrue(TEXT("a frozen set exists"), Resolved.Validated.IsValid()))
		{
			TestTrue(TEXT("nothing of the rejected preset is applied (L is not 1)"), Resolved.Validated->Values().Penalty_Public.NpcKillStrikeLimit != StrikeLimitOne);
		}
	}

	// I-NKP-1, Default: rejected as a whole, the ladder falls to the schema defaults.
	{
		FLocalPresetSource Source;
		Source.DefaultValues.Penalty_Public.NpcKillStrikeLimit = StrikeLimitOne;
		Source.bDefaultFlag = true;
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = ResolveWith(Source, FTuningPipelineCore::DefaultPresetId(), Capture);
		TestTrue(TEXT("L = 1 in Default records I-NKP-1 as a Reject"), Resolved.Report.HasFinding("I-NKP-1", ETuningCheck::Reject));
		TestEqual(TEXT("Default is rejected and the schema defaults are used"), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::SchemaDefaults));
	}

	// W-NKP-2: D 25 with RespawnStarvationSec 10 (45 > 40) warns; the boundary D 20 (40) does not.
	{
		FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V) { V.Penalty_Public.InformantRespawnDelaySec = DowntimeRespawnDelaySec; });
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = ResolveWith(Source, TestPresetId, Capture);
		TestTrue(TEXT("D 25 logs the downtime warning W-NKP-2"), RequestedHas(Resolved, "W-NKP-2", ETuningCheck::Warn));
		TestEqual(TEXT("one warning names W-NKP-2"), WarningsNaming(Capture, "W-NKP-2"), 1);

		FLocalPresetSource BoundarySource = MakeSource([](FStealthServerTuning& V) { V.Penalty_Public.InformantRespawnDelaySec = DowntimeBoundaryRespawnDelaySec; });
		FStealthLogCapture BoundaryCapture;
		const FTuningResolveResult Boundary = ResolveWith(BoundarySource, TestPresetId, BoundaryCapture);
		TestFalse(TEXT("D 20 (20 + 2 x 10 = 40) is within the budget: no W-NKP-2"), Boundary.Report.HasFinding("W-NKP-2", ETuningCheck::Warn));
	}

	// W-NKP-3: grace 1.5 with lockout 1.75 warns.
	{
		FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V)
		{
			V.Penalty_Server.StrikeOutReleaseGraceSec = ShortGraceSec;
			V.Assassination_Public.AttackAttemptLockoutSec = LockoutSec;
		});
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = ResolveWith(Source, TestPresetId, Capture);
		TestTrue(TEXT("grace 1.5 with lockout 1.75 logs the grace warning W-NKP-3"), RequestedHas(Resolved, "W-NKP-3", ETuningCheck::Warn));
		TestEqual(TEXT("one warning names W-NKP-3"), WarningsNaming(Capture, "W-NKP-3"), 1);
	}

	// W-NKP-4: the hard-maximum floor (4.7 <= 5) loads without the warning; the predicate with D = 4.0 returns the warning.
	{
		FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V)
		{
			V.Penalty_Public.InformantRespawnDelaySec = FloorRespawnDelaySec;
			V.Assassination_Public.DeathFallSec = HardMaxDeathFallSec;
		});
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = ResolveWith(Source, TestPresetId, Capture);
		TestEqual(TEXT("the hard-maximum floor preset is accepted at the requested rung"), static_cast<int32>(Resolved.FallbackLevel), static_cast<int32>(ETuningFallback::Requested));
		TestFalse(TEXT("D 5, DeathFallSec 2.0: no W-NKP-4 through the pipeline"), Resolved.Report.HasFinding("W-NKP-4", ETuningCheck::Warn));

		FPenaltyInvariantInputs Inputs = FPenaltyInvariantInputs::FromValues(Source.RequestedValues);
		Inputs.NoPawnFadeSec = HardMaxNoPawnFadeSec;
		const FPenaltyInvariantReport Report = StealthPenaltyInvariants::Evaluate(Source.RequestedValues, Inputs, FTuningCheckContext());
		TestFalse(TEXT("D 5, DeathFallSec 2.0, NoPawnFadeSec 1.5 (floor 4.7): no W-NKP-4"), Report.Warned.Contains(TEXT("W-NKP-4")));

		TestEqual(TEXT("the predicate at the hard maxima passes (4.7 <= 5)"),
			static_cast<int32>(StealthPenaltyInvariants::FloorGuard(HardMaxDeathFallSec, HardMaxNoPawnFadeSec, FloorRespawnDelaySec)), static_cast<int32>(ETuningCheck::Pass));
		TestEqual(TEXT("the predicate with the injected D = 4.0 returns the warning"),
			static_cast<int32>(StealthPenaltyInvariants::FloorGuard(HardMaxDeathFallSec, HardMaxNoPawnFadeSec, InjectedOutOfRangeDelaySec)), static_cast<int32>(ETuningCheck::Warn));
	}

	// W-NKP-5: card timeout 1.0 with beat 0.8 warns (1.85 needed); the defaults (2.0) do not.
	{
		FLocalPresetSource Source = MakeSource([](FStealthServerTuning& V)
		{
			V.Penalty_Public.ExileCardTimeoutSec = ShortCardTimeoutSec;
			V.Penalty_Public.ExileBeatSec = DefaultBeatSec;
		});
		FStealthLogCapture Capture;
		const FTuningResolveResult Resolved = ResolveWith(Source, TestPresetId, Capture);
		TestTrue(TEXT("card timeout 1.0 with beat 0.8 logs the card-timeout warning W-NKP-5"), RequestedHas(Resolved, "W-NKP-5", ETuningCheck::Warn));
		TestEqual(TEXT("one warning names W-NKP-5"), WarningsNaming(Capture, "W-NKP-5"), 1);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
