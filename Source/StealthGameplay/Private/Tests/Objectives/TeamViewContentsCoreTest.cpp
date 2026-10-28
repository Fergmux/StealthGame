// Story oc-010 (production/epics/objectives-chain/story-010-team-view-struct-and-builder.md), AC2 (AC-OC-03 contents):
// StealthGame.Objectives.TeamViewContentsCore. On the Cacao chain at Active start, team 0's built view holds only P1[0] as a
// live site (row DefP1Team0) and team 1's only P1[1]; no view holds the Final, a P2 site or any opponent site, and the stage,
// epoch and chain serial are set. When team 0 completes Prelim1 its P1 leaves its live set and only P2[0] is live; team 1's
// view is unchanged by team 0's progress. When team 0 completes both preliminaries only team 0's view gains the Final; team 1
// still never holds it. FRoundPublicState (Round's public carrier) has no objective field at any depth.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesTestRig.h"
#include "UObject/UnrealType.h"
#include "View/StealthRoundPublicState.h"

namespace ObjectivesTeamViewContentsCoreTestData
{
	/** The courts of a view's live set. */
	TArray<uint16> Courts(const FObjectiveTeamView& View)
	{
		TArray<uint16> Out;
		for (const FObjectiveLiveSite& Site : View.LiveSites)
		{
			Out.Add(Site.Court.Value);
		}
		return Out;
	}

	bool Holds(const FObjectiveTeamView& View, uint16 Court) { return View.FindLiveSite(FCourtFixtureId(Court)) != nullptr; }

	/** Names (at any depth) of Struct's properties that look like objective data, or whose type is an Objectives view type. */
	void FindObjectiveFields(const UStruct* Struct, const FString& Prefix, TArray<FString>& Out)
	{
		static const TCHAR* const Words[] = { TEXT("Objective"), TEXT("Site"), TEXT("Court"), TEXT("Stage"), TEXT("Prelim"), TEXT("Rival"), TEXT("Clue") };
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FString Name = Prefix + It->GetName();
			for (const TCHAR* Word : Words)
			{
				if (It->GetName().Contains(Word))
				{
					Out.Add(Name);
				}
			}
			if (const FStructProperty* AsStruct = CastField<FStructProperty>(*It))
			{
				const UScriptStruct* Type = AsStruct->Struct;
				if (Type == FObjectiveTeamView::StaticStruct() || Type == FObjectiveLiveSite::StaticStruct() || Type == FClueTag::StaticStruct())
				{
					Out.Add(Name + TEXT(" (Objectives view type)"));
				}
				FindObjectiveFields(Type, Name + TEXT("."), Out);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesTeamViewContentsCoreTest, "StealthGame.Objectives.TeamViewContentsCore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesTeamViewContentsCoreTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace ObjectivesTeamViewContentsCoreTestData;

	// Arrange: the Cacao chain bound and Active.
	FCoreRig Rig;

	// Act + Assert: Active start.
	const FObjectiveTeamView Start0 = Rig.Core.BuildTeamView(0);
	const FObjectiveTeamView Start1 = Rig.Core.BuildTeamView(1);
	TestTrue(TEXT("start: team 0 live set = {P1[0]}"), Courts(Start0) == TArray<uint16>{ CourtP1Team0 });
	TestTrue(TEXT("start: team 1 live set = {P1[1]}"), Courts(Start1) == TArray<uint16>{ CourtP1Team1 });
	TestEqual(TEXT("start: team 0's site carries its card row"), Start0.LiveSites.Num() == 1 ? static_cast<int32>(Start0.LiveSites[0].DefIndex) : -1, DefP1Team0);
	TestEqual(TEXT("start: team 1's site carries its card row"), Start1.LiveSites.Num() == 1 ? static_cast<int32>(Start1.LiveSites[0].DefIndex) : -1, DefP1Team1);
	for (const FObjectiveTeamView* View : { &Start0, &Start1 })
	{
		TestFalse(TEXT("start: no view holds the Final"), Holds(*View, CourtFinal));
		TestFalse(TEXT("start: no view holds a P2 site"), Holds(*View, CourtP2Team0) || Holds(*View, CourtP2Team1));
		TestEqual(TEXT("start: Stage1"), static_cast<int32>(View->StageIndex), static_cast<int32>(EObjectiveTeamStage::Stage1));
		TestEqual(TEXT("start: RoundEpoch set"), static_cast<int64>(View->RoundEpoch), static_cast<int64>(RoundEpoch));
		TestEqual(TEXT("start: ChainSerial set"), static_cast<int32>(View->ChainSerial), static_cast<int32>(StealthObjectivesCacaoFixture::ChainSerial));
		TestEqual(TEXT("start: nothing completed"), static_cast<int32>(View->CompletedDefMask), 0);
		TestEqual(TEXT("start: no rival count"), static_cast<int32>(View->RivalPrelimCount), 0);
	}
	TestFalse(TEXT("start: team 0 never holds team 1's site"), Holds(Start0, CourtP1Team1));
	TestFalse(TEXT("start: team 1 never holds team 0's site"), Holds(Start1, CourtP1Team0));

	// Act: team 0 completes Prelim1.
	TestTrue(TEXT("team 0's Prelim1 counts"), Rig.Core.ApplyCountedCompletion(0, DefP1Team0).bApplied);
	const FObjectiveTeamView Mid0 = Rig.Core.BuildTeamView(0);
	TestTrue(TEXT("after P1: team 0 live set = {P2[0]} (the completed P1 left it)"), Courts(Mid0) == TArray<uint16>{ CourtP2Team0 });
	TestEqual(TEXT("after P1: Stage2"), static_cast<int32>(Mid0.StageIndex), static_cast<int32>(EObjectiveTeamStage::Stage2));
	TestEqual(TEXT("after P1: the completed row is in the mask"), static_cast<int32>(Mid0.CompletedDefMask), 1 << DefP1Team0);
	TestTrue(TEXT("after P1: team 1's view is unchanged by team 0's progress"), Rig.Core.BuildTeamView(1) == Start1);

	// Act: team 0 completes Prelim2.
	TestTrue(TEXT("team 0's Prelim2 counts"), Rig.Core.ApplyCountedCompletion(0, DefP2Team0).bApplied);
	const FObjectiveTeamView End0 = Rig.Core.BuildTeamView(0);
	const FObjectiveTeamView End1 = Rig.Core.BuildTeamView(1);
	TestTrue(TEXT("both prelims: team 0 live set = {Final}"), Courts(End0) == TArray<uint16>{ CourtFinal });
	TestEqual(TEXT("both prelims: the Final's card row"), End0.LiveSites.Num() == 1 ? static_cast<int32>(End0.LiveSites[0].DefIndex) : -1, DefFinal);
	TestEqual(TEXT("both prelims: team 0 at Final"), static_cast<int32>(End0.StageIndex), static_cast<int32>(EObjectiveTeamStage::Final));
	TestFalse(TEXT("both prelims: team 1 does not gain the Final"), Holds(End1, CourtFinal));
	TestTrue(TEXT("both prelims: team 1's view still unchanged"), End1 == Start1);
	TestFalse(TEXT("team 0 never holds an opponent site"), Holds(End0, CourtP1Team1) || Holds(End0, CourtP2Team1));

	// FRoundPublicState carries no objective field.
	TArray<FString> Fields;
	FindObjectiveFields(FRoundPublicState::StaticStruct(), TEXT("FRoundPublicState."), Fields);
	TestEqual(TEXT("FRoundPublicState has no objective field"), Fields.Num(), 0);
	for (const FString& Field : Fields)
	{
		AddError(FString::Printf(TEXT("objective-looking field on FRoundPublicState: %s"), *Field));
	}
	// Positive control: the same search finds the view's own fields.
	TArray<FString> Control;
	FindObjectiveFields(FObjectiveTeamView::StaticStruct(), TEXT("FObjectiveTeamView."), Control);
	TestTrue(TEXT("control: the search finds the view's own fields"), Control.Num() >= 3);
	return true;
}

#endif
