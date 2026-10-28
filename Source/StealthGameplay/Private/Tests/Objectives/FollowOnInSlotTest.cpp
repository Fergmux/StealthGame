// Story oc-009 (production/epics/objectives-chain/story-009-final-completion-and-win-declaration.md), AC2 / AC-OC-07
// FollowOnInSlot, on the real queue: a counted final completes through the hold stand-in's HoldTimer(Complete) slot and
// DeclareObjectiveWin is called INSIDE that slot (the queue's CurrentKey at the call is the key passed), with the key of the
// slot that completed the hold (the key the Completed event carried); FinalCompleted(team, Key) is logged with the same key;
// another item submitted in the same frame (a Begin elsewhere) resolves after the latch as Rejected_Latched (nothing
// interleaves the follow-on); no team-view write contains the final (no push after the final completion).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesFollowOnInSlotTest, "StealthGame.Objectives.FollowOnInSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesFollowOnInSlotTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	FQueueRig Rig;
	TestTrue(TEXT("Active"), Rig.ToActive());
	Rig.CompleteStageFast(0);
	Rig.CompleteStageFast(0);
	TestEqual(TEXT("team 0's final is Live"), static_cast<int32>(Rig.Core().GetSiteState(0, DefFinal)), static_cast<int32>(EObjectiveSiteState::Live));

	AStealthPawn* Tender = Rig.Pawn(CourtFinal, 0, 1, 0);
	AStealthPawn* Other = Rig.Pawn(CourtOrdinary, 0);
	FQueueKey CompletedKey;
	Rig.Hub->OnActionCompleted.AddLambda([&Rig, &CompletedKey, Tender](AStealthPawn* Pawn, EActionType) { if (Pawn == Tender) { CompletedKey = Rig.Q.Queue->CurrentKey(); } });
	Rig.Begin(Tender, CourtFinal);
	Rig.Q.Tick(0.1);
	const int32 PushesBefore = Rig.Views.Pushes.Num();
	const FInteractHoldStandIn::FHold* Hold = Rig.Holds->FindHold(Tender);
	if (!TestNotNull(TEXT("the final hold runs"), Hold))
	{
		return false;
	}
	Rig.RunToFrameBefore(Hold->DueServerTime);
	TestEqual(TEXT("not yet due"), Rig.Round.Wins.Num(), 0);
	// The completing frame: another command submitted in the same frame as the due hold (the due item is stamped first).
	Rig.Begin(Other, CourtOrdinary);
	Rig.Q.Tick(0.1);
	Rig.Hub->OnActionCompleted.Clear();

	if (TestEqual(TEXT("DeclareObjectiveWin called once"), Rig.Round.Wins.Num(), 1))
	{
		TestTrue(TEXT("called inside a slot with that slot's own key"), Rig.Round.SlotKeysAtWin[0].IsSet() && Rig.Round.SlotKeysAtWin[0] == Rig.Round.Wins[0].Value);
		TestTrue(TEXT("the key is the HoldTimer(Complete) slot's (the Completed event's)"), CompletedKey.IsSet() && Rig.Round.Wins[0].Value == CompletedKey);
		TestEqual(TEXT("for team 0"), static_cast<int32>(Rig.Round.Wins[0].Key), 0);
	}
	const TArray<FRoundLogEntry> Finals = Rig.LoggedOf(ERoundLogType::FinalCompleted);
	if (TestEqual(TEXT("FinalCompleted logged once"), Finals.Num(), 1))
	{
		TestTrue(TEXT("FinalCompleted(team 0, Key)"), Finals[0].Data[0] == 0 && Finals[0].Key == CompletedKey);
	}
	TestTrue(TEXT("the queue latched at the win"), Rig.Q.Queue->IsLatched());
	TestNull(TEXT("the item behind the follow-on did not execute (Rejected_Latched)"), Rig.Holds->FindHold(Other));
	TestTrue(TEXT("the queue logged a Rejected_Latched for it"), Rig.Logged(ERoundLogType::Rejected_Latched) >= 1);
	TestEqual(TEXT("no team-view write for the final"), Rig.Views.Pushes.Num(), PushesBefore);
	TestEqual(TEXT("no rival release for the final"), Rig.Rival.Calls.Num(), 2);
	return true;
}

#endif
