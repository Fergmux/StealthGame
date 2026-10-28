// Story oc-002 (production/epics/objectives-chain/story-002-chain-data-types-and-cacao-asset.md), AC2 (OC-16, OC-3)
// CacaoChainContent: DA_ObjectiveChain_Cacao loaded holds exactly the five OC-16 rows (id, stage, eligibility, site role,
// card title, card line equal to the constant fixture View/Testing/ObjectiveCacaoChainFixture.h), ChainSerial is set, at most
// 16 rows, card text non-empty (whitespace-only counts as empty), ids unique, and no title or line contains objective,
// target, enemy, player or NPC as a case-insensitive whole word. The rule helpers are exercised on their edge cases first
// (whitespace-only, 16 vs 17 rows, "targets" and "Players" are not the whole words).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/ObjectiveChainData.h"
#include "View/Testing/ObjectiveCacaoChainFixture.h"

namespace ObjectivesCacaoChainContentTestData
{
	bool IsBlank(const FText& Text)
	{
		return Text.ToString().TrimStartAndEnd().IsEmpty();
	}

	bool IsWordChar(TCHAR C)
	{
		return FChar::IsAlnum(C) || C == TEXT('_');
	}

	/** True when Word occurs in Text as a case-insensitive whole word. */
	bool HasWholeWord(const FString& Text, const FString& Word)
	{
		int32 From = 0;
		while (true)
		{
			const int32 At = Text.Find(Word, ESearchCase::IgnoreCase, ESearchDir::FromStart, From);
			if (At == INDEX_NONE)
			{
				return false;
			}
			const int32 End = At + Word.Len();
			if ((At == 0 || !IsWordChar(Text[At - 1])) && (End >= Text.Len() || !IsWordChar(Text[End])))
			{
				return true;
			}
			From = At + 1;
		}
	}

	bool HasForbiddenWord(const FString& Text)
	{
		for (const TCHAR* Word : StealthObjectivesCacaoFixture::ForbiddenWords)
		{
			if (HasWholeWord(Text, Word))
			{
				return true;
			}
		}
		return false;
	}

	bool WithinRowBound(int32 Rows)
	{
		return Rows <= UObjectiveChainData::MaxDefinitions;
	}

	FString AssetPath()
	{
		return FString::Printf(TEXT("%s/%s.%s"), StealthObjectivesCacaoFixture::AssetFolder, StealthObjectivesCacaoFixture::AssetName, StealthObjectivesCacaoFixture::AssetName);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesCacaoChainContentTest, "StealthGame.Objectives.CacaoChainContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesCacaoChainContentTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesCacaoChainContentTestData;

	// The rule helpers on their edges.
	TestTrue(TEXT("whitespace-only text counts as empty"), IsBlank(FText::FromString(TEXT("   \t"))));
	TestFalse(TEXT("real text is not empty"), IsBlank(FText::FromString(TEXT("Tend"))));
	TestTrue(TEXT("16 rows are within the bound"), WithinRowBound(16));
	TestFalse(TEXT("17 rows exceed the bound"), WithinRowBound(17));
	TestFalse(TEXT("'targets' is not the whole word target"), HasForbiddenWord(TEXT("Mark the targets")));
	TestFalse(TEXT("'Players' is not the whole word player"), HasForbiddenWord(TEXT("Players gather")));
	TestTrue(TEXT("'Enemy' is the whole word enemy (case-insensitive)"), HasForbiddenWord(TEXT("Find the Enemy.")));
	TestTrue(TEXT("'npc' is the whole word NPC"), HasForbiddenWord(TEXT("an npc waits")));

	// Arrange: the shipped asset.
	const UObjectiveChainData* Chain = LoadObject<UObjectiveChainData>(nullptr, *AssetPath());
	if (!TestNotNull(TEXT("DA_ObjectiveChain_Cacao loads"), Chain))
	{
		return false;
	}

	// Assert.
	TestEqual(TEXT("ChainSerial is the authored serial"), static_cast<int32>(Chain->ChainSerial), static_cast<int32>(StealthObjectivesCacaoFixture::ChainSerial));
	TestTrue(TEXT("ChainSerial is set"), Chain->ChainSerial != 0);
	TestTrue(TEXT("at most 16 rows"), WithinRowBound(Chain->Definitions.Num()));
	TestEqual(TEXT("exactly the five OC-16 rows"), Chain->Definitions.Num(), static_cast<int32>(UE_ARRAY_COUNT(StealthObjectivesCacaoFixture::Rows)));
	TSet<FName> Ids;
	for (int32 Index = 0; Index < Chain->Definitions.Num(); ++Index)
	{
		const FObjectiveDef& Def = Chain->Definitions[Index];
		bool bDuplicate = false;
		Ids.Add(Def.ObjectiveId, &bDuplicate);
		TestFalse(FString::Printf(TEXT("row %d: id unique"), Index), bDuplicate);
		TestFalse(FString::Printf(TEXT("row %d: title non-empty"), Index), IsBlank(Def.CardTitle));
		TestFalse(FString::Printf(TEXT("row %d: line non-empty"), Index), IsBlank(Def.CardLine));
		TestFalse(FString::Printf(TEXT("row %d: title has no forbidden word"), Index), HasForbiddenWord(Def.CardTitle.ToString()));
		TestFalse(FString::Printf(TEXT("row %d: line has no forbidden word"), Index), HasForbiddenWord(Def.CardLine.ToString()));
		TestEqual(FString::Printf(TEXT("row %d: one completion"), Index), Def.CompletionsRequiredCount, 1);
		if (Index >= UE_ARRAY_COUNT(StealthObjectivesCacaoFixture::Rows))
		{
			continue;
		}
		const StealthObjectivesCacaoFixture::FRow& Want = StealthObjectivesCacaoFixture::Rows[Index];
		TestEqual(FString::Printf(TEXT("row %d: id"), Index), Def.ObjectiveId, FName(Want.ObjectiveId));
		TestEqual(FString::Printf(TEXT("row %d: stage"), Index), static_cast<int32>(Def.StageKind), static_cast<int32>(Want.Stage));
		TestEqual(FString::Printf(TEXT("row %d: eligibility"), Index), static_cast<int32>(Def.TeamEligibility), static_cast<int32>(Want.Eligibility));
		TestEqual(FString::Printf(TEXT("row %d: site role"), Index), static_cast<int32>(Def.SiteRole), static_cast<int32>(Want.Role));
		TestEqual(FString::Printf(TEXT("row %d: card title"), Index), Def.CardTitle.ToString(), FString(Want.CardTitle));
		TestEqual(FString::Printf(TEXT("row %d: card line"), Index), Def.CardLine.ToString(), FString(Want.CardLine));
	}
	return true;
}

#endif
