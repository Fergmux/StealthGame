// Story oc-008 (production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md), AC3 (OC-10 stage effect)
// StageCompletionEffects, through the queue (the hold stand-in's HoldTimer(Complete) slot): a bound human pawn completing a
// tend at its team's Live P1 site marks the site Completed and makes the next stage Live for that team only, in the same slot;
// logs ObjectiveStage(team, stage, ObjectiveId as the row index with ChainSerial, ParticipantId, Key = the slot key); pushes the
// team's view; schedules the rival release (the stub observes team 0, ordinal 1); and appends TendCompleted(P, Court, Counted)
// after the ObjectiveStage entry. A bound pawn's no-effect tend appends TendCompleted(NoEffect) only; an unbound NPC decoy's
// completion appends nothing. The opponent team's state is untouched.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesStageCompletionEffectsTest, "StealthGame.Objectives.StageCompletionEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesStageCompletionEffectsTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	FQueueRig Rig;
	TestTrue(TEXT("Active"), Rig.ToActive());
	AStealthPawn* Tender = Rig.Pawn(CourtP1Team0, 0, 1, 0);
	AStealthPawn* Feinter = Rig.Pawn(CourtOrdinary, 0, 2, 0);
	AStealthPawn* Decoy = Rig.Pawn(CourtOrdinary, 1);
	TArray<EObjectiveSiteState> Team1Before;
	for (int32 Def = 0; Def < Rig.Core().GetDefinitionCount(); ++Def)
	{
		Team1Before.Add(Rig.Core().GetSiteState(1, Def));
	}
	const int32 Team0Pushes = Rig.Views.PushesFor(0);
	const int32 Team1Pushes = Rig.Views.PushesFor(1);

	// The counted tend.
	FQueueKey CompleteKey;
	Rig.Hub->OnActionCompleted.AddLambda([&Rig, &CompleteKey, Tender](AStealthPawn* Pawn, EActionType) { if (Pawn == Tender) { CompleteKey = Rig.Q.Queue->CurrentKey(); } });
	Rig.Begin(Tender, CourtP1Team0);
	Rig.Run(3.2);
	Rig.Hub->OnActionCompleted.Clear();
	TestEqual(TEXT("the P1 site is Completed"), static_cast<int32>(Rig.Core().GetSiteState(0, DefP1Team0)), static_cast<int32>(EObjectiveSiteState::Completed));
	TestEqual(TEXT("team 0's P2 is Live"), static_cast<int32>(Rig.Core().GetSiteState(0, DefP2Team0)), static_cast<int32>(EObjectiveSiteState::Live));
	for (int32 Def = 0; Def < Rig.Core().GetDefinitionCount(); ++Def)
	{
		TestEqual(FString::Printf(TEXT("team 1 row %d untouched"), Def), static_cast<int32>(Rig.Core().GetSiteState(1, Def)), static_cast<int32>(Team1Before[Def]));
	}
	const TArray<FRoundLogEntry> Stages = Rig.LoggedOf(ERoundLogType::ObjectiveStage);
	if (TestEqual(TEXT("one ObjectiveStage"), Stages.Num(), 1))
	{
		const FRoundLogEntry& E = Stages[0];
		TestEqual(TEXT("ObjectiveStage team"), E.Data[0], 0u);
		TestEqual(TEXT("ObjectiveStage stage (Prelim1)"), E.Data[1], static_cast<uint32>(EObjectiveStage::Prelim1));
		TestEqual(TEXT("ObjectiveStage objective (row index)"), E.Data[2], static_cast<uint32>(DefP1Team0));
		TestEqual(TEXT("ObjectiveStage chain serial"), E.Data[3], static_cast<uint32>(StealthObjectivesCacaoFixture::ChainSerial));
		TestEqual(TEXT("ObjectiveStage participant"), E.Participants[0], 1u);
		TestTrue(TEXT("ObjectiveStage carries the HoldTimer(Complete) slot's key"), CompleteKey.IsSet() && E.Key == CompleteKey);
	}
	TestEqual(TEXT("team 0's view pushed once more"), Rig.Views.PushesFor(0), Team0Pushes + 1);
	TestEqual(TEXT("team 1's view not pushed (opponents receive nothing)"), Rig.Views.PushesFor(1), Team1Pushes);
	if (TestEqual(TEXT("one rival release scheduled"), Rig.Rival.Calls.Num(), 1))
	{
		TestTrue(TEXT("for team 0's completion, ordinal 1"), Rig.Rival.Calls[0].Key == 0 && Rig.Rival.Calls[0].Value == 1);
	}
	const TArray<FRoundLogEntry> Tends = Rig.LoggedOf(ERoundLogType::TendCompleted);
	if (TestEqual(TEXT("one TendCompleted for the counted tend"), Tends.Num(), 1))
	{
		TestTrue(TEXT("TendCompleted(P, court, Counted)"), Tends[0].Participants[0] == 1 && Tends[0].Data[0] == CourtP1Team0 && Tends[0].Code == static_cast<uint16>(ERoundTendResult::Counted));
		const int32 StageIndex = Rig.Q.Log.Entries.IndexOfByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::ObjectiveStage; });
		const int32 TendIndex = Rig.Q.Log.Entries.IndexOfByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::TendCompleted; });
		TestTrue(TEXT("stable order: ObjectiveStage, then TendCompleted"), StageIndex != INDEX_NONE && StageIndex < TendIndex);
	}

	// A bound pawn's no-effect tend, and an unbound decoy's.
	Rig.Begin(Feinter, CourtOrdinary);
	Rig.Begin(Decoy, CourtOrdinary);
	Rig.Run(3.2);
	const TArray<FRoundLogEntry> AllTends = Rig.LoggedOf(ERoundLogType::TendCompleted);
	TestEqual(TEXT("the bound feint adds one TendCompleted, the decoy none"), AllTends.Num(), 2);
	if (AllTends.Num() == 2)
	{
		TestTrue(TEXT("the feint's is NoEffect with participant 2"), AllTends[1].Participants[0] == 2 && AllTends[1].Code == static_cast<uint16>(ERoundTendResult::NoEffect));
	}
	TestEqual(TEXT("still one ObjectiveStage"), Rig.Logged(ERoundLogType::ObjectiveStage), 1);
	TestEqual(TEXT("still one rival release"), Rig.Rival.Calls.Num(), 1);
	return true;
}

#endif
