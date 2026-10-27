// Story nkp-004 (production/epics/npc-kill-penalty-informants/story-004-event-log-rows.md), AC1 / AC-NKP-22 (PEN-R17,
// ADR-0019 section 7): each Penalty row lands once per scenario fixture in the in-memory sink, with the slot key and exactly
// its section 7 fields.
//   - exile (the L-th ungraced NPC kill, real path): one StrikeCounted{P, L, L, false}; no StrikeLimit row (Round's);
//   - informant kill (real path): one StrikeCounted with StrikeAfter unchanged and TallyAfter + 1;
//   - every NPC or informant kill by an Active participant writes a StrikeCounted; a rival kill, a whiff and a reversal
//     write none;
//   - RespawnScheduled{P, StrikeLimit, t_lost, T_due}, InformantRespawn{P, PickLatencySec = now - T_due, WideningLevel},
//     RespawnCancelled{P, Disconnect} and PawnLost{P, KillZ}: their producing paths are stories nkp-005 / 009 / 010 / 012,
//     so here each is written through the Penalty writer inside a slot and read back field by field (t_lost and T_due
//     compared exactly against the fake clock); the producing stories assert them through their real paths.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyEventLogRowsTest, "StealthGame.Penalty.EventLogRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyEventLogRowsTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;

	// Exile fixture: the L-th ungraced NPC kill.
	{
		FRig Rig;
		const int32 Limit = Rig.Core->GetLatchedStrikeLimit();
		for (int32 Index = 0; Index < Limit - 1; ++Index)
		{
			Rig.NpcKill(P, 900 + Index);
		}
		Rig.Log.Entries.Reset();
		const FStrikeTally Tally = Rig.NpcKill(P, 950);
		const FQueueKey SlotKey = Rig.Slot.Key;
		TestTrue(TEXT("exile fixture: the kill exiles"), Tally.bExiled);
		const TArray<FRoundLogEntry> Rows = Rig.Log.OfType(ERoundLogType::StrikeCounted);
		FPenaltyStrikeCountedRow Row;
		if (TestEqual(TEXT("exile fixture: one StrikeCounted"), Rows.Num(), 1) && TestTrue(TEXT("exile fixture: decodes"), PenaltyEventRows::Decode(Rows[0], Row)))
		{
			TestTrue(TEXT("exile fixture: StrikeCounted{P, L, L, false}"), Row == FPenaltyStrikeCountedRow{ P, static_cast<uint8>(Limit), static_cast<uint8>(Limit), false });
			TestTrue(TEXT("exile fixture: the slot key"), Rows[0].Key == SlotKey);
			TestEqual(TEXT("exile fixture: the slot's server time"), Rows[0].ServerTime, Rig.Clock.GetServerTimeSeconds());
			TestEqual(TEXT("exile fixture: the round epoch"), Rows[0].RoundEpoch, FixtureEpoch);
		}
		TestEqual(TEXT("exile fixture: no StrikeLimit row (Round writes it with the transition)"), Rig.Log.OfType(ERoundLogType::StrikeLimit).Num(), 0);
		TestEqual(TEXT("exile fixture: nothing else"), Rig.Log.Entries.Num(), 1);
	}

	// Informant-kill fixture: StrikeAfter unchanged, TallyAfter + 1.
	{
		FRig Rig;
		Rig.NpcKill(P, 900);
		Rig.Log.Entries.Reset();
		Rig.InformantKill(P, 901);
		const TArray<FRoundLogEntry> Rows = Rig.Log.OfType(ERoundLogType::StrikeCounted);
		FPenaltyStrikeCountedRow Row;
		TestTrue(TEXT("informant fixture: one StrikeCounted{P, 1, 2, false}"), Rows.Num() == 1 && PenaltyEventRows::Decode(Rows[0], Row)
			&& Row == FPenaltyStrikeCountedRow{ P, 1, 2, false } && Rows[0].Key == Rig.Slot.Key);
	}

	// No StrikeCounted for a rival kill, a whiff or a reversal.
	{
		FRig Rig;
		Rig.Attack(P, FPenaltyKill{ Pawn(20), P, EVictimKind::Player, EKillCause::Strike }, EStrikeResult::RivalEliminated);
		Rig.Attack(P, TOptional<FPenaltyKill>(), EStrikeResult::NoContact);
		Rig.Attack(Q, FPenaltyKill{ Pawn(21), FParticipantId(), EVictimKind::Player, EKillCause::Reversal }, EStrikeResult::Reversed);
		TestEqual(TEXT("rival kill, whiff, reversal: no row"), Rig.Log.Entries.Num(), 0);
	}

	// The four rows whose producers are later stories: each written once in a slot and read back exactly.
	{
		FRig Rig;
		FPenaltyEventLog Writer(&Rig.Log, &Rig.Slot, &Rig.Clock, &Rig.Faults);
		const double LostTime = Rig.Clock.GetServerTimeSeconds();
		const double DueTime = LostTime + 15.0;

		Rig.Slot.NextSlot();
		const FPenaltyRespawnScheduledRow Scheduled{ P, ERespawnCause::StrikeLimit, LostTime, DueTime };
		Writer.Write(Scheduled, FixtureEpoch);
		Rig.Clock.AdvanceSeconds(DueTime - LostTime + 0.5);
		Rig.Slot.NextSlot();
		const FPenaltyInformantRespawnRow Respawned{ P, Rig.Clock.GetServerTimeSeconds() - DueTime, 1 };
		Writer.Write(Respawned, FixtureEpoch);
		Rig.Slot.NextSlot();
		const FPenaltyRespawnCancelledRow Cancelled{ Q, ERespawnCancelReason::Disconnect };
		Writer.Write(Cancelled, FixtureEpoch);
		Rig.Slot.NextSlot();
		const FPenaltyPawnLostRow Lost{ P, EPenaltyPawnLostCause::KillZ };
		Writer.Write(Lost, FixtureEpoch);

		FPenaltyRespawnScheduledRow ScheduledBack;
		FPenaltyInformantRespawnRow RespawnedBack;
		FPenaltyRespawnCancelledRow CancelledBack;
		FPenaltyPawnLostRow LostBack;
		const TArray<FRoundLogEntry> S = Rig.Log.OfType(ERoundLogType::RespawnScheduled);
		const TArray<FRoundLogEntry> I = Rig.Log.OfType(ERoundLogType::InformantRespawn);
		const TArray<FRoundLogEntry> C = Rig.Log.OfType(ERoundLogType::RespawnCancelled);
		const TArray<FRoundLogEntry> L = Rig.Log.OfType(ERoundLogType::PawnLost);
		TestTrue(TEXT("RespawnScheduled{P, StrikeLimit, t_lost, T_due} once, exact"), S.Num() == 1 && PenaltyEventRows::Decode(S[0], ScheduledBack) && ScheduledBack == Scheduled
			&& ScheduledBack.LostServerTime == LostTime && ScheduledBack.DueServerTime == DueTime);
		TestTrue(TEXT("InformantRespawn{P, PickLatencySec = now - T_due, WideningLevel} once, exact"), I.Num() == 1 && PenaltyEventRows::Decode(I[0], RespawnedBack)
			&& RespawnedBack == Respawned && RespawnedBack.PickLatencySec == I[0].ServerTime - DueTime);
		TestTrue(TEXT("RespawnCancelled{Q, Disconnect} once"), C.Num() == 1 && PenaltyEventRows::Decode(C[0], CancelledBack) && CancelledBack == Cancelled);
		TestTrue(TEXT("PawnLost{P, KillZ} once"), L.Num() == 1 && PenaltyEventRows::Decode(L[0], LostBack) && LostBack == Lost);
		TestTrue(TEXT("each row carries its own slot key"), S.Num() == 1 && I.Num() == 1 && C.Num() == 1 && L.Num() == 1 && S[0].Key < I[0].Key && I[0].Key < C[0].Key
			&& C[0].Key < L[0].Key && L[0].Key == Rig.Slot.Key);
		FPenaltyStrikeCountedRow Wrong;
		TestFalse(TEXT("a row never decodes as another type"), S.Num() == 1 && PenaltyEventRows::Decode(S[0], Wrong));
		const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({});
		TestTrue(Teardown.Describe(), Teardown.IsClean());
	}
	return true;
}

#endif
