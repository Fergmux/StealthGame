// Story tdc-047 (production/epics/tuning-data-config/story-047-derived-indicators.md), GDD AC-18 indicator
// clauses: the F-3 indicators at the schema defaults (the C++ initialisers, i.e. the generated table's
// defaults) and at NpcCount 15 / NpcKillStrikeLimit 3 match the GDD F-3 examples, and no preset field named
// BlindGuessWarnPct exists (it is the schema constant StealthTuningConstants::BlindGuessWarnPct, so no preset
// can loosen its own I-4 gate). The expected numbers are literals: the exact GDD value is the point
// (boundary-value exception). The preset-scope accept/reject clauses of AC-18 are story tdc-098.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningConstants.h"
#include "Server/StealthTuningData.h"
#include "Server/StealthTuningDerived.h"
#include "UObject/UnrealType.h"
#include "View/StealthClientTuning.h"

namespace DerivedIndicatorsTestData
{
	/** Every property name under Struct, recursing into struct members. */
	void CollectPropertyNames(const UStruct* Struct, TArray<FString>& OutNames)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			OutNames.Add(It->GetName());
			if (const FStructProperty* Inner = CastField<FStructProperty>(*It))
			{
				CollectPropertyNames(Inner->Struct, OutNames);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTuningDerivedIndicatorsTest, "StealthGame.Tuning.DerivedIndicators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningDerivedIndicatorsTest::RunTest(const FString& Parameters)
{
	using namespace DerivedIndicatorsTestData;

	// At the schema defaults (TeamSize 5, NpcCount 50, L 3, duration 4, cooldown 60).
	{
		FStealthServerTuning Defaults;
		const FTuningDerived Derived = FTuningDerived::Compute(Defaults);
		TestEqual(TEXT("CrowdRatio at defaults is 5.0"), Derived.CrowdRatio, 5.0, 1e-9);
		TestEqual(TEXT("DutyCycle at defaults is 0.0667"), Derived.DutyCycle, 0.0667, 0.0001);
		TestEqual(TEXT("BlindGuessSuccess at defaults is 0.249"), Derived.BlindGuessSuccess, 0.249, 0.001);
		TestEqual(TEXT("ExpectedBlindKills at defaults is 0.300"), Derived.ExpectedBlindKills, 0.300, 0.001);
		TestEqual(TEXT("MissToHit at defaults is 10"), Derived.MissToHit, 10.0, 1e-9);
	}

	// NpcCount 15 with NpcKillStrikeLimit 3 (the GDD's rejected-public-preset example).
	{
		FStealthServerTuning Values;
		Values.Crowd_Server.NpcCount = 15;
		Values.Penalty_Public.NpcKillStrikeLimit = 3;
		const FTuningDerived Derived = FTuningDerived::Compute(Values);
		TestEqual(TEXT("p at NpcCount 15 is 0.25"), Derived.P, 0.25, 1e-9);
		TestEqual(TEXT("BlindGuessSuccess at NpcCount 15, L 3 is 0.578"), Derived.BlindGuessSuccess, 0.578, 0.001);
		TestEqual(TEXT("ExpectedBlindKills at NpcCount 15, L 3 is 1.0"), Derived.ExpectedBlindKills, 1.0, 0.001);
	}

	// Unclamped input never divides by zero.
	{
		FStealthServerTuning Zero;
		Zero.Round_Public.TeamSize = 0;
		Zero.Crowd_Server.NpcCount = 0;
		Zero.Assassination_Public.DefenceCooldownSec = 0.0f;
		const FTuningDerived Derived = FTuningDerived::Compute(Zero);
		TestTrue(TEXT("zero denominators give finite indicators"), FMath::IsFinite(Derived.CrowdRatio) && FMath::IsFinite(Derived.DutyCycle)
			&& FMath::IsFinite(Derived.P) && FMath::IsFinite(Derived.BlindGuessSuccess) && FMath::IsFinite(Derived.ExpectedBlindKills) && FMath::IsFinite(Derived.MissToHit));
	}

	// No preset field (or face field) named BlindGuessWarnPct: it is a schema constant.
	{
		TArray<FString> Names;
		CollectPropertyNames(UStealthTuningData::StaticClass(), Names);
		CollectPropertyNames(FStealthServerTuning::StaticStruct(), Names);
		CollectPropertyNames(FStealthClientTuning::StaticStruct(), Names);
		TestTrue(TEXT("the reflection walk found the tuning keys"), Names.Contains(TEXT("NpcKillStrikeLimit")));
		TestFalse(TEXT("no preset field named BlindGuessWarnPct"), Names.Contains(TEXT("BlindGuessWarnPct")));
		TestEqual(TEXT("BlindGuessWarnPct is the schema constant 50"), StealthTuningConstants::BlindGuessWarnPct, 50.0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
