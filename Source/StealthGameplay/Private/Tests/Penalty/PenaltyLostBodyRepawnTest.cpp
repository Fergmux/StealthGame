// Story nkp-010 (production/epics/npc-kill-penalty-informants/story-010-absent-path-readmission-and-lost-body.md), AC2 /
// AC-NKP-28 (PEN-R11; ADR-0019 section 3.3 "Embodied -> Picking(0)"): GIVEN an Embodied informant whose body hits KillZ at
// 130.0, WHEN OnInformantPawnLost(P) is called, THEN T_due = 130.0 (D = 0), the state is Picking(0) and the fake LSC
// receives PickRespawnPawn in that same call with a FRESH RespawnRequestSeq, PEN-R9 runs (here to Embodied with a second
// pick answering a pawn), strike and tally are unchanged, and one PawnLost{P, KillZ} row is written. Edges: a second loss
// report is idempotent; a pawn lost while Disconnected makes P Absent instead (no pick).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyLostBodyRepawnTestData
{
	inline constexpr double LostAt = 130.0;

	/** A rig starting at 50.0, so the embodiment precedes the loss at 130.0. */
	inline constexpr double EarlyStart = 50.0;

	/** P embodied (an exile at 60.0 committed at 75.0), with a strike and a tally of its own to check they are kept. */
	void ArrangeEmbodied(StealthPenaltyTest::FRig& Rig)
	{
		using namespace StealthPenaltyTest;
		Rig.NpcKill(P, 900); // k = 1, t = 1 while Active
		Rig.SetTime(60.0);
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Roster.Teams.Add(P, ERosterTeam::Team0);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, 60.0);
		Rig.Fire(P);
		Rig.Roster.LivePawns.Add(P);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyLostBodyRepawnTest, "StealthGame.Penalty.LostBodyRepawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyLostBodyRepawnTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyLostBodyRepawnTestData;

	{
		// Arrange.
		FRig Rig(nullptr, false, /*bWithRespawnGateway*/ true, EarlyStart);
		ArrangeEmbodied(Rig);
		TestTrue(TEXT("fixture: embodied"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied);
		const uint32 FirstSeq = Rig.Gateway.Named(TEXT("Pick")).Last().RequestSeq;
		const uint8 K = Rig.Core->GetStrikeCount(P);
		const uint8 T = Rig.Core->GetWrongTargetTally(P);
		const int32 PicksBefore = Rig.Gateway.Count(TEXT("Pick"));

		// Act: the body hits KillZ at 130.0 (the pick in this slot answers None, so the Picking state is observable).
		Rig.SetTime(LostAt);
		Rig.Roster.LivePawns.Remove(P);
		Rig.Gateway.Picks.Add(FPenaltyRespawnPick());
		++Rig.Gateway.Frame;
		Rig.Slot.NextSlot();
		Rig.Core->OnInformantPawnLost(P, EPenaltyPawnLostCause::KillZ);

		// Assert.
		TestEqual(TEXT("T_due = 130.0 (D = 0)"), Rig.Core->GetDueServerTime(P).Get(0.0), LostAt);
		TestTrue(TEXT("Picking(0)"), Rig.Core->GetLifeState(P) == EInformantLifeState::Picking && Rig.Core->FindRecord(P)->LastWideningLevel == 0);
		TestEqual(TEXT("the fake LSC receives PickRespawnPawn in that same call"), Rig.Gateway.Count(TEXT("Pick")), PicksBefore + 1);
		const uint32 LostSeq = Rig.Gateway.Named(TEXT("Pick")).Last().RequestSeq;
		TestTrue(TEXT("with a fresh RespawnRequestSeq"), LostSeq != 0 && LostSeq != FirstSeq);
		const TArray<FPenaltyPawnLostRow> Rows = Rig.Rows<FPenaltyPawnLostRow>();
		TestTrue(TEXT("one PawnLost{P, KillZ} row"), Rows.Num() == 1 && Rows[0].Participant == P && Rows[0].Cause == EPenaltyPawnLostCause::KillZ);
		TestTrue(TEXT("strike and tally unchanged"), Rig.Core->GetStrikeCount(P) == K && Rig.Core->GetWrongTargetTally(P) == T);

		// A second loss report is idempotent (P has no body: nothing).
		Rig.Core->OnInformantPawnLost(P, EPenaltyPawnLostCause::KillZ);
		TestEqual(TEXT("a second report: no second row"), Rig.Rows<FPenaltyPawnLostRow>().Num(), 1);
		TestEqual(TEXT("a second report: no second pick"), Rig.Gateway.Count(TEXT("Pick")), PicksBefore + 1);

		// PEN-R9 runs: the regular retry picks a pawn and the takeover commits (Embodied again), the same request.
		TestTrue(TEXT("the retry fires"), Rig.Fire(P));
		TestTrue(TEXT("PEN-R9: Embodied again through the same commit"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied);
		TestEqual(TEXT("PEN-R9: the retry reused the request's sequence"), Rig.Gateway.Named(TEXT("Pick")).Last().RequestSeq, LostSeq);
		TestEqual(TEXT("the respawn row's cause history: no RespawnScheduled for a lost body (PawnLost is its row)"),
			Rig.Rows<FPenaltyRespawnScheduledRow>().FilterByPredicate([](const FPenaltyRespawnScheduledRow& R) { return R.Cause == ERespawnCause::PawnLost; }).Num(), 0);
	}

	// Edge: the pawn is lost while Disconnected: Absent instead, no pick.
	{
		FRig Rig(nullptr, false, true, EarlyStart);
		ArrangeEmbodied(Rig);
		const int32 PicksBefore = Rig.Gateway.Count(TEXT("Pick"));
		Rig.SetTime(LostAt);
		Rig.Roster.LivePawns.Remove(P);
		Rig.Roster.Disconnected.Add(P);
		Rig.Core->OnInformantPawnLost(P, EPenaltyPawnLostCause::KillZ);
		TestTrue(TEXT("lost while Disconnected: Absent"), Rig.Core->GetLifeState(P) == EInformantLifeState::Absent);
		TestEqual(TEXT("lost while Disconnected: no pick"), Rig.Gateway.Count(TEXT("Pick")), PicksBefore);
		TestEqual(TEXT("lost while Disconnected: t_lost = 130.0 with D = 0 kept for the readmission"), Rig.Core->FindRecord(P)->RespawnDelaySec, 0.0);
	}
	return true;
}

#endif
