// Story oc-002 (production/epics/objectives-chain/story-002-chain-data-types-and-cacao-asset.md), AC1 / AC-OC-01 static check
// ChainDefShape: by reflection, FObjectiveDef has exactly the fields ObjectiveId, StageKind, TeamEligibility, SiteRole,
// CompletionsRequiredCount, CardTitle, CardLine, ClueForm (order not asserted) and no property whose name contains "Hold" or
// "InteractSec" (no per-row hold time, ADR-0020 section 2; the user removed InteractSecOverride 2026-10-25). A seeded control
// struct with a HoldSec member fails the same check (ADR-0008 negative control).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesTestTypes.h"
#include "UObject/UnrealType.h"
#include "View/ObjectiveChainData.h"

namespace ObjectivesChainDefShapeTestData
{
	const TCHAR* const ExpectedFields[] = {
		TEXT("ObjectiveId"), TEXT("StageKind"), TEXT("TeamEligibility"), TEXT("SiteRole"), TEXT("CompletionsRequiredCount"),
		TEXT("CardTitle"), TEXT("CardLine"), TEXT("ClueForm"),
	};

	/** Property names of Struct whose name contains Hold or InteractSec (the per-row hold-time check). */
	TArray<FString> HoldFields(const UScriptStruct* Struct)
	{
		TArray<FString> Out;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FString Name = It->GetName();
			if (Name.Contains(TEXT("Hold"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("InteractSec"), ESearchCase::IgnoreCase))
			{
				Out.Add(Name);
			}
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesChainDefShapeTest, "StealthGame.Objectives.ChainDefShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesChainDefShapeTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesChainDefShapeTestData;

	// Arrange: the reflected field set.
	TSet<FString> Fields;
	for (TFieldIterator<FProperty> It(FObjectiveDef::StaticStruct()); It; ++It)
	{
		Fields.Add(It->GetName());
	}

	// Assert: exactly the OC-2 fields.
	TestEqual(TEXT("FObjectiveDef has exactly the eight OC-2 fields"), Fields.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedFields)));
	for (const TCHAR* Field : ExpectedFields)
	{
		TestTrue(FString::Printf(TEXT("FObjectiveDef has %s"), Field), Fields.Contains(Field));
	}
	TestEqual(TEXT("FObjectiveDef has no hold-time field (no Hold / InteractSec in any name)"), HoldFields(FObjectiveDef::StaticStruct()).Num(), 0);

	// Negative control: the seeded struct with HoldSec fails the same check.
	const TArray<FString> Seeded = HoldFields(FObjectivesSeededHoldDef::StaticStruct());
	TestEqual(TEXT("seeded: a HoldSec member is found"), Seeded.Num(), 1);
	TestTrue(TEXT("seeded: the finding names HoldSec"), Seeded.Contains(TEXT("HoldSec")));
	return true;
}

#endif
