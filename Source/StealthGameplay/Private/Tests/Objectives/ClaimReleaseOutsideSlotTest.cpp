// Story oc-006 (production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md), AC3 (ADR-0020 5.1,
// OC-8) ClaimReleaseOutsideSlot:
//   - OnInteractEnded with an invalid FQueueKey (round-end staging, pawn teardown), even with End = Completed, is a claim
//     release only: the gate is not entered (no Roster binding lookup) and the log call count is 0;
//   - a body standing at a face with no hold holds no claim and cannot lock a fixture (claim count 0; the predicate allows);
//   - an End for a pawn with no claim is a no-op;
//   - teardown during Active: the stand-in's CancelAll ends every running hold outside a slot (claims released, nothing
//     logged); at round end the same.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

namespace ObjectivesInteractPredicate
{
	FPredicateResult Evaluate(EObjectivesLegalityClass Legality, int32 ClaimCount, int32 SideCount);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesClaimReleaseOutsideSlotTest, "StealthGame.Objectives.ClaimReleaseOutsideSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesClaimReleaseOutsideSlotTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;

	// Core level.
	{
		StealthAssassinationTest::FScopedWorld World(TEXT("ObjectivesClaimReleaseOutsideSlot"));
		FCoreRig Rig;
		AStealthPawn* Tender = FCoreRig::PawnAt(World, CourtP1Team0, 0);
		AStealthPawn* Parked = FCoreRig::PawnAt(World, CourtP1Team0, 1);
		Rig.Roster.Bind(Tender, 1, 0);
		Rig.Core.OnInteractExecuted(*Tender, FCourtFixtureId(CourtP1Team0), FQueueKey(1, 1));
		const int32 Queries = Rig.Roster.ParticipantQueries;
		const EObjectivesCompletion Outcome = Rig.Core.OnInteractEnded(*Tender, FCourtFixtureId(CourtP1Team0), EObjectivesInteractEnd::Completed, FQueueKey());
		TestEqual(TEXT("an invalid key is a claim release only"), static_cast<int32>(Outcome), static_cast<int32>(EObjectivesCompletion::ClaimReleaseOnly));
		TestEqual(TEXT("the claim is released"), Rig.Core.GetClaimCount(FCourtFixtureId(CourtP1Team0)), 0);
		TestEqual(TEXT("the gate is not entered (no binding lookup)"), Rig.Roster.ParticipantQueries, Queries);
		TestEqual(TEXT("log call count 0"), Rig.Round.Entries.Num(), 0);
		TestEqual(TEXT("the site is still Live (nothing counted)"), static_cast<int32>(Rig.Core.GetSiteState(0, DefP1Team0)), static_cast<int32>(EObjectiveSiteState::Live));

		// A parked body at a face holds no claim: the fixture is not locked.
		TestEqual(TEXT("a body standing at a face holds no claim"), Rig.Core.GetClaimCount(FCourtFixtureId(CourtP1Team0)), 0);
		TestEqual(TEXT("the predicate allows a Begin there"), static_cast<int32>(ObjectivesInteractPredicate::Evaluate(EObjectivesLegalityClass::Active,
			Rig.Core.GetClaimCount(FCourtFixtureId(CourtP1Team0)), Rig.Core.GetFixtureSideCount()).Verdict), static_cast<int32>(EPredicateVerdict::Allow));

		// End for a pawn with no claim: a no-op.
		TestEqual(TEXT("an End with no claim is a no-op"), static_cast<int32>(Rig.Core.OnInteractEnded(*Parked, FCourtFixtureId(CourtP1Team0), EObjectivesInteractEnd::Cancelled, FQueueKey(2, 1))),
			static_cast<int32>(EObjectivesCompletion::ClaimReleaseOnly));
		TestEqual(TEXT("still nothing logged"), Rig.Round.Entries.Num(), 0);
	}

	// Through the stand-in: teardown during Active and at round end end every hold outside a slot.
	for (const bool bRoundEnd : { false, true })
	{
		FQueueRig Rig;
		TestTrue(TEXT("Active"), Rig.ToActive());
		AStealthPawn* Tender = Rig.Pawn(CourtP1Team0, 0, 1, 0);
		Rig.Begin(Tender, CourtP1Team0);
		Rig.Q.Tick(0.1);
		TestEqual(TEXT("one claim while holding"), Rig.Core().GetClaimCount(FCourtFixtureId(CourtP1Team0)), 1);
		const int32 Entries = Rig.Q.Log.Entries.Num();
		if (bRoundEnd)
		{
			Rig.Service->HandleRoundEnded();
		}
		Rig.Holds->CancelAll();
		const TCHAR* When = bRoundEnd ? TEXT("at round end") : TEXT("during Active");
		TestEqual(FString::Printf(TEXT("%s: the claim is released"), When), Rig.Core().GetClaimCount(FCourtFixtureId(CourtP1Team0)), 0);
		TestEqual(FString::Printf(TEXT("%s: no hold runs"), When), Rig.Holds->GetHoldCount(), 0);
		TestEqual(FString::Printf(TEXT("%s: nothing logged"), When), Rig.Q.Log.Entries.Num(), Entries);
		Rig.Run(3.5); // the orphaned HoldTimer(Complete) is a stale timer: Voided, nothing counts
		TestEqual(FString::Printf(TEXT("%s: the stale timer counts nothing"), When), Rig.Logged(ERoundLogType::ObjectiveStage), 0);
	}
	return true;
}

#endif
