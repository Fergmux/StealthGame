// Story tdc-048 (production/epics/tuning-data-config/story-048-invariants-i1-i5.md), GDD AC-17 (invariant-table
// half; the "whole preset at FallbackLevel 1" clause is story tdc-097): the I-1 to I-5 rows return Pass, Warn
// or Reject exactly as the AC-17 bullets state. Each case starts from the schema defaults, sets the bullet's
// keys, runs F-1 (story tdc-046) and F-3 (tdc-047) and evaluates only the rows I-1..I-5, selected by Id from
// GetTuningInvariants() (owner rows added later are ignored). Non-selectable presets use NpcKillStrikeLimit 1
// with bTestAllowsStrikeLimitOne so I-4 cannot interfere; the HostSelectable bullet uses limit 2, no flag.
// I-5 runs on a fixture schema view holding the pair FixtureMinSec/FixtureMaxSec (fixture rows whose
// accessors store into TargetRoundMinutes and AssassinationRangeCm, two keys no I-1..I-4 row reads).
// Purity: every case asserts the evaluated value set is unchanged. Negative controls: a row that writes a
// value and a row returning the wrong severity are each detected by the same case check.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningDerived.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/Testing/StealthLogCapture.h"
#include "Server/Testing/StealthNegativeControl.h"

namespace InvariantBoundariesTestData
{
	const ANSICHAR* const TdcIds[] = { "I-1", "I-2", "I-3", "I-4", "I-5" };

	/** The I-1..I-5 rows of the real table, in Id order. */
	TArray<FTuningInvariant> TdcRows()
	{
		TArray<FTuningInvariant> Rows;
		for (const ANSICHAR* Id : TdcIds)
		{
			for (const FTuningInvariant& Row : GetTuningInvariants())
			{
				if (FCStringAnsi::Strcmp(Row.Id, Id) == 0)
				{
					Rows.Add(Row);
				}
			}
		}
		return Rows;
	}

	double GetFixtureMin(const FStealthServerTuning& V) { return V.Round_Server.TargetRoundMinutes; }
	void SetFixtureMin(FStealthServerTuning& V, double X) { V.Round_Server.TargetRoundMinutes = static_cast<float>(X); }
	double GetFixtureMax(const FStealthServerTuning& V) { return V.Assassination_Public.AssassinationRangeCm; }
	void SetFixtureMax(FStealthServerTuning& V, double X) { V.Assassination_Public.AssassinationRangeCm = static_cast<float>(X); }

	/** The test schema fixture: the real rows plus the range pair FixtureMinSec/FixtureMaxSec (no clamp: range 0-1000). */
	TArray<FTuningSchemaRow> FixtureSchema()
	{
		TArray<FTuningSchemaRow> Rows(GetTuningSchema().GetData(), GetTuningSchema().Num());
		// The fixture pair reuses two real keys' storage, so those two real rows leave the fixture view.
		Rows.RemoveAll([](const FTuningSchemaRow& Row)
		{
			return FCStringAnsi::Strcmp(Row.Key, "TargetRoundMinutes") == 0 || FCStringAnsi::Strcmp(Row.Key, "AssassinationRangeCm") == 0;
		});
		for (const bool bMax : { false, true })
		{
			FTuningSchemaRow& Row = Rows.AddDefaulted_GetRef();
			Row.Key = bMax ? "FixtureMaxSec" : "FixtureMinSec";
			Row.TypeTag = StealthTuningTypeTag::Float;
			Row.Get = bMax ? &GetFixtureMax : &GetFixtureMin;
			Row.Set = bMax ? &SetFixtureMax : &SetFixtureMin;
			Row.Default = 10.0;
			Row.HardMin = 0.0;
			Row.HardMax = 1000.0;
			Row.SafeMin = 0.0;
			Row.SafeMax = 1000.0;
			Row.Latch = ETuningLatch::Round;
			Row.Owner = "Fixture";
			Row.Unit = "Sec";
		}
		return Rows;
	}

	/** One AC-17 case: the authored keys and the expected result of each row. */
	struct FCase
	{
		const TCHAR* Name;
		bool bHostSelectable = false;
		TFunction<void(FStealthServerTuning&)> Author;
		/** Expected I-1..I-5 results, in TdcIds order. */
		ETuningCheck Expected[5];
	};

	constexpr ETuningCheck P = ETuningCheck::Pass;
	constexpr ETuningCheck W = ETuningCheck::Warn;
	constexpr ETuningCheck R = ETuningCheck::Reject;

	/**
	 * Runs one case (defaults, author, F-1, F-3, rows) and checks each row's result and the purity of the
	 * evaluation into Ctx. Returns the derived indicators for further assertions.
	 */
	FTuningDerived RunCase(TConstArrayView<FTuningSchemaRow> Schema, TConstArrayView<FTuningInvariant> Rows, const FCase& Case, FStealthCheckContext& Ctx)
	{
		FStealthServerTuning Values = FTuningPipelineCore::BuildSchemaDefaults(Schema);
		if (!Case.bHostSelectable)
		{
			Values.Penalty_Public.NpcKillStrikeLimit = 1;
		}
		else
		{
			Values.Penalty_Public.NpcKillStrikeLimit = 2;
		}
		Case.Author(Values);
		FTuningF1Options Options;
		Options.bTestAllowsStrikeLimitOne = !Case.bHostSelectable;
		FTuningF1Report F1;
		FTuningPipelineCore::RunF1(Schema, Values, Options, F1);
		const FTuningDerived Derived = FTuningDerived::Compute(Values);

		FTuningCheckContext Context;
		Context.bIsPublic = Case.bHostSelectable;
		Context.PresetId = Case.bHostSelectable ? FName(TEXT("Playtest")) : FName(TEXT("TestPreset"));
		Context.Schema = Schema;

		for (int32 Index = 0; Index < Rows.Num() && Index < 5; ++Index)
		{
			const FStealthServerTuning Before = Values;
			const ETuningCheck Result = Rows[Index].Check(Values, Derived, Context);
			Ctx.TestEqual(FString::Printf(TEXT("%s: %s result"), Case.Name, *FString(Rows[Index].Id)), static_cast<int32>(Result), static_cast<int32>(Case.Expected[Index]));
			for (const FTuningSchemaRow& Row : Schema)
			{
				Ctx.TestTrue(FString::Printf(TEXT("%s: %s wrote no value (%s unchanged)"), Case.Name, *FString(Rows[Index].Id), *FString(Row.Key)),
					Row.Get(Values) == Row.Get(Before));
			}
		}
		return Derived;
	}

	TArray<FCase> Cases()
	{
		TArray<FCase> Out;
		Out.Add({ TEXT("DefenceDurationSec 5 cooldown 40"), false, [](FStealthServerTuning& V) { V.Assassination_Public.DefenceDurationSec = 5.0f; V.Assassination_Public.DefenceCooldownSec = 40.0f; }, { P, P, P, P, P } });
		Out.Add({ TEXT("DefenceDurationSec 5 cooldown 39.9"), false, [](FStealthServerTuning& V) { V.Assassination_Public.DefenceDurationSec = 5.0f; V.Assassination_Public.DefenceCooldownSec = 39.9f; }, { R, P, P, P, P } });
		Out.Add({ TEXT("TeamSize 5 NpcCount 10"), false, [](FStealthServerTuning& V) { V.Round_Public.TeamSize = 5; V.Crowd_Server.NpcCount = 10; }, { P, P, W, P, P } });
		Out.Add({ TEXT("TeamSize 5 NpcCount 9"), false, [](FStealthServerTuning& V) { V.Round_Public.TeamSize = 5; V.Crowd_Server.NpcCount = 9; }, { P, R, W, P, P } });
		Out.Add({ TEXT("TeamSize 1 NpcCount 10"), false, [](FStealthServerTuning& V) { V.Round_Public.TeamSize = 1; V.Crowd_Server.NpcCount = 10; }, { P, P, W, P, P } });
		Out.Add({ TEXT("TeamSize 4 NpcCount 8"), false, [](FStealthServerTuning& V) { V.Round_Public.TeamSize = 4; V.Crowd_Server.NpcCount = 8; }, { P, P, W, P, P } });
		Out.Add({ TEXT("TeamSize 4 NpcCount 7"), false, [](FStealthServerTuning& V) { V.Round_Public.TeamSize = 4; V.Crowd_Server.NpcCount = 7; }, { P, R, W, P, P } });
		Out.Add({ TEXT("HostSelectable TeamSize 1 NpcCount 21"), true, [](FStealthServerTuning& V) { V.Round_Public.TeamSize = 1; V.Crowd_Server.NpcCount = 21; }, { P, P, R, P, P } });
		Out.Add({ TEXT("HostSelectable TeamSize 1 NpcCount 22"), true, [](FStealthServerTuning& V) { V.Round_Public.TeamSize = 1; V.Crowd_Server.NpcCount = 22; }, { P, P, P, P, P } });
		Out.Add({ TEXT("range pair Min 20 > Max 10"), false, [](FStealthServerTuning& V) { SetFixtureMin(V, 20.0); SetFixtureMax(V, 10.0); }, { P, P, P, P, R } });
		Out.Add({ TEXT("range pair Min 10 == Max 10"), false, [](FStealthServerTuning& V) { SetFixtureMin(V, 10.0); SetFixtureMax(V, 10.0); }, { P, P, P, P, P } });
		Out.Add({ TEXT("authored DefenceDurationSec 7.5 cooldown 40"), false, [](FStealthServerTuning& V) { V.Assassination_Public.DefenceDurationSec = 7.5f; V.Assassination_Public.DefenceCooldownSec = 40.0f; }, { P, P, P, P, P } });
		return Out;
	}

	/** Negative-control row: writes a value through a const_cast (purity violation), returns Pass. */
	ETuningCheck WritingI1(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext&)
	{
		const_cast<FStealthServerTuning&>(Values).Assassination_Public.DefenceCooldownSec = 120.0f;
		return ETuningCheck::Pass;
	}

	/** Negative-control row: I-2 with the wrong severity (always Pass). */
	ETuningCheck AlwaysPassI2(const FStealthServerTuning&, const FTuningDerived&, const FTuningCheckContext&)
	{
		return ETuningCheck::Pass;
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FTuningInvariantBoundariesTest, FStealthLogCaptureTestBase,
	"StealthGame.Tuning.InvariantBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningInvariantBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace InvariantBoundariesTestData;

	if (!TestTrue(TEXT("the generated schema has rows (not a bootstrap build)"), GetTuningSchema().Num() > 0))
	{
		return false;
	}
	const TArray<FTuningInvariant> Rows = TdcRows();
	if (!TestEqual(TEXT("the table holds exactly one row for each of I-1..I-5"), Rows.Num(), 5))
	{
		return false;
	}
	for (const FTuningInvariant& Row : Rows)
	{
		TestFalse(FString::Printf(TEXT("%s is a preset-load row"), *FString(Row.Id)), Row.bOffline);
		TestEqual(FString::Printf(TEXT("%s is owned by TDC"), *FString(Row.Id)), FString(Row.Owner), FString(TEXT("TDC")));
	}
	const TArray<FTuningSchemaRow> Schema = FixtureSchema();
	TestEqual(TEXT("the fixture schema holds exactly one range pair"), FindTuningRangePairs(Schema).Num(), 1);
	TestEqual(TEXT("the real schema holds no range pair yet"), FindTuningRangePairs(GetTuningSchema()).Num(), 0);

	// Range-pair recognition: <Prefix>Min<Unit>/<Prefix>Max<Unit>; a one-sided bound or a lone Min is not a pair.
	{
		FTuningSchemaRow OneSided[3];
		OneSided[0].Key = "CrowdMaxIdleSec"; OneSided[0].Unit = "Sec";
		OneSided[1].Key = "FinalCandidatesMinCount"; OneSided[1].Unit = "Count";
		OneSided[2].Key = "MinSec"; OneSided[2].Unit = "Sec";
		TestEqual(TEXT("one-sided bounds and a prefix-less key are not range pairs"), FindTuningRangePairs(MakeArrayView(OneSided)).Num(), 0);
	}

	FStealthLogCapture Capture;
	for (const FCase& Case : Cases())
	{
		FStealthCheckContext Ctx;
		const FTuningDerived Derived = RunCase(Schema, Rows, Case, Ctx);
		for (const FString& Failure : Ctx.GetFailures())
		{
			AddError(Failure);
		}
		if (FCString::Strcmp(Case.Name, TEXT("HostSelectable TeamSize 1 NpcCount 22")) == 0)
		{
			TestTrue(FString::Printf(TEXT("I-4 BlindGuessSuccess is 0.085 (got %.4f)"), Derived.BlindGuessSuccess), FMath::IsNearlyEqual(Derived.BlindGuessSuccess, 0.085, 0.001));
		}
	}
	// The 7.5 case clamps through F-1 (story tdc-046): exactly one clamp warning names it.
	const FStealthLogMatchResult Clamp = Capture.ExpectExactlyOneWarning(TEXT("DefenceDurationSec"), TEXT("7.5"), TEXT("5"));
	TestTrue(Clamp.Describe(), Clamp.Passed());
	Capture.Stop();

	// Negative control: a row that writes a value is detected by the purity check.
	{
		TArray<FTuningInvariant> Writing = Rows;
		Writing[0].Check = &WritingI1;
		const FCase Case = Cases()[0];
		FStealthNegativeControl Control(TEXT("InvariantRowWritesAValue"));
		FStealthNegativeControl::Report(*this, Control.Run([&](FStealthCheckContext& Ctx) { RunCase(Schema, Writing, Case, Ctx); }));
	}
	// Negative control: a row returning the wrong severity (I-2 never rejects) is detected.
	{
		TArray<FTuningInvariant> Wrong = Rows;
		Wrong[1].Check = &AlwaysPassI2;
		const FCase Case = Cases()[3];
		FStealthNegativeControl Control(TEXT("InvariantRowWrongSeverity"));
		FStealthNegativeControl::Report(*this, Control.Run([&](FStealthCheckContext& Ctx) { RunCase(Schema, Wrong, Case, Ctx); }));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
