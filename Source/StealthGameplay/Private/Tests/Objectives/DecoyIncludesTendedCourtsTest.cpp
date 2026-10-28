// Story oc-007 (production/epics/objectives-chain/story-007-interact-predicate-and-feint-parity.md), AC3 / AC-OC-31 (Objectives
// side) DecoyIncludesTendedCourts: GIVEN a court with one stand point taken by a tending human THEN a decoy's CanBegin is Allow
// and its Begin (through the queue) takes a free stand point (not the human's); WHEN all four stand points are taken THEN a
// further decoy's CanBegin is Reject(NotNow) and its Begin is Voided at the slot (no hold, no claim). The 2,000-trial uniform
// court-choice chi-square is the Crowd epic's.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesDecoyIncludesTendedCourtsTest, "StealthGame.Objectives.DecoyIncludesTendedCourts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesDecoyIncludesTendedCourtsTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	FQueueRig Rig;
	TestTrue(TEXT("Active"), Rig.ToActive());
	const FCourtFixtureId Court(CourtP1Team0);
	FCommandPayload Cmd;
	Cmd.Target = FSubjectRef::ForCourtSocket(CourtP1Team0);

	AStealthPawn* Human = Rig.Pawn(CourtP1Team0, 0, 1, 0);
	Rig.Begin(Human, CourtP1Team0);
	Rig.Q.Tick(0.1);
	TestEqual(TEXT("the human holds one stand point"), Rig.Core().GetClaimCount(Court), 1);

	AStealthPawn* Decoy = Rig.Pawn(CourtP1Team0, 0); // standing where the human tends: it takes another face
	TestTrue(TEXT("the decoy's CanBegin at the tended court is Allow"), Rig.Service->CanBegin(*Decoy, Cmd) == FPredicateResult::Allow());
	Rig.Begin(Decoy, CourtP1Team0);
	Rig.Q.Tick(0.1);
	TestNotNull(TEXT("the decoy's Begin executed (a hold runs)"), Rig.Holds->FindHold(Decoy));
	const int32 DecoyFace = Rig.Core().GetClaims().FindFace(Court, Decoy);
	TestTrue(TEXT("the decoy took a free stand point"), DecoyFace != INDEX_NONE && DecoyFace != Rig.Core().GetClaims().FindFace(Court, Human));
	TestEqual(TEXT("two stand points claimed"), Rig.Core().GetClaimCount(Court), 2);

	// Fill the fixture, then a further decoy is refused.
	AStealthPawn* Third = Rig.Pawn(CourtP1Team0, 2);
	AStealthPawn* Fourth = Rig.Pawn(CourtP1Team0, 3);
	Rig.Begin(Third, CourtP1Team0);
	Rig.Begin(Fourth, CourtP1Team0);
	Rig.Q.Tick(0.1);
	TestEqual(TEXT("all four stand points claimed"), Rig.Core().GetClaimCount(Court), 4);
	AStealthPawn* Late = Rig.Pawn(CourtP1Team0, 1);
	TestTrue(TEXT("a decoy at the full fixture: Reject(NotNow)"), Rig.Service->CanBegin(*Late, Cmd) == FPredicateResult::Reject(ECommandRejectCode::NotNow));
	Rig.Begin(Late, CourtP1Team0);
	Rig.Q.Tick(0.1);
	TestNull(TEXT("its Begin is Voided at the slot: no hold"), Rig.Holds->FindHold(Late));
	TestEqual(TEXT("still four claims"), Rig.Core().GetClaimCount(Court), 4);
	return true;
}

#endif
