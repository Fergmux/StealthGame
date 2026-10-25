// Story tdc-055 (production/epics/tuning-data-config/story-055-owner-invariant-mechanism.md), GDD AC-30 (the
// mechanism half; the full-schema closure over every real owner row is story tdc-096): over a synthetic
// invariant table injected through FTuningResolveInput::Invariants (the seam beside story tdc-050's Schema),
// with the generated schema as the value fixture,
//   - a reject-class row that alone is broken rejects the preset, and the report names exactly that Id;
//   - a warn-class row accepts the preset with exactly one warning naming the Id;
//   - a mixed row (reject clause and warn clause, the I-OC2 / I-HUD-4 pattern) reports the worst result and
//     names the clause that produced it;
//   - context rows behave per scope: reject only when bIsPublic else warn (the I-3 / I-4 / I-NKP-1 pattern),
//     and not applicable when PresetId == Solo (the W-RMF-4 pattern);
//   - an offline row is never evaluated at preset load (its check counts calls; the count stays 0), and the
//     real table GetTuningInvariants() holds no offline row.
// No real owner row is added here: each owner epic adds its rows with fixtures (CR-15.6).
// Every synthetic preset keeps its keys inside their safe ranges so the only warnings are the rows' own.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningDerived.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthValidatedTuning.h"
#include "Server/Testing/StealthLogCapture.h"
#include "Tests/Tuning/StealthTuningFakePresetSource.h"

#include <type_traits>

// CR-6 purity, at compile level: a Check receives only const references and has no output parameter.
static_assert(std::is_same_v<decltype(FTuningInvariant::Check),
	ETuningCheck (*)(const FStealthServerTuning&, const FTuningDerived&, const FTuningCheckContext&)>, "invariant checks take const input only");

namespace OwnerInvariantMechanismTestData
{
	int32 OfflineCalls = 0;

	/** Reject-class: TargetRoundMinutes above 14 (safe 8-15) rejects. */
	ETuningCheck RejectRow(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		return V.Round_Server.TargetRoundMinutes > 14.0f ? ETuningCheck::Reject : ETuningCheck::Pass;
	}

	/** Warn-class: ObjectiveInteractSec above 4 (safe 2.9-4.5) warns. */
	ETuningCheck WarnRow(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		return V.Objectives_Public.ObjectiveInteractSec > 4.0f ? ETuningCheck::Warn : ETuningCheck::Pass;
	}

	/** Mixed (I-OC2 pattern): DefenceCooldownSec below 50 rejects, below 55 warns (safe 45-75). */
	ETuningCheck MixedRow(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext&)
	{
		const float Cooldown = V.Assassination_Public.DefenceCooldownSec;
		return Cooldown < 50.0f ? ETuningCheck::Reject : Cooldown < 55.0f ? ETuningCheck::Warn : ETuningCheck::Pass;
	}

	/** Public-scoped (I-3 / I-4 / I-NKP-1 pattern): AssassinationRangeCm above 240 (safe 150-250). */
	ETuningCheck PublicScopedRow(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext& Context)
	{
		if (V.Assassination_Public.AssassinationRangeCm <= 240.0f)
		{
			return ETuningCheck::Pass;
		}
		return Context.bIsPublic ? ETuningCheck::Reject : ETuningCheck::Warn;
	}

	/** Solo-scoped (W-RMF-4 pattern): NpcKillStrikeLimit 4 (safe 2-4) warns outside the Solo preset. */
	ETuningCheck SoloScopedRow(const FStealthServerTuning& V, const FTuningDerived&, const FTuningCheckContext& Context)
	{
		if (Context.PresetId == TuningSoloPresetId())
		{
			return ETuningCheck::Pass;
		}
		return V.Penalty_Public.NpcKillStrikeLimit >= 4 ? ETuningCheck::Warn : ETuningCheck::Pass;
	}

	/** Offline row: always rejects, and counts every evaluation (must stay 0 at preset load). */
	ETuningCheck OfflineRow(const FStealthServerTuning&, const FTuningDerived&, const FTuningCheckContext&)
	{
		++OfflineCalls;
		return ETuningCheck::Reject;
	}

	const FTuningInvariant FixtureRows[] = {
		{ "X-REJ-1", "Fixture", &RejectRow },
		{ "X-WARN-1", "Fixture", &WarnRow },
		{ "X-MIX-1", "Fixture", &MixedRow, "DefenceCooldownSec < 50", "DefenceCooldownSec < 55" },
		{ "X-PUB-1", "Fixture", &PublicScopedRow },
		{ "X-SOLO-1", "Fixture", &SoloScopedRow },
		{ "O-X-1", "Fixture", &OfflineRow, nullptr, nullptr, true },
	};

	const ANSICHAR* const AllIds[] = { "X-REJ-1", "X-WARN-1", "X-MIX-1", "X-PUB-1", "X-SOLO-1", "O-X-1" };

	/** Resolves PresetId (values from Author on top of the C++ defaults) against the fixture table; Default is clean. */
	FTuningResolveResult ResolveFixture(FName PresetId, bool bIsPublic, TFunctionRef<void(FStealthServerTuning&)> Author, FStealthLogCapture& Capture)
	{
		FStealthServerTuning Values;
		Author(Values);
		FStealthTuningFakePresetSource Source;
		Source.Add(PresetId, Values, bIsPublic);
		Source.Add(FTuningPipelineCore::DefaultPresetId(), FStealthServerTuning(), true);
		FTuningResolveInput Input;
		Input.RequestedPreset = PresetId;
		Input.Source = &Source;
		Input.Invariants = FixtureRows;
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

	/** The findings of the requested rung (rung 0). */
	const TArray<FTuningInvariantFinding>* RequestedFindings(const FTuningResolveResult& Result)
	{
		return Result.Report.Rungs.Num() > 0 ? &Result.Report.Rungs[0].Findings : nullptr;
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FTuningOwnerInvariantMechanismTest, FStealthLogCaptureTestBase,
	"StealthGame.Tuning.OwnerInvariantMechanism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningOwnerInvariantMechanismTest::RunTest(const FString& Parameters)
{
	using namespace OwnerInvariantMechanismTestData;

	if (!TestTrue(TEXT("the generated schema has rows (not a bootstrap build)"), GetTuningSchema().Num() > 0))
	{
		return false;
	}
	OfflineCalls = 0;
	const FName Playtest(TEXT("Playtest"));
	const FName TestPreset(TEXT("TestPreset"));

	// Baseline: the C++ defaults pass every fixture row with no warning.
	{
		FStealthLogCapture Capture;
		const FTuningResolveResult Result = ResolveFixture(TestPreset, false, [](FStealthServerTuning&) {}, Capture);
		TestTrue(TEXT("baseline: accepted at FallbackLevel 0"), !Result.IsFailed() && Result.FallbackLevel == ETuningFallback::Requested);
		TestEqual(TEXT("baseline: no warning"), Capture.GetWarnings().Num(), 0);
	}

	// A broken reject-class row alone rejects the preset; the report names exactly that Id.
	{
		FStealthLogCapture Capture;
		const FTuningResolveResult Result = ResolveFixture(TestPreset, false, [](FStealthServerTuning& V) { V.Round_Server.TargetRoundMinutes = 15.0f; }, Capture);
		TestTrue(TEXT("reject row: the preset is rejected (Default active, FallbackLevel 1)"), !Result.IsFailed() && Result.FallbackLevel == ETuningFallback::Default);
		const TArray<FTuningInvariantFinding>* Findings = RequestedFindings(Result);
		if (TestNotNull(TEXT("reject row: rung 0 recorded"), Findings) && TestEqual(TEXT("reject row: exactly one finding"), Findings->Num(), 1))
		{
			TestEqual(TEXT("reject row: the finding names X-REJ-1"), FString((*Findings)[0].Id), FString(TEXT("X-REJ-1")));
			TestEqual(TEXT("reject row: the finding is a Reject"), static_cast<int32>((*Findings)[0].Result), static_cast<int32>(ETuningCheck::Reject));
		}
		for (const ANSICHAR* Id : AllIds)
		{
			const bool bExpected = FCStringAnsi::Strcmp(Id, "X-REJ-1") == 0;
			TestEqual(FString::Printf(TEXT("reject row: the report names %s %s"), *FString(Id), bExpected ? TEXT("once") : TEXT("never")),
				Result.Report.HasFinding(Id, ETuningCheck::Reject), bExpected);
		}
	}

	// A warn-class row accepts the preset with exactly one warning naming the Id.
	{
		FStealthLogCapture Capture;
		const FTuningResolveResult Result = ResolveFixture(TestPreset, false, [](FStealthServerTuning& V) { V.Objectives_Public.ObjectiveInteractSec = 4.4f; }, Capture);
		TestTrue(TEXT("warn row: accepted at FallbackLevel 0"), !Result.IsFailed() && Result.FallbackLevel == ETuningFallback::Requested);
		TestEqual(TEXT("warn row: exactly one warning names X-WARN-1"), WarningsNaming(Capture, "X-WARN-1"), 1);
		TestEqual(TEXT("warn row: no other warning"), Capture.GetWarnings().Num(), 1);
		TestTrue(TEXT("warn row: the report records the Warn"), Result.Report.HasFinding("X-WARN-1", ETuningCheck::Warn));
	}

	// Mixed row: the worst result wins and the report names the clause that produced it.
	{
		FStealthLogCapture Capture;
		const FTuningResolveResult Result = ResolveFixture(TestPreset, false, [](FStealthServerTuning& V) { V.Assassination_Public.DefenceCooldownSec = 52.0f; }, Capture);
		TestTrue(TEXT("mixed row (52): accepted with a warning"), !Result.IsFailed() && Result.FallbackLevel == ETuningFallback::Requested);
		const TArray<FTuningInvariantFinding>* Findings = RequestedFindings(Result);
		if (TestNotNull(TEXT("mixed row (52): rung 0 recorded"), Findings) && TestEqual(TEXT("mixed row (52): one finding"), Findings->Num(), 1))
		{
			TestEqual(TEXT("mixed row (52): Warn"), static_cast<int32>((*Findings)[0].Result), static_cast<int32>(ETuningCheck::Warn));
			TestEqual(TEXT("mixed row (52): names the warn clause"), FString((*Findings)[0].Clause), FString(TEXT("DefenceCooldownSec < 55")));
		}
		TestEqual(TEXT("mixed row (52): one warning names X-MIX-1"), WarningsNaming(Capture, "X-MIX-1"), 1);
	}
	{
		FStealthLogCapture Capture;
		const FTuningResolveResult Result = ResolveFixture(TestPreset, false, [](FStealthServerTuning& V) { V.Assassination_Public.DefenceCooldownSec = 48.0f; }, Capture);
		TestTrue(TEXT("mixed row (48): rejected"), !Result.IsFailed() && Result.FallbackLevel == ETuningFallback::Default);
		const TArray<FTuningInvariantFinding>* Findings = RequestedFindings(Result);
		if (TestNotNull(TEXT("mixed row (48): rung 0 recorded"), Findings) && TestEqual(TEXT("mixed row (48): one finding (the worst)"), Findings->Num(), 1))
		{
			TestEqual(TEXT("mixed row (48): Reject"), static_cast<int32>((*Findings)[0].Result), static_cast<int32>(ETuningCheck::Reject));
			TestEqual(TEXT("mixed row (48): names the reject clause"), FString((*Findings)[0].Clause), FString(TEXT("DefenceCooldownSec < 50")));
		}
	}

	// Context row, bIsPublic scope: rejects in a public (HostSelectable) preset, warns in a non-selectable one.
	{
		FStealthLogCapture PublicCapture;
		const FTuningResolveResult Public = ResolveFixture(Playtest, true, [](FStealthServerTuning& V) { V.Assassination_Public.AssassinationRangeCm = 245.0f; }, PublicCapture);
		TestTrue(TEXT("public scope: rejected"), !Public.IsFailed() && Public.FallbackLevel == ETuningFallback::Default);
		TestTrue(TEXT("public scope: the report names X-PUB-1 as Reject"), Public.Report.HasFinding("X-PUB-1", ETuningCheck::Reject));

		FStealthLogCapture PrivateCapture;
		const FTuningResolveResult Private = ResolveFixture(TestPreset, false, [](FStealthServerTuning& V) { V.Assassination_Public.AssassinationRangeCm = 245.0f; }, PrivateCapture);
		TestTrue(TEXT("non-selectable scope: accepted"), !Private.IsFailed() && Private.FallbackLevel == ETuningFallback::Requested);
		TestTrue(TEXT("non-selectable scope: the report names X-PUB-1 as Warn"), Private.Report.HasFinding("X-PUB-1", ETuningCheck::Warn));
		TestEqual(TEXT("non-selectable scope: one warning names X-PUB-1"), WarningsNaming(PrivateCapture, "X-PUB-1"), 1);
	}

	// Context row, Solo scope: not applicable in Solo, warns elsewhere.
	{
		FStealthLogCapture SoloCapture;
		const FTuningResolveResult Solo = ResolveFixture(TuningSoloPresetId(), false, [](FStealthServerTuning& V) { V.Penalty_Public.NpcKillStrikeLimit = 4; }, SoloCapture);
		TestTrue(TEXT("Solo: accepted"), !Solo.IsFailed() && Solo.FallbackLevel == ETuningFallback::Requested);
		TestEqual(TEXT("Solo: X-SOLO-1 not applicable (no warning)"), WarningsNaming(SoloCapture, "X-SOLO-1"), 0);
		TestFalse(TEXT("Solo: no X-SOLO-1 finding"), Solo.Report.HasFinding("X-SOLO-1", ETuningCheck::Warn));

		FStealthLogCapture OtherCapture;
		const FTuningResolveResult Other = ResolveFixture(TestPreset, false, [](FStealthServerTuning& V) { V.Penalty_Public.NpcKillStrikeLimit = 4; }, OtherCapture);
		TestTrue(TEXT("outside Solo: accepted with a warning"), !Other.IsFailed() && Other.Report.HasFinding("X-SOLO-1", ETuningCheck::Warn));
		TestEqual(TEXT("outside Solo: one warning names X-SOLO-1"), WarningsNaming(OtherCapture, "X-SOLO-1"), 1);
	}

	// Offline rows: never evaluated at preset load; the real table holds none.
	TestEqual(TEXT("the offline row was never evaluated at preset load"), OfflineCalls, 0);
	for (const FTuningInvariant& Row : GetTuningInvariants())
	{
		TestFalse(FString::Printf(TEXT("the preset-load table holds no offline row (%s)"), *FString(Row.Id)), Row.bOffline);
	}
	for (const FTuningInvariant& Row : GetTuningOfflineInvariants())
	{
		TestTrue(FString::Printf(TEXT("the offline list holds only offline rows (%s)"), *FString(Row.Id)), Row.bOffline);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
