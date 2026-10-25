// Story tdc-046 (production/epics/tuning-data-config/story-046-f1-validate-clamp-nan.md), GDD AC-4 (CR-6,
// F-1): the F-1 stage in isolation. A non-finite float takes the schema default (tested before clamping),
// a declared sentinel is kept, everything else is clamped to the hard range, a value exactly on a bound
// or equal to a sentinel gives no warning, and each changed key logs exactly one LogStealthFoundation
// warning naming its authored and effective value. The whole-preset outcome of this fixture (I-2 would
// reject NpcCount 6 with TeamSize 5) is AC-6a / AC-17, not this test: F-1 evaluates no invariant.
// The five AC keys use the real generated schema rows; EndedAutoResetSec is a fixture row (its owner key
// is not in the schema yet), exercised through the value-level ApplyF1/LogF1Change.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/Testing/StealthLogCapture.h"

#include <limits>

namespace ValidateClampAndNanTestData
{
	const FTuningSchemaRow* FindRow(const ANSICHAR* Key)
	{
		for (const FTuningSchemaRow& Row : GetTuningSchema())
		{
			if (FCStringAnsi::Strcmp(Row.Key, Key) == 0)
			{
				return &Row;
			}
		}
		return nullptr;
	}

	/** Fixture row: EndedAutoResetSec (Round), sentinel 0, continuous range 3 to 600 (registry, GDD F-1 example). */
	FTuningSchemaRow EndedAutoResetRow()
	{
		FTuningSchemaRow Row;
		Row.Key = "EndedAutoResetSec";
		Row.TypeTag = StealthTuningTypeTag::Float;
		Row.Default = 0.0;
		Row.HardMin = 3.0;
		Row.HardMax = 600.0;
		Row.SafeMin = 20.0;
		Row.SafeMax = 120.0;
		Row.bPublic = false;
		Row.Latch = ETuningLatch::Use;
		Row.Owner = "Round";
		Row.Unit = "Sec";
		Row.bHasSentinel = true;
		Row.Sentinel = 0.0;
		return Row;
	}

	constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
	constexpr float PlusInf = std::numeric_limits<float>::infinity();
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FTuningValidateClampAndNanTest, FStealthLogCaptureTestBase,
	"StealthGame.Tuning.ValidateClampAndNan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningValidateClampAndNanTest::RunTest(const FString& Parameters)
{
	using namespace ValidateClampAndNanTestData;

	const TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();
	if (!TestTrue(TEXT("the generated schema has rows (not a bootstrap build)"), Schema.Num() > 0))
	{
		return false;
	}
	const FTuningF1Options NoFlag;

	// The AC fixture: NaN, above max, below min (float and int).
	{
		FStealthServerTuning Values;
		Values.Objectives_Public.ObjectiveInteractSec = NaN;
		Values.Assassination_Public.DefenceDurationSec = 7.5f;
		Values.Assassination_Public.AssassinationRangeCm = 50.0f;
		Values.Penalty_Public.NpcKillStrikeLimit = 0;
		Values.Crowd_Server.NpcCount = 0;

		FStealthLogCapture Capture;
		FTuningF1Report Report;
		FTuningPipelineCore::RunF1(Schema, Values, NoFlag, Report);
		Capture.Stop();

		TestEqual(TEXT("ObjectiveInteractSec NaN gives the default 3.0"), Values.Objectives_Public.ObjectiveInteractSec, 3.0f);
		TestEqual(TEXT("DefenceDurationSec 7.5 gives 5.0"), Values.Assassination_Public.DefenceDurationSec, 5.0f);
		TestEqual(TEXT("AssassinationRangeCm 50 gives 100"), Values.Assassination_Public.AssassinationRangeCm, 100.0f);
		TestEqual(TEXT("NpcKillStrikeLimit 0 gives 2"), Values.Penalty_Public.NpcKillStrikeLimit, 2);
		TestEqual(TEXT("NpcCount 0 gives 6"), Values.Crowd_Server.NpcCount, 6);
		TestEqual(TEXT("five keys changed"), Report.Changes.Num(), 5);

		const TCHAR* const Expected[][3] = {
			{ TEXT("ObjectiveInteractSec"), TEXT("NaN"), TEXT("3") },
			{ TEXT("DefenceDurationSec"), TEXT("7.5"), TEXT("5") },
			{ TEXT("AssassinationRangeCm"), TEXT("50"), TEXT("100") },
			{ TEXT("NpcKillStrikeLimit"), TEXT("0"), TEXT("2") },
			{ TEXT("NpcCount"), TEXT("0"), TEXT("6") },
		};
		for (const auto& Line : Expected)
		{
			const FStealthLogMatchResult Match = Capture.ExpectExactlyOneWarning(Line[0], Line[1], Line[2]);
			TestTrue(Match.Describe(), Match.Passed());
		}
		TestEqual(TEXT("exactly one warning per changed key, no other warning"), Capture.GetWarnings().Num(), 5);
		for (const FTuningF1Change& Change : Report.Changes)
		{
			const bool bNonFinite = FCStringAnsi::Strcmp(Change.Key, "ObjectiveInteractSec") == 0;
			TestEqual(FString::Printf(TEXT("%s action"), *FString(Change.Key)), static_cast<int32>(Change.Action),
				static_cast<int32>(bNonFinite ? ETuningF1Action::DefaultedNonFinite : ETuningF1Action::Clamped));
		}
	}

	// +Infinity and -Infinity each take the default (not a bound).
	for (const float Infinity : { PlusInf, -PlusInf })
	{
		FStealthServerTuning Values;
		Values.Objectives_Public.ObjectiveInteractSec = Infinity;
		FStealthLogCapture Capture;
		FTuningF1Report Report;
		FTuningPipelineCore::RunF1(Schema, Values, NoFlag, Report);
		Capture.Stop();
		const TCHAR* const Authored = Infinity > 0.0f ? TEXT("+Infinity") : TEXT("-Infinity");
		TestEqual(FString::Printf(TEXT("ObjectiveInteractSec %s gives 3.0"), Authored), Values.Objectives_Public.ObjectiveInteractSec, 3.0f);
		const FStealthLogMatchResult Match = Capture.ExpectExactlyOneWarning(TEXT("ObjectiveInteractSec"), Authored, TEXT("3"));
		TestTrue(Match.Describe(), Match.Passed());
		TestEqual(FString::Printf(TEXT("%s: one warning"), Authored), Capture.GetWarnings().Num(), 1);
	}

	// NpcKillStrikeLimit 9 gives 5.
	{
		FStealthServerTuning Values;
		Values.Penalty_Public.NpcKillStrikeLimit = 9;
		FStealthLogCapture Capture;
		FTuningF1Report Report;
		FTuningPipelineCore::RunF1(Schema, Values, NoFlag, Report);
		Capture.Stop();
		TestEqual(TEXT("NpcKillStrikeLimit 9 gives 5"), Values.Penalty_Public.NpcKillStrikeLimit, 5);
		const FStealthLogMatchResult Match = Capture.ExpectExactlyOneWarning(TEXT("NpcKillStrikeLimit"), TEXT("9"), TEXT("5"));
		TestTrue(Match.Describe(), Match.Passed());
	}

	// Exactly on a bound: no change, no warning (every row, both bounds).
	for (const bool bMax : { false, true })
	{
		FStealthServerTuning Values;
		for (const FTuningSchemaRow& Row : Schema)
		{
			Row.Set(Values, bMax ? Row.HardMax : Row.HardMin);
		}
		const FStealthServerTuning Before = Values;
		FStealthLogCapture Capture;
		FTuningF1Report Report;
		FTuningPipelineCore::RunF1(Schema, Values, NoFlag, Report);
		Capture.Stop();
		TestEqual(FString::Printf(TEXT("values on the hard %s: no change"), bMax ? TEXT("maximum") : TEXT("minimum")), Report.Changes.Num(), 0);
		TestEqual(FString::Printf(TEXT("values on the hard %s: no warning"), bMax ? TEXT("maximum") : TEXT("minimum")), Capture.GetWarnings().Num(), 0);
		for (const FTuningSchemaRow& Row : Schema)
		{
			TestEqual(FString::Printf(TEXT("%s unchanged on its bound"), *FString(Row.Key)), Row.Get(Values), Row.Get(Before));
		}
	}

	// Sentinel rule (fixture row): EndedAutoResetSec 0 stays 0 with no warning; 1 gives 3 with one warning.
	{
		const FTuningSchemaRow Row = EndedAutoResetRow();
		FStealthLogCapture Capture;
		ETuningF1Action Action = ETuningF1Action::Kept;
		const double Zero = FTuningPipelineCore::ApplyF1(Row, 0.0, NoFlag, Action);
		TestEqual(TEXT("EndedAutoResetSec 0 (sentinel) stays 0"), Zero, 0.0);
		TestEqual(TEXT("EndedAutoResetSec 0 is kept"), static_cast<int32>(Action), static_cast<int32>(ETuningF1Action::Kept));
		const double One = FTuningPipelineCore::ApplyF1(Row, 1.0, NoFlag, Action);
		TestEqual(TEXT("EndedAutoResetSec 1 gives 3"), One, 3.0);
		TestEqual(TEXT("EndedAutoResetSec 1 is clamped"), static_cast<int32>(Action), static_cast<int32>(ETuningF1Action::Clamped));
		FTuningPipelineCore::LogF1Change(Row, { Row.Key, 1.0, One, Action });
		Capture.Stop();
		const FStealthLogMatchResult Match = Capture.ExpectExactlyOneWarning(TEXT("EndedAutoResetSec"), TEXT("1"), TEXT("3"));
		TestTrue(Match.Describe(), Match.Passed());
		TestEqual(TEXT("sentinel: only the clamped value warned"), Capture.GetWarnings().Num(), 1);
	}

	// bTestAllowsStrikeLimitOne: NpcKillStrikeLimit 0 gives 1 only for a flagged preset; 1 is then kept.
	{
		const FTuningSchemaRow* StrikeRow = FindRow("NpcKillStrikeLimit");
		if (TestNotNull(TEXT("NpcKillStrikeLimit row exists"), StrikeRow))
		{
			FTuningF1Options Flagged;
			Flagged.bTestAllowsStrikeLimitOne = true;
			ETuningF1Action Action = ETuningF1Action::Kept;
			TestEqual(TEXT("NpcKillStrikeLimit 0 gives 1 with the flag"), FTuningPipelineCore::ApplyF1(*StrikeRow, 0.0, Flagged, Action), 1.0);
			TestEqual(TEXT("NpcKillStrikeLimit 0 gives 2 without the flag"), FTuningPipelineCore::ApplyF1(*StrikeRow, 0.0, NoFlag, Action), 2.0);
			TestEqual(TEXT("NpcKillStrikeLimit 1 is kept with the flag"), FTuningPipelineCore::ApplyF1(*StrikeRow, 1.0, Flagged, Action), 1.0);
			TestEqual(TEXT("NpcKillStrikeLimit 1 is kept with the flag (no change)"), static_cast<int32>(Action), static_cast<int32>(ETuningF1Action::Kept));
		}
		const FTuningSchemaRow* CountRow = FindRow("NpcCount");
		if (TestNotNull(TEXT("NpcCount row exists"), CountRow))
		{
			FTuningF1Options Flagged;
			Flagged.bTestAllowsStrikeLimitOne = true;
			ETuningF1Action Action = ETuningF1Action::Kept;
			TestEqual(TEXT("the flag lowers no other key's minimum (NpcCount 0 gives 6)"), FTuningPipelineCore::ApplyF1(*CountRow, 0.0, Flagged, Action), 6.0);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
