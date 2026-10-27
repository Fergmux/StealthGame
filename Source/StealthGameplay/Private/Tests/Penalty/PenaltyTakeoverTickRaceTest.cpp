// Story nkp-012 (production/epics/npc-kill-penalty-informants/story-012-respawn-commit-sequence.md), AC3 / AC-NKP-42
// (Edge Cases; ADR-0019 section 4.2: decided entirely by queue key order and the factory's atomic check, Penalty adds no
// ordering code): GIVEN a strike on the takeover candidate X in the takeover's commit tick, in both key orders, WHEN the
// drain resolves, THEN
//   attack first   X is dead when the respawn slot runs: the pre-check answers NotEligible, X is excluded, a retry is
//                  scheduled at ServerNow + the next-frame offset with the same RequestSeq, and no card moves;
//   takeover first the strike is an informant kill (the Roster binding names X's participant: VictimKind Informant): the
//                  attacker's result is WrongTarget with tally + 1 and no strike, and a new T_due = max(t_lost + D, floor,
//                  now) is scheduled for the informant (Embodied -> Awaiting).
// The stub queue runs the two slots of one drain in the order of the case.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyTakeoverTickRaceTestData
{
	inline constexpr uint64 PawnX = 700;
	inline constexpr double StrikeContactSec = 0.40;

	/** P awaiting (an exile at the rig's start), its item due; Q is the Active attacker. */
	void ArrangeAwaiting(StealthPenaltyTest::FRig& Rig)
	{
		using namespace StealthPenaltyTest;
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Roster.Teams.Add(P, ERosterTeam::Team0);
		Rig.Gateway.PickPawn = FPenaltyPawnKey(PawnX);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, Rig.Clock.GetServerTimeSeconds());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyTakeoverTickRaceTest, "StealthGame.Penalty.TakeoverTickRace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyTakeoverTickRaceTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyTakeoverTickRaceTestData;

	// Attack first: Q's strike kills X (an NPC) in the earlier slot; then P's respawn slot in the same drain.
	{
		FRig Rig(nullptr, false, /*bWithRespawnGateway*/ true);
		ArrangeAwaiting(Rig);
		const uint32 Seq = Rig.Respawns.PendingFor(P)[0].RequestSeq;
		Rig.SetTime(Rig.Respawns.PendingFor(P)[0].DueServerTime);
		const FStrikeTally StrikeTally = Rig.Attack(Q, FPenaltyKill{ FPenaltyPawnKey(PawnX), Q, EVictimKind::Npc, EKillCause::Strike }, EStrikeResult::WrongTarget);
		Rig.Gateway.Ineligible.Add(FPenaltyPawnKey(PawnX)); // X has its death write: the factory's predicate refuses it
		Rig.Gateway.Picks.Add(FPenaltyRespawnPick{ EPenaltyPickStatus::Picked, FPenaltyPawnKey(PawnX), 0 }); // the LSC pre-filter ran before the kill
		const double SlotTime = Rig.Clock.GetServerTimeSeconds();
		Rig.Fire(P, /*bSameFrame*/ true);

		TestEqual(TEXT("attack first: the strike was an NPC kill for Q (strike, tally + 1)"), static_cast<int32>(StrikeTally.TallyAfter), 1);
		TestEqual(TEXT("attack first: the pre-check answered NotEligible and nothing followed"), Rig.Gateway.Sequence(), FString(TEXT("Pick PreCheck")));
		TestTrue(TEXT("attack first: X excluded for the request"), Rig.Core->FindRecord(P)->RequestExclusions.Contains(FPenaltyPawnKey(PawnX)));
		const TArray<FRecordingRespawns::FCall> Retry = Rig.Respawns.PendingFor(P);
		TestTrue(TEXT("attack first: a retry at ServerNow + the next-frame offset with the same RequestSeq"), Retry.Num() == 1
			&& FMath::IsNearlyEqual(Retry[0].DueServerTime, SlotTime + FPenaltyCore::NextFrameRetryOffsetSec(), 1e-9) && Retry[0].RequestSeq == Seq);
		TestEqual(TEXT("attack first: no card moves"), Rig.Gateway.Count(TEXT("Card")), 0);
		TestTrue(TEXT("attack first: still Picking"), Rig.Core->GetLifeState(P) == EInformantLifeState::Picking);
	}

	// Takeover first: P's respawn slot takes X; then Q's strike on X in the same drain is an informant kill.
	{
		FRig Rig(nullptr, false, true);
		ArrangeAwaiting(Rig);
		Rig.Gateway.OnApplied = [&Rig](FParticipantId Participant, FPenaltyPawnKey Pawn) { Rig.Roster.LivePawns.Add(Participant); };
		Rig.Fire(P);
		TestTrue(TEXT("takeover first: P embodied in X"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied);
		const double StrikeSlot = Rig.Clock.GetServerTimeSeconds();
		// The strike in the same tick: the Roster binding now names P, so Assassination reports VictimKind = Informant.
		Rig.Slot.NextSlot();
		FPenaltyKill Kill{ FPenaltyPawnKey(PawnX), Q, EVictimKind::Informant, EKillCause::Strike };
		Kill.VictimParticipant = P;
		Kill.VictimDeathServerTime = StrikeSlot + StrikeContactSec;
		const EPenaltyKillOutcome Outcome = Rig.Core->OnPawnKilled(Kill);
		const FStrikeTally Tally = Rig.Core->GetTallyForResult(Q, EStrikeResult::WrongTarget, FRig::Seq(77));

		TestTrue(TEXT("takeover first: no strike for Q (an informant kill)"), Outcome == EPenaltyKillOutcome::NoStrike && Rig.Core->GetStrikeCount(Q) == 0);
		TestEqual(TEXT("takeover first: WrongTarget, tally + 1"), static_cast<int32>(Tally.TallyAfter), 1);
		const double D = Rig.Core->GetLatchedParams().InformantRespawnDelaySec;
		TestTrue(TEXT("takeover first: P awaits again"), Rig.Core->GetLifeState(P) == EInformantLifeState::Awaiting);
		TestEqual(TEXT("takeover first: new T_due = max(t_lost + D, floor, now) = t_lost + D"), Rig.Core->GetDueServerTime(P).Get(0.0), StrikeSlot + D);
		TestEqual(TEXT("takeover first: one card move (the takeover's)"), Rig.Gateway.Count(TEXT("Card")), 1);
	}
	return true;
}

#endif
