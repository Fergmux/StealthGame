// Story nkp-006 (production/epics/npc-kill-penalty-informants/story-006-exile-releases-body-to-crowd.md), AC1 /
// AC-NKP-06 (PEN-R4.2; Roster CR-3, decision D42): GIVEN an exile through the REAL Roster core, WHEN ResolveVictimKind is
// called on the exiled body when the transition's event is dispatched (inside the exile's call), right after the kill and
// again 1.75 s later, THEN it answers Npc every time (never AlreadyResolved); the body keeps its handle (the same actor)
// and the participant's AppearanceId, the body is never left Retired (no binding row at all) and no pawn is created or
// destroyed (the Roster's liveness never changes, the exile's calls name the same body). Edge (QA): a rival's kill of the
// body at + 0 and at + 1.75 s resolves Npc (WrongTarget, never RivalEliminated) and is graced (no strike).
// Deviation, recorded in the story: the PIE variant (StealthGame.Penalty.PIE.ExileReleasesBody) and the card's NpcLive
// state need the Character Pool epic (no card ledger exists yet: ReleaseCard is a stand-in); this test asserts the
// Roster composition headless, and ReleaseCard is asserted called once with the body.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyRealRosterRig.h"

namespace PenaltyExileReleasesBodyTestData
{
	/** Captures ResolveVictimKind(Body) at the dispatch of the exile's state event (the transition has returned its writes). */
	struct FKindAtDispatch final : IRosterEventListener
	{
		const FRosterCore* Core = nullptr;
		FRosterPawnHandle Body;
		TArray<FRosterVictimKind> Kinds;
		virtual void OnRosterEvent(const FRosterEvent& Event) override
		{
			if (Event.Type == ERosterEventType::ParticipantStateChanged && Event.Reason == ERosterReason::StrikeLimit)
			{
				Kinds.Add(Core->ResolveVictimKind(Body));
			}
		}
	};

	/** The time after the release the AC samples (the strike clip's lockout, AttackAttemptLockoutSec's default). */
	inline double SampleAfterSec() { return StealthPenaltyTest::FRig::LockoutSec(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyExileReleasesBodyTest, "StealthGame.Penalty.ExileReleasesBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyExileReleasesBodyTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyRealRoster;
	using namespace PenaltyExileReleasesBodyTestData;
	const FRosterVictimKind Npc = FRosterVictimKind::Make(ERosterVictimKind::Npc);

	for (const bool bRivalLate : { false, true })
	{
		const TCHAR* Case = bRivalLate ? TEXT("rival at + 1.75 s") : TEXT("rival in the next slot (+ 0.25 s)");

		// Arrange: P (participant 0) at k = L - 1 on the real Roster.
		FRig Rig;
		const int32 Limit = Rig.Penalty->GetLatchedStrikeLimit();
		for (int32 Index = 0; Index < Limit - 1; ++Index)
		{
			Rig.NpcKill(0, 900 + Index);
		}
		const FRosterPawnHandle Body = HandleOf(FRig::BodyOf(0));
		const int32 AppearanceBefore = Rig.Roster->FindRecord(Rig.Ids[0])->AssignedAppearanceId;
		FKindAtDispatch Spy;
		Spy.Core = Rig.Roster.Get();
		Spy.Body = Body;
		Rig.Roster->AddListener(&Spy);

		// Act: the exiling kill.
		EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::NoStrike;
		const FStrikeTally Exiling = Rig.NpcKill(0, 990, &Outcome);
		const double ReleasedAt = Rig.Clock.GetServerTimeSeconds();
		Rig.Roster->RemoveListener(&Spy);

		// Assert: released, not retired; the same body.
		TestTrue(FString::Printf(TEXT("%s: the exile ran (bExiled)"), Case), Outcome == EPenaltyKillOutcome::StrikeAndExile && Exiling.bExiled);
		TestTrue(FString::Printf(TEXT("%s: P is Informant (StrikeLimit)"), Case), Rig.Roster->FindRecord(Rig.Ids[0])->State == EParticipantState::Informant
			&& Rig.Roster->FindRecord(Rig.Ids[0])->LastTransitionReason == ERosterReason::StrikeLimit);
		TestTrue(FString::Printf(TEXT("%s: Npc when the transition's event is dispatched"), Case), Spy.Kinds.Num() == 1 && Spy.Kinds[0] == Npc);
		TestTrue(FString::Printf(TEXT("%s: Npc right after the kill"), Case), Rig.Roster->ResolveVictimKind(Body) == Npc);
		TestNull(FString::Printf(TEXT("%s: the body has no binding row (never Retired)"), Case), Rig.Roster->FindBinding(Body));
		TestFalse(FString::Printf(TEXT("%s: P has no live pawn"), Case), Rig.Roster->FindRecord(Rig.Ids[0])->HasLivePawn());
		TestEqual(FString::Printf(TEXT("%s: the AppearanceId is unchanged"), Case), Rig.Roster->FindRecord(Rig.Ids[0])->AssignedAppearanceId, AppearanceBefore);
		TestEqual(FString::Printf(TEXT("%s: the exile's calls"), Case), FString::Join(Rig.Bodies.Calls, TEXT(" ")),
			FString(TEXT("RequestTransition ReleaseCard AdoptReleasedPawn RequestHandOver")));
		TestEqual(FString::Printf(TEXT("%s: no pawn destroyed"), Case), Rig.Liveness.Destroyed.Num(), 0);

		// 1.75 s later, still an NPC; then (case) a rival kills the body.
		if (bRivalLate)
		{
			Rig.Clock.Advance(SampleAfterSec() - 0.25, 1); // KillBody advances the last 0.25 s into the rival's slot
			TestTrue(FString::Printf(TEXT("%s: Npc 1.75 s after the release (never AlreadyResolved)"), Case),
				Rig.Roster->ResolveVictimKind(Body) == Npc);
		}
		EPenaltyKillOutcome RivalOutcome = EPenaltyKillOutcome::NoStrike;
		const FStrikeTally Rival = Rig.KillBody(2, FRig::BodyOf(0), &RivalOutcome);
		TestTrue(FString::Printf(TEXT("%s: the rival's slot is at the sampled time"), Case), FMath::IsNearlyEqual(Rig.Clock.GetServerTimeSeconds() - ReleasedAt,
			bRivalLate ? SampleAfterSec() : 0.25, 1e-6));
		TestTrue(FString::Printf(TEXT("%s: the rival's kill is an NPC kill (WrongTarget), graced"), Case), RivalOutcome == EPenaltyKillOutcome::Graced);
		TestEqual(FString::Printf(TEXT("%s: the rival's tally + 1 (WrongTarget)"), Case), static_cast<int32>(Rival.TallyAfter), 1);
		TestEqual(FString::Printf(TEXT("%s: no strike for the rival"), Case), static_cast<int32>(Rig.Penalty->GetStrikeCount(Rig.Ids[2])), 0);
		TestEqual(FString::Printf(TEXT("%s: no Roster fault"), Case), Rig.RosterFaults.GetRecords().Num(), 0);
		TestEqual(FString::Printf(TEXT("%s: no Penalty fault"), Case), Rig.PenaltyFaults.GetRecords().Num(), 0);
	}
	return true;
}

#endif
