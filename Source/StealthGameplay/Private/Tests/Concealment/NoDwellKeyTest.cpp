// Story con-001 (production/epics/concealment-bushes/story-001-concealment-tuning-keys-and-timing-validator.md), AC3
// (GDD concealment-bushes.md AC-CON-12 whitelist part; ADR-0017 section 2, P4): no Concealment key name contains dwell,
// capacity, stay, reveal or timeout (case-insensitive). The check runs over the compiled schema rows of owner Concealment;
// planted names (BushDwellSec, bushStayMax) are flagged by the same function (negative controls).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthTuningSchema.h"
#include "Server/Testing/StealthConcealmentTuningKnobs.h"

namespace ConcealmentNoDwellKeyTestData
{
	/** The names in Keys that contain a forbidden word (case-insensitive). */
	TArray<FString> FindForbidden(const TArray<FString>& Keys)
	{
		TArray<FString> Out;
		for (const FString& Key : Keys)
		{
			for (const ANSICHAR* Word : StealthConcealmentTuningKnobs::ForbiddenWords)
			{
				if (Key.Contains(ANSI_TO_TCHAR(Word), ESearchCase::IgnoreCase))
				{
					Out.Add(Key);
					break;
				}
			}
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentNoDwellKeyTest, "StealthGame.Concealment.NoDwellKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentNoDwellKeyTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentNoDwellKeyTestData;

	// Arrange: the Concealment key set as compiled.
	TArray<FString> Keys;
	for (const FTuningSchemaRow& Row : GetTuningSchema())
	{
		if (Row.Owner != nullptr && FCStringAnsi::Strcmp(Row.Owner, StealthConcealmentTuningKnobs::Owner) == 0)
		{
			Keys.Add(FString(Row.Key));
		}
	}
	TestEqual(TEXT("the scan sees the eleven Concealment keys"), Keys.Num(), 11);

	// Act + assert: the real set is clean.
	const TArray<FString> Findings = FindForbidden(Keys);
	TestEqual(TEXT("no Concealment key names a dwell, capacity, stay, reveal or timeout"), Findings.Num(), 0);
	for (const FString& Finding : Findings)
	{
		AddError(FString::Printf(TEXT("forbidden key name: %s"), *Finding));
	}

	// Negative controls: planted names are flagged, mixed case included.
	TArray<FString> Planted = Keys;
	Planted.Add(TEXT("BushDwellSec"));
	TestEqual(TEXT("planted BushDwellSec is flagged"), FindForbidden(Planted), TArray<FString>({ TEXT("BushDwellSec") }));
	TestEqual(TEXT("mixed-case bushStayMax is flagged"), FindForbidden({ TEXT("bushStayMax") }).Num(), 1);
	TestEqual(TEXT("BushRevealTimeoutSec is flagged once"), FindForbidden({ TEXT("BushRevealTimeoutSec") }).Num(), 1);
	TestEqual(TEXT("BushMaxCapacityCount is flagged"), FindForbidden({ TEXT("BushMaxCapacityCount") }).Num(), 1);
	return true;
}

#endif
