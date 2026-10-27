// Story nkp-009 (production/epics/npc-kill-penalty-informants/story-009-respawn-due-time-scheduling-and-cancellation.md),
// AC3 / AC-NKP-11 (PEN-R10; ADR-0019 section 3.3; reason values Disconnect, RoundEnded, Reset confirmed at readiness):
// GIVEN a pending respawn item, WHEN the event of the row occurs before it resolves, THEN zero BindPawn calls are made
// (the takeover is the only binder: zero TakeOverCrowdPawn calls) and:
//   disconnect      the item is cancelled (Cancel(TimerHandle)), RespawnCancelled{Disconnect} logged, Absent, t_lost kept;
//   round end       every item cancelled, one RespawnCancelled{RoundEnded} per pending item (two here), RoundEnded, the
//                   rows written inside the end handling (in the slot, not refused by the freeze);
//   reset           RespawnCancelled{Reset} per pending item, Active(0), and a leftover item of the earlier epoch that
//                   still fires on the REAL queue resolves Rejected_StaleEpoch (the resolver itself voids it as well).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Testing/StealthQueueTestRig.h"
#include "StealthPenaltyQueueAdapter.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyRespawnCancellationTestData
{
	inline constexpr double CauseAt = 100.0;
	inline constexpr double EventAt = 103.0;

	/** IPenaltySlotContext over the real queue (as UPenaltyService's FQueueSlot). */
	struct FQueueSlot final : IPenaltySlotContext
	{
		const FResolutionQueueCore* Queue = nullptr;
		virtual bool IsInSlot() const override { return Queue != nullptr && Queue->IsInSlot(); }
		virtual FQueueKey CurrentKey() const override { return Queue != nullptr ? Queue->CurrentKey() : FQueueKey(); }
	};

	/** P and Q awaiting (StrikeLimit at CauseAt), the clock moved to EventAt. */
	void ArrangeTwoPending(StealthPenaltyTest::FRig& Rig)
	{
		using namespace StealthPenaltyTest;
		Rig.SetTime(CauseAt);
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Roster.States.Add(Q, EParticipantState::Informant);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, CauseAt);
		Rig.Core->OnInformantAwaitingBody(Q, ERespawnCause::StrikeLimit, CauseAt);
		Rig.SetTime(EventAt);
		Rig.Slot.NextSlot();
	}

	TArray<FPenaltyRespawnCancelledRow> CancelledRows(const StealthPenaltyTest::FRig& Rig, ERespawnCancelReason Reason)
	{
		return Rig.Rows<FPenaltyRespawnCancelledRow>().FilterByPredicate([Reason](const FPenaltyRespawnCancelledRow& Row) { return Row.Reason == Reason; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyRespawnCancellationTest, "StealthGame.Penalty.RespawnCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyRespawnCancellationTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyRespawnCancellationTestData;

	// Row 1: a participant disconnect.
	{
		FRig Rig(nullptr, false, /*bWithRespawnGateway*/ true);
		ArrangeTwoPending(Rig);
		const uint64 Handle = Rig.Respawns.PendingFor(P)[0].Handle;
		Rig.Roster.Disconnected.Add(P);
		Rig.Core->OnConnectionChanged(P, false);
		const TArray<FPenaltyRespawnCancelledRow> Rows = CancelledRows(Rig, ERespawnCancelReason::Disconnect);

		TestTrue(TEXT("disconnect: Cancel(TimerHandle) on P's item"), Rig.Respawns.Cancelled.Contains(Handle));
		TestTrue(TEXT("disconnect: one RespawnCancelled{Disconnect} for P"), Rows.Num() == 1 && Rows[0].Participant == P);
		TestTrue(TEXT("disconnect: Absent"), Rig.Core->GetLifeState(P) == EInformantLifeState::Absent);
		TestEqual(TEXT("disconnect: t_lost kept"), Rig.Core->FindRecord(P)->LostServerTime, CauseAt);
		TestTrue(TEXT("disconnect: Q's item untouched"), Rig.Respawns.PendingFor(Q).Num() == 1 && Rig.Core->GetLifeState(Q) == EInformantLifeState::Awaiting);
		TestFalse(TEXT("disconnect: the cancelled item no longer resolves"), Rig.Core->ResolveRespawnItem(P, Rig.Respawns.For(P)[0].RequestSeq, FixtureEpoch));
		TestEqual(TEXT("disconnect: zero BindPawn (no takeover)"), Rig.Gateway.Count(TEXT("Takeover")), 0);
	}

	// Row 2: OnRosterRoundEnded with two pending items.
	{
		FRig Rig(nullptr, false, true);
		ArrangeTwoPending(Rig);
		Rig.Core->OnRosterRoundEnded();
		const TArray<FPenaltyRespawnCancelledRow> Rows = CancelledRows(Rig, ERespawnCancelReason::RoundEnded);

		TestEqual(TEXT("round end: every item cancelled"), Rig.Respawns.Pending().Num(), 0);
		TestEqual(TEXT("round end: one RespawnCancelled{RoundEnded} per pending item (two)"), Rows.Num(), 2);
		TestTrue(TEXT("round end: RoundEnded for both"), Rig.Core->GetLifeState(P) == EInformantLifeState::RoundEnded && Rig.Core->GetLifeState(Q) == EInformantLifeState::RoundEnded);
		bool bAllInSlot = true;
		for (int32 Index = 0; Index < Rig.Log.Entries.Num(); ++Index)
		{
			bAllInSlot &= Rig.Log.Entries[Index].Type != ERoundLogType::RespawnCancelled || Rig.Log.InSlotAtWrite[Index];
		}
		TestTrue(TEXT("round end: the rows were written inside the end handling (in the slot)"), bAllInSlot);
		Rig.Core->OnRosterRoundEnded();
		TestEqual(TEXT("round end: idempotent (no second row)"), CancelledRows(Rig, ERespawnCancelReason::RoundEnded).Num(), 2);
		TestEqual(TEXT("round end: zero BindPawn"), Rig.Gateway.Count(TEXT("Takeover")), 0);
		const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({});
		TestTrue(Teardown.Describe(), Teardown.IsClean());
	}

	// Row 3: OnRosterReset with a pending item.
	{
		FRig Rig(nullptr, false, true);
		ArrangeTwoPending(Rig);
		const uint32 OldSeq = Rig.Respawns.PendingFor(P)[0].RequestSeq;
		Rig.Core->OnRosterReset(FixtureEpoch + 1);
		TestEqual(TEXT("reset: one RespawnCancelled{Reset} per pending item (two)"), CancelledRows(Rig, ERespawnCancelReason::Reset).Num(), 2);
		TestTrue(TEXT("reset: Active(0)"), Rig.Core->GetLifeState(P) == EInformantLifeState::Active && Rig.Core->GetStrikeCount(P) == 0);
		TestEqual(TEXT("reset: no item left"), Rig.Respawns.Pending().Num(), 0);
		TestFalse(TEXT("reset: a leftover item of the earlier epoch voids in the resolver"), Rig.Core->ResolveRespawnItem(P, OldSeq, FixtureEpoch));
		TestEqual(TEXT("reset: zero BindPawn"), Rig.Gateway.Count(TEXT("Takeover")), 0);
	}

	// Row 3 on the REAL queue: an injected leftover item of the earlier epoch fires after the reset: Rejected_StaleEpoch.
	{
		FStealthQueueTestRig Q2;
		FFakeRoster Roster;
		Roster.Epoch = Q2.Epoch.Current.RoundEpoch;
		Roster.States.Add(P, EParticipantState::Informant);
		FFakeTuning Tuning;
		FQueueSlot Slot;
		Slot.Queue = Q2.Queue.Get();
		FRecordingLog Log;
		Log.Slot = &Slot;
		FStealthRecordingFaultSink Faults;
		FFakeRespawnGateway Gateway;
		FPenaltyRespawnQueueAdapter Adapter(Q2.Queue.Get(), [&Q2]() { return Q2.Epoch.Current.MatchId; });
		FPenaltyCoreDeps Deps;
		Deps.Clock = &Q2.Clock;
		Deps.Roster = &Roster;
		Deps.Tuning = &Tuning;
		Deps.Slot = &Slot;
		Deps.Log = &Log;
		Deps.Timers = &Adapter;
		Deps.Respawn = &Gateway;
		Deps.Faults = &Faults;
		FPenaltyCore Core(Deps);
		Adapter.SetCore(&Core);
		TestTrue(TEXT("real queue: the InformantRespawn resolver registers"), Q2.Queue->RegisterTimerResolver(ETimerKind::InformantRespawn, &Adapter));

		const double Now = Q2.Clock.GetServerTimeSeconds();
		const uint32 OldEpoch = Q2.Epoch.Current.RoundEpoch;
		Adapter.ScheduleRespawnItem(Now + 1.0, FPenaltyRespawnItem{ P, 1, OldEpoch, ERespawnCause::StrikeLimit });
		Q2.Epoch.Current.RoundEpoch = OldEpoch + 1; // the Roster's ResetAll: a new RoundEpoch
		Roster.Epoch = OldEpoch + 1;
		Core.OnRosterReset(OldEpoch + 1);
		Q2.Tick(2.0);
		const TArray<FQueueSlotRecord>& Records = Q2.Queue->GetLastDrainRecords();
		TestTrue(TEXT("real queue: the leftover item resolves Rejected_StaleEpoch"), Records.Num() == 1 && Records[0].Outcome == EQueueSlotOutcome::Rejected_StaleEpoch);
		TestEqual(TEXT("real queue: zero BindPawn"), Gateway.Count(TEXT("Takeover")), 0);
		Q2.Queue->UnregisterTimerResolver(ETimerKind::InformantRespawn);
	}
	return true;
}

#endif
