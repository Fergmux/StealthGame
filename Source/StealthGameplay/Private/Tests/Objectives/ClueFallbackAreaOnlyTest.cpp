// Objectives Chain story oc-013 AC3 (ADR-0020 section 4 [U]): StealthGame.Objectives.ClueFallbackAreaOnly.
// FClueTag is {uint8 AreaType, uint8 FeatureKind}; a court record with no feature has FeatureKind None and the clue is the area
// type only; the phrase is built client-side from the localised phrase table (BuildClueText); a key outside the table returns
// the safe fallback "A court" and a warning; the same inputs give the same text; no string member (FString, FText, FName) exists
// anywhere in the replicated view (FObjectiveTeamView, FObjectiveLiveSite, FClueTag).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesClue.h"
#include "UObject/UnrealType.h"
#include "View/ObjectiveTeamView.h"
#include "View/ObjectiveViewHelpers.h"

namespace ClueFallbackAreaOnlyTestData
{
	/** Every property of Struct (recursing into struct and array-of-struct members) that is a string type. */
	void StringMembers(const UScriptStruct* Struct, const FString& Prefix, TArray<FString>& Out, int32 Depth = 0)
	{
		if (Struct == nullptr || Depth > 4)
		{
			return;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			const FProperty* Inner = Property;
			if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				Inner = Array->Inner;
			}
			const FString Name = Prefix + Property->GetName();
			if (Inner->IsA<FStrProperty>() || Inner->IsA<FTextProperty>() || Inner->IsA<FNameProperty>())
			{
				Out.Add(Name);
			}
			if (const FStructProperty* Nested = CastField<FStructProperty>(Inner))
			{
				StringMembers(Nested->Struct, Name + TEXT("."), Out, Depth + 1);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesClueFallbackAreaOnlyTest, "StealthGame.Objectives.ClueFallbackAreaOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesClueFallbackAreaOnlyTest::RunTest(const FString& Parameters)
{
	// The tag's shape: two uint8 properties, two bytes.
	const UScriptStruct* TagStruct = FClueTag::StaticStruct();
	TArray<FString> Fields;
	for (TFieldIterator<FProperty> It(TagStruct); It; ++It)
	{
		Fields.Add(FString::Printf(TEXT("%s:%s"), *It->GetName(), *It->GetCPPType()));
	}
	TestEqual(TEXT("FClueTag is {uint8 AreaType, uint8 FeatureKind}"), FString::Join(Fields, TEXT(",")), FString(TEXT("AreaType:uint8,FeatureKind:uint8")));
	TestEqual(TEXT("FClueTag is two bytes"), static_cast<int32>(sizeof(FClueTag)), 2);

	// A court with no feature: FeatureKind None, the area-only clue.
	FCourtClueRecord NoFeature;
	NoFeature.Court = FCourtFixtureId(7);
	NoFeature.AreaTypeId = static_cast<uint8>(EClueAreaType::Temple);
	const FClueTag AreaOnly = FObjectivesClue::ClueOf(NoFeature);
	TestEqual(TEXT("no feature: FeatureKind None"), AreaOnly.FeatureKind, static_cast<uint8>(EClueFeatureKind::None));
	TestEqual(TEXT("the area-only phrase"), ObjectiveViewHelpers::BuildClueText(AreaOnly).ToString(), FString(TEXT("A court in the temple quarter")));

	// Area and feature from the table; the same inputs give the same text.
	FClueTag Full;
	Full.AreaType = static_cast<uint8>(EClueAreaType::Market);
	Full.FeatureKind = static_cast<uint8>(EClueFeatureKind::WaterSteps);
	const FString Phrase = ObjectiveViewHelpers::BuildClueText(Full).ToString();
	TestEqual(TEXT("area and feature"), Phrase, FString(TEXT("A court in the market, by the water steps")));
	TestEqual(TEXT("deterministic"), ObjectiveViewHelpers::BuildClueText(Full).ToString(), Phrase);

	// Every key of the table has a phrase (the key set is complete).
	int32 Missing = 0;
	for (uint8 Area = 1; Area <= GClueAreaTypeMax; ++Area)
	{
		for (uint8 Feature = 0; Feature <= GClueFeatureKindMax; ++Feature)
		{
			FClueTag Key;
			Key.AreaType = Area;
			Key.FeatureKind = Feature;
			Missing += ObjectiveViewHelpers::BuildClueText(Key).ToString() == TEXT("A court") ? 1 : 0;
		}
	}
	TestEqual(TEXT("every (area, feature) key of the table has a phrase"), Missing, 0);

	// A key outside the table: the safe fallback and a warning; an unknown record value maps to None on the server side.
	AddExpectedMessage(TEXT("clue phrase table has no key"), EAutomationExpectedErrorFlags::Contains, 2);
	FClueTag Unknown;
	Unknown.AreaType = 200;
	TestEqual(TEXT("an unknown area: the fallback"), ObjectiveViewHelpers::BuildClueText(Unknown).ToString(), FString(TEXT("A court")));
	Unknown.AreaType = static_cast<uint8>(EClueAreaType::Market);
	Unknown.FeatureKind = 99;
	TestEqual(TEXT("an unknown feature: the fallback"), ObjectiveViewHelpers::BuildClueText(Unknown).ToString(), FString(TEXT("A court")));
	FCourtClueRecord Bad;
	Bad.AreaTypeId = 200;
	Bad.NearestFeatureKind = 99;
	const FClueTag Sanitised = FObjectivesClue::ClueOf(Bad);
	TestTrue(TEXT("ClueOf maps values outside the key set to None"), Sanitised.AreaType == 0 && Sanitised.FeatureKind == 0);
	TestTrue(TEXT("no clue ({0, 0}) builds no clue line"), ObjectiveViewHelpers::BuildClueText(FClueTag()).IsEmpty());

	// No string member anywhere in the replicated view.
	TArray<FString> Strings;
	ClueFallbackAreaOnlyTestData::StringMembers(FObjectiveTeamView::StaticStruct(), TEXT("FObjectiveTeamView."), Strings);
	TestEqual(TEXT("the replicated view has no FString / FText / FName member"), FString::Join(Strings, TEXT(", ")), FString());
	// Negative control: the scan sees a string member where one exists (FObjectiveDef's card text, which never travels).
	TArray<FString> Control;
	ClueFallbackAreaOnlyTestData::StringMembers(FObjectiveDef::StaticStruct(), TEXT("FObjectiveDef."), Control);
	TestTrue(TEXT("control: the scan finds FObjectiveDef's FText and FName members"), Control.Num() >= 3);
	return true;
}

#endif
