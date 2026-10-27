// Story nkp-012 (production/epics/npc-kill-penalty-informants/story-012-respawn-commit-sequence.md), AC2 / AC-NKP-33
// (PEN-R9, Edge Cases; ADR-0019 section 4.2): GIVEN two same-team informants P1 and P2 due in one drain of the REAL queue
// and a fake LSC with a single eligible pawn X and the one-pick-per-frame guard, WHEN both resolve, THEN the lower key
// completes the full commit and becomes Embodied; the other gets Deferred and is rescheduled for the next frame
// (ServerNow + the next-frame offset) with the same RequestSeq; at that retry X is Bound, so the pick answers None and the
// next retry is EmergenceRetrySec later; TakeOverCrowdPawn is called once, for X; the same pawn is never taken twice.
// Three key orders: P1 due first, P2 due first (the keys swapped), and equal due times (the queue's TieRank decides).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Testing/StealthQueueTestRig.h"
#include "StealthPenaltyQueueAdapter.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltySameTeamDueOneDrainTestData
{
	inline constexpr uint64 PawnX = 700;
	/** The second due time inside the same drain (both are stamped: DueTime <= DrainStart + DueEpsilonSec). */
	inline constexpr double InsideTheDrainSec = 0.0005;

	/** IPenaltySlotContext over the real queue. */
	struct FQueueSlot final : IPenaltySlotContext
	{
		const FResolutionQueueCore* Queue = nullptr;
		virtual bool IsInSlot() const override { return Queue != nullptr && Queue->IsInSlot(); }
		virtual FQueueKey CurrentKey() const override { return Queue != nullptr ? Queue->CurrentKey() : FQueueKey(); }
	};

	/** Records every schedule (participant, due time, seq) and forwards to the queue adapter. */
	struct FRecordingTimers final : IPenaltyRespawnTimers
	{
		IPenaltyRespawnTimers* Inner = nullptr;
		const IStealthClock* Clock = nullptr;
		struct FCall
		{
			FParticipantId Participant;
			double DueServerTime = 0.0;
			double ScheduledAt = 0.0;
			uint32 RequestSeq = 0;
		};
		TArray<FCall> Calls;
		virtual uint64 ScheduleRespawnItem(double DueServerTime, const FPenaltyRespawnItem& Item) override
		{
			Calls.Add({ Item.Participant, DueServerTime, Clock->GetServerTimeSeconds(), Item.RequestSeq });
			return Inner->ScheduleRespawnItem(DueServerTime, Item);
		}
		virtual void CancelRespawnItem(uint64 Handle) override { Inner->CancelRespawnItem(Handle); }
	};

	struct FCase
	{
		const TCHAR* Name;
		double P1Offset;
		double P2Offset;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltySameTeamDueOneDrainTest, "StealthGame.Penalty.SameTeamDueOneDrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltySameTeamDueOneDrainTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltySameTeamDueOneDrainTestData;

	const FCase Cases[] = {
		{ TEXT("P1 due first"), 0.0, InsideTheDrainSec },
		{ TEXT("P2 due first (keys swapped)"), InsideTheDrainSec, 0.0 },
		{ TEXT("equal due times (TieRank)"), 0.0, 0.0 },
	};
	for (const FCase& Case : Cases)
	{
		// Arrange: the real queue, two same-team informants, one eligible pawn X.
		FStealthQueueTestRig Q2;
		FFakeRoster Roster;
		Roster.Epoch = Q2.Epoch.Current.RoundEpoch;
		for (const FParticipantId Id : { P, Q })
		{
			Roster.States.Add(Id, EParticipantState::Informant);
			Roster.Teams.Add(Id, ERosterTeam::Team0);
		}
		FFakeTuning Tuning;
		FQueueSlot Slot;
		Slot.Queue = Q2.Queue.Get();
		FRecordingLog Log;
		Log.Slot = &Slot;
		FStealthRecordingFaultSink Faults;
		FFakeRespawnGateway Gateway;
		Gateway.PickPawn = FPenaltyPawnKey(PawnX);
		Gateway.OnApplied = [&Gateway, &Roster](FParticipantId Participant, FPenaltyPawnKey Pawn)
		{
			Gateway.Ineligible.Add(Pawn); // X is Bound from here on (Crowd's Bound reason; LSC "claimed this tick")
			Roster.LivePawns.Add(Participant);
		};
		FPenaltyRespawnQueueAdapter Adapter(Q2.Queue.Get(), [&Q2]() { return Q2.Epoch.Current.MatchId; });
		FRecordingTimers Timers;
		Timers.Inner = &Adapter;
		Timers.Clock = &Q2.Clock;
		FPenaltyCoreDeps Deps;
		Deps.Clock = &Q2.Clock;
		Deps.Roster = &Roster;
		Deps.Tuning = &Tuning;
		Deps.Slot = &Slot;
		Deps.Log = &Log;
		Deps.Timers = &Timers;
		Deps.Respawn = &Gateway;
		Deps.Faults = &Faults;
		FPenaltyCore Core(Deps);
		Adapter.SetCore(&Core);
		Q2.Queue->RegisterTimerResolver(ETimerKind::InformantRespawn, &Adapter);

		const double LostAt = Q2.Clock.GetServerTimeSeconds();
		Core.OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, LostAt + Case.P1Offset);
		Core.OnInformantAwaitingBody(Q, ERespawnCause::StrikeLimit, LostAt + Case.P2Offset);
		const double D = Core.GetLatchedParams().InformantRespawnDelaySec;

		// Act: the drain where both are due (frame 1), the next frame (frame 2), then a frame past the regular retry.
		Gateway.Frame = 1;
		Q2.Tick(D + 0.001);
		const FParticipantId First = Core.GetLifeState(P) == EInformantLifeState::Embodied ? P : Q;
		const FParticipantId Second = First == P ? Q : P;
		Gateway.Frame = 2;
		Q2.Tick();
		Gateway.Frame = 3;
		Q2.Tick(Core.GetLatchedParams().EmergenceRetrySec + 0.01);

		// Assert.
		const FString Name(Case.Name);
		if (Case.P1Offset != Case.P2Offset)
		{
			TestTrue(Name + TEXT(": the lower key (the earlier due time) takes X"), First == (Case.P1Offset < Case.P2Offset ? P : Q));
		}
		TestTrue(Name + TEXT(": the first completes the full commit (Embodied)"), Core.GetLifeState(First) == EInformantLifeState::Embodied);
		TestTrue(Name + TEXT(": the second keeps waiting (Picking)"), Core.GetLifeState(Second) == EInformantLifeState::Picking);
		const TArray<FFakeRespawnGateway::FCall> Takeovers = Gateway.Named(TEXT("Takeover"));
		TestTrue(Name + TEXT(": TakeOverCrowdPawn once, for X, for the first"), Takeovers.Num() == 1 && Takeovers[0].Pawn == FPenaltyPawnKey(PawnX) && Takeovers[0].Participant == First);
		const TArray<FFakeRespawnGateway::FCall> Picks = Gateway.Named(TEXT("Pick"));
		const TArray<FFakeRespawnGateway::FCall> SecondPicks = Picks.FilterByPredicate([&Core, Second](const FFakeRespawnGateway::FCall& C)
		{
			return C.RequestSeq == Core.FindRecord(Second)->RespawnRequestSeq;
		});
		TestTrue(Name + TEXT(": the second's picks: Deferred in frame 1, None in frame 2, None again after EmergenceRetrySec"),
			SecondPicks.Num() == 3 && SecondPicks[0].Frame == 1 && SecondPicks[1].Frame == 2 && SecondPicks[2].Frame == 3);
		const TArray<FRecordingTimers::FCall> SecondRetries = Timers.Calls.FilterByPredicate([Second](const FRecordingTimers::FCall& C) { return C.Participant == Second; });
		if (TestTrue(Name + TEXT(": the second was scheduled, deferred, then retried regularly"), SecondRetries.Num() >= 3))
		{
			TestTrue(Name + TEXT(": Deferred -> ServerNow + the next-frame offset, the same RequestSeq"),
				FMath::IsNearlyEqual(SecondRetries[1].DueServerTime, SecondRetries[1].ScheduledAt + FPenaltyCore::NextFrameRetryOffsetSec(), 1e-9)
				&& SecondRetries[1].RequestSeq == SecondRetries[0].RequestSeq);
			TestTrue(Name + TEXT(": None (X Bound) -> ServerNow + EmergenceRetrySec, the same RequestSeq"),
				FMath::IsNearlyEqual(SecondRetries[2].DueServerTime, SecondRetries[2].ScheduledAt + Core.GetLatchedParams().EmergenceRetrySec, 1e-9)
				&& SecondRetries[2].RequestSeq == SecondRetries[0].RequestSeq);
		}
		TestEqual(Name + TEXT(": one card move"), Gateway.Count(TEXT("Card")), 1);
		Q2.Queue->UnregisterTimerResolver(ETimerKind::InformantRespawn);
	}
	return true;
}

#endif
