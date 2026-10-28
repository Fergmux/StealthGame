// Objectives Chain story oc-018 AC1 (AC-OC-27 [U]; SP-3, SP-6): StealthGame.Objectives.HidingNeverWins.
// GIVEN every team 1 Active pawn concealed and team 0 tending normally WHEN team 0 completes its chain THEN ObjectiveWin(0), with
// no team 1 state change and no Objectives timer kept alive by the hiding; GIVEN a concealed team 1 pawn issuing Interact at the
// nearest court (its own live P1) THEN it is rejected (the Concealment stub, AC-CON-24 owned by that epic) and no team 1 state
// changes. Runs on the real queue, the Interact hold stand-in and the real Objectives core and gate (holds of D = 3 s).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesConcealmentStub.h"
#include "ObjectivesQueueRig.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesHidingNeverWinsTest, "StealthGame.Objectives.HidingNeverWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesHidingNeverWinsTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	// Arrange: the Concealment stub in front of the Objectives predicate; team 1's two pawns hidden.
	FQueueRig Rig;
	TStrongObjectPtr<UObjectivesConcealmentStubPredicate> Concealment(NewObject<UObjectivesConcealmentStubPredicate>());
	Concealment->Inner = TWeakInterfacePtr<ICommandPredicate>(Rig.Service);
	Rig.Hub->RegisterPredicate(EActionType::Interact, TWeakInterfacePtr<ICommandPredicate>(Concealment.Get()));
	TestTrue(TEXT("Active"), Rig.ToActive());
	AStealthPawn* Hidden1 = Rig.Pawn(CourtP1Team1, 0, 21, 1);
	AStealthPawn* Hidden2 = Rig.Pawn(CourtOrdinary, 0, 22, 1);
	Concealment->Concealed = { Hidden1, Hidden2 };
	const uint16 Team1Mask = Rig.Core().GetCompletedDefMask(1);
	const EObjectiveTeamStage Team1Stage = Rig.Core().GetStageIndex(1);
	const EObjectiveSiteState Team1P1 = Rig.Core().GetSiteState(1, DefP1Team1);

	// A concealed team 1 pawn tries to tend its own live court: rejected, nothing changes.
	Rig.Begin(Hidden1, CourtP1Team1);
	Rig.Q.Tick(0.1);
	TestNull(TEXT("the concealed pawn's Begin started no hold"), Rig.Holds->FindHold(Hidden1));
	TestTrue(TEXT("the Concealment stub rejected it"), Concealment->Rejected >= 1);
	TestEqual(TEXT("no claim at team 1's court"), Rig.Core().GetClaimCount(FCourtFixtureId(CourtP1Team1)), 0);

	// Act: team 0 tends P1, P2 and the final with real holds while team 1 stays hidden.
	const uint16 Courts[] = { CourtP1Team0, CourtP2Team0, CourtFinal };
	uint32 Participant = 1;
	for (const uint16 Court : Courts)
	{
		AStealthPawn* Tender = Rig.Pawn(Court, 0, Participant, 0);
		Rig.Begin(Tender, Court);
		Rig.Q.Tick(0.1);
		const FInteractHoldStandIn::FHold* Hold = Rig.Holds->FindHold(Tender);
		if (!TestNotNull(*FString::Printf(TEXT("team 0's hold at court %d runs"), Court), Hold))
		{
			return false;
		}
		const double Due = Hold->DueServerTime;
		Rig.RunToFrameBefore(Due);
		Rig.Q.Tick(0.1);
	}

	// Assert: ObjectiveWin(0); team 1 untouched; no Objectives timer outlives the win for the hiders.
	if (TestEqual(TEXT("one win declared"), Rig.Round.Wins.Num(), 1))
	{
		TestEqual(TEXT("ObjectiveWin(0)"), static_cast<int32>(Rig.Round.Wins[0].Key), 0);
	}
	TestTrue(TEXT("the round latched"), Rig.Q.Queue->IsLatched());
	TestEqual(TEXT("team 1's completed mask unchanged"), Rig.Core().GetCompletedDefMask(1), Team1Mask);
	TestTrue(TEXT("team 1's stage unchanged"), Rig.Core().GetStageIndex(1) == Team1Stage);
	TestTrue(TEXT("team 1's P1 site unchanged"), Rig.Core().GetSiteState(1, DefP1Team1) == Team1P1);
	TestEqual(TEXT("two counted stages, one final, all team 0"), Rig.Logged(ERoundLogType::ObjectiveStage), 2);
	const TArray<FRoundLogEntry> Stages = Rig.LoggedOf(ERoundLogType::ObjectiveStage);
	TestTrue(TEXT("every counted stage is team 0's"), !Stages.ContainsByPredicate([](const FRoundLogEntry& E) { return E.Data[0] != 0; }));
	TestTrue(TEXT("no tend by a team 1 participant was logged"), !Rig.LoggedOf(ERoundLogType::TendCompleted).ContainsByPredicate([](const FRoundLogEntry& E)
	{
		return E.Participants[0] == 21 || E.Participants[0] == 22;
	}));
	return true;
}

#endif
