// Story oc-007 (production/epics/objectives-chain/story-007-interact-predicate-and-feint-parity.md), AC1 / AC-OC-04
// PredicateSiteAndClassOnly: GIVEN Active-class pawns (a human-bound Active pawn, an unbound crowd NPC) and an Informant WHEN
// CanBegin(Interact) is asked at a live, a locked, a completed, an opponent and the hidden-final court, each with a free stand
// point, THEN the Active pawns get Allow at all five and the Informant Reject(NotNow) at all five; with 3 of 4 stand points
// claimed it is still Allow; with all 4 claimed every pawn gets Reject(NotNow) at every court kind; it never returns
// AllowNoEffect. Static check: the predicate translation unit (ObjectivesInteractPredicate.cpp) calls no Roster team or
// binding query, no FSiteAssignment accessor and no FObjectivesCore site-state accessor; a seeded violation fails the scan.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Misc/FileHelper.h"
#include "ObjectivesQueueRig.h"
#include "ObjectivesSourceScan.h"

namespace ObjectivesPredicateSiteAndClassOnlyTestData
{
	const TArray<FString> Forbidden = {
		TEXT("GetTeam"), TEXT("GetParticipant"), TEXT("GetState"), TEXT("GetSiteAssignment_Server"), TEXT("FSiteAssignment"),
		TEXT("GetSiteState"), TEXT("FindLiveDefinition"), TEXT("GetLiveDefinitions"), TEXT("GetStageIndex"), TEXT("GetBinding"),
		TEXT("GetCompletedDefMask"), TEXT("FChainBinding"), TEXT("AllowNoEffect"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesPredicateSiteAndClassOnlyTest, "StealthGame.Objectives.PredicateSiteAndClassOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesPredicateSiteAndClassOnlyTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace ObjectivesPredicateSiteAndClassOnlyTestData;

	// Arrange: team 1 completed its Prelim1, so its P1 court is a completed site; team 0's P1 is live; team 0's P2 locked;
	// team 1's P2 (locked for team 1) is the opponent's court to team 0; the final is hidden from both.
	FQueueRig Rig;
	TestTrue(TEXT("Active"), Rig.ToActive());
	Rig.CompleteStageFast(1);
	struct FCourtCase { const TCHAR* Name; uint16 Court; };
	const FCourtCase Courts[] = {
		{ TEXT("live"), CourtP1Team0 }, { TEXT("locked"), CourtP2Team0 }, { TEXT("completed"), CourtP1Team1 },
		{ TEXT("opponent"), CourtP2Team1 }, { TEXT("hidden final"), CourtFinal },
	};
	AStealthPawn* Human = Rig.Pawn(CourtOrdinary, 0, 1, 0);
	AStealthPawn* Npc = Rig.Pawn(CourtOrdinary, 1);
	AStealthPawn* Informant = Rig.Pawn(CourtOrdinary, 2, 2, 0);
	Rig.Roster.States[2] = EParticipantState::Informant;
	struct FPawnCase { const TCHAR* Name; AStealthPawn* Pawn; bool bAllowed; };
	const FPawnCase Pawns[] = { { TEXT("human-bound Active"), Human, true }, { TEXT("unbound NPC"), Npc, true }, { TEXT("Informant"), Informant, false } };
	auto Ask = [&Rig](AStealthPawn* Pawn, uint16 Court)
	{
		FCommandPayload Cmd;
		Cmd.Target = FSubjectRef::ForCourtSocket(Court);
		return Rig.Service->CanBegin(*Pawn, Cmd);
	};

	// A free stand point at every court.
	for (const FCourtCase& Court : Courts)
	{
		for (const FPawnCase& Pawn : Pawns)
		{
			const FPredicateResult Result = Ask(Pawn.Pawn, Court.Court);
			TestTrue(FString::Printf(TEXT("%s at the %s court: %s"), Pawn.Name, Court.Name, Pawn.bAllowed ? TEXT("Allow") : TEXT("Reject(NotNow)")),
				Result == (Pawn.bAllowed ? FPredicateResult::Allow() : FPredicateResult::Reject(ECommandRejectCode::NotNow)));
			TestTrue(FString::Printf(TEXT("%s at the %s court: never AllowNoEffect"), Pawn.Name, Court.Name), Result.Verdict != EPredicateVerdict::AllowNoEffect);
		}
	}

	// 3 of 4 stand points claimed: still Allow; all 4: Reject(NotNow) for every pawn at every court kind.
	for (const FCourtCase& Court : Courts)
	{
		TArray<AStealthPawn*> Tenders;
		for (int32 Face = 0; Face < 4; ++Face)
		{
			Tenders.Add(Rig.Pawn(Court.Court, Face));
		}
		for (int32 Face = 0; Face < 3; ++Face)
		{
			Rig.Service->OnInteractExecuted(*Tenders[Face], FCourtFixtureId(Court.Court), FQueueKey(1, 1));
		}
		TestTrue(FString::Printf(TEXT("%s court, 3 of 4 claimed: Allow"), Court.Name), Ask(Human, Court.Court) == FPredicateResult::Allow());
		Rig.Service->OnInteractExecuted(*Tenders[3], FCourtFixtureId(Court.Court), FQueueKey(1, 1));
		for (const FPawnCase& Pawn : Pawns)
		{
			TestTrue(FString::Printf(TEXT("%s court, all 4 claimed: %s gets Reject(NotNow)"), Court.Name, Pawn.Name),
				Ask(Pawn.Pawn, Court.Court) == FPredicateResult::Reject(ECommandRejectCode::NotNow));
		}
		// Occupancy is re-read on every call: one release frees a stand point again.
		Rig.Service->OnInteractEnded(*Tenders[0], FCourtFixtureId(Court.Court), EInteractEnd::Cancelled, FQueueKey());
		TestTrue(FString::Printf(TEXT("%s court, a claim released: Allow again"), Court.Name), Ask(Human, Court.Court) == FPredicateResult::Allow());
	}

	// Static check of the predicate translation unit.
	FString Text;
	const FString Path = FPaths::Combine(StealthObjectivesSourceScan::SourceRoot(), TEXT("StealthGameplay/Features/Objectives/Private/ObjectivesInteractPredicate.cpp"));
	if (TestTrue(TEXT("the predicate translation unit exists"), FFileHelper::LoadFileToString(Text, *Path)))
	{
		StealthObjectivesSourceScan::FSource Source;
		Source.Path = TEXT("ObjectivesInteractPredicate.cpp");
		Source.Text = Text;
		const TArray<FString> Findings = StealthObjectivesSourceScan::FindNames({ Source }, Forbidden);
		TestEqual(TEXT("the predicate reads only the court, its occupancy and the legality class"), Findings.Num(), 0);
		for (const FString& Finding : Findings)
		{
			AddError(Finding);
		}
	}
	StealthObjectivesSourceScan::FSource Seeded;
	Seeded.Path = TEXT("Seeded.cpp");
	Seeded.Text = TEXT("if (Roster->GetTeam(Roster->GetParticipant(Pawn)) == 0 && Core->GetSiteState(0, 1) == Live) return FPredicateResult::AllowNoEffect();");
	TestTrue(TEXT("seeded: a team / binding / site-state read and AllowNoEffect are reported"), StealthObjectivesSourceScan::FindNames({ Seeded }, Forbidden).Num() >= 4);
	return true;
}

#endif
