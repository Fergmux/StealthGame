// Story rmf-040 (production/epics/round-match-framework/story-040-disconnected-at-start-spawned.md), GDD AC 9 / R6, R17, F4:
//   - a participant Disconnected at P6 (fake clock) still gets a pawn: it exists, is bound (spawned for its participant
//     under the round epoch), unpossessed and Ai-driven, and not net-relevant to its connection (closed spawn gate);
//   - with DisconnectedAt before ActivePhaseStart the grace expires at ActivePhaseStart + G_eff;
//   - with DisconnectedAt = ActivePhaseStart + 10 it expires at ActivePhaseStart + 10 + G_eff.
// The record's GraceExpiry is asserted, and the TimerFired(GraceExpiry) item scheduled on the real queue (fake clock) is
// observed firing at exactly that server time (not one frame before). G_eff comes from the fixture (DisconnectGraceSec
// 30, RejoinWindowSec 120: min(30, 120 - 30) = 30). Decision (story Open item): the grace record type and its
// creation entry are created here (FRoundGraceTable, FRoundMatchCore::OnParticipantConnectionChanged / Active entry);
// stories rmf-060..071 reuse them.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundFormulas.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "StealthRoundExitBTestRig.h"

namespace DisconnectedAtStartSpawnedTestData
{
	/** A round queue on the rig's own fake clock (the queue rig supplies seeds, epoch, log and fault sink). */
	struct FQueueOnRigClock
	{
		FStealthQueueTestRig Support;
		TUniquePtr<FResolutionQueueCore> Queue;

		explicit FQueueOnRigClock(FRoundExitBRig& Rig)
		{
			Support.Epoch.Current = FEpochToken(0, RoundTestFixtures::FixtureRoundEpoch);
			FResolutionQueueDeps Deps;
			Deps.Clock = &Rig.Clock;
			Deps.SubSeeds = &Support.Seeds;
			Deps.Epoch = &Support.Epoch;
			Deps.Log = Rig.Core->GetRoundLogWriter();
			Deps.FaultSink = &Support.Faults;
			Queue = MakeUnique<FResolutionQueueCore>(Deps);
			Queue->RegisterTimerResolver(ETimerKind::GraceExpiry, Rig.Core->GetGraceResolver());
			Rig.Core->SetRoundQueue(Queue.Get());
		}
	};

	FRoundRuleParams GraceParams()
	{
		FRoundRuleParams Params = FRoundExitBRig::DefaultParams();
		Params.DisconnectGraceSec = RoundTestFixtures::GraceDisconnectSec;
		Params.RejoinWindowSec = RoundTestFixtures::GraceRejoinWindowSec;
		return Params;
	}

	/** Frames until Time (one tick each, the last landing exactly on Time), draining the queue after the Round tick. */
	void RunUntil(FRoundExitBRig& Rig, FResolutionQueueCore& Queue, double Time)
	{
		while (Rig.Clock.GetServerTimeSeconds() < Time)
		{
			const double Step = FMath::Min(RoundTestFixtures::RigFrameSec, Time - Rig.Clock.GetServerTimeSeconds());
			Rig.Frame(Step);
			Queue.Drain();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundDisconnectedAtStartSpawnedTest, "StealthGame.Round.DisconnectedAtStartSpawned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundDisconnectedAtStartSpawnedTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace DisconnectedAtStartSpawnedTestData;
	const FParticipantId Away(FRoundExitBRig::HostIdValue + 3);
	const double GEff = StealthRoundFormulas::EffectiveGraceSec(GraceDisconnectSec, GraceRejoinWindowSec);
	TestEqual(TEXT("F4 G_eff at the fixture"), GEff, GraceExpectedEffectiveSec);

	// --- Case 1: Disconnected before Active (a load timeout during Preparing) -------------------------------------------
	{
		FRoundExitBRig Rig(GraceParams());
		FQueueOnRigClock Queue(Rig);
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.Fakes.Roster.SetAllStates(EParticipantState::Active);
		Rig.EnterPreparing();
		Rig.Clock.AdvanceSeconds(GraceDisconnectedBeforeStartSec);
		Rig.Fakes.Session.ConnectionStates[Away.Value] = ERoundConnectionState::Disconnected;
		Rig.Core->OnParticipantConnectionChanged(Away, ERoundConnectionState::Disconnected, false, Rig.Clock.GetServerTimeSeconds());
		TestEqual(TEXT("a disconnect in Preparing creates no record yet"), Rig.Core->GetGraceTable().Num(), 0);
		Rig.RunPreparingToActive();
		TestTrue(TEXT("case 1: Active"), Rig.Core->GetPhase() == ERoundPhase::Active);
		const double ActivePhaseStart = Rig.Core->GetActivePhaseStart();

		const FRoundFakeStartSpawner::FSpawn* Spawn = Rig.Fakes.Spawner.FindSpawn(Away);
		if (TestNotNull(TEXT("the Disconnected participant's pawn exists"), Spawn))
		{
			TestTrue(TEXT("bound: spawned for its participant under the round epoch"), Spawn->Entry.Participant == Away && Spawn->Expected.RoundEpoch == FixtureRoundEpoch);
			TestTrue(TEXT("unpossessed and Ai-driven"), Spawn->Entry.Control == ERoundSpawnControl::Ai);
			TestTrue(TEXT("not net-relevant to its connection (gate closed)"), Spawn->Entry.bGateClosed && Rig.Core->GetPreparing().IsGateClosed(Away));
		}
		const FRoundGraceRecord* Record = Rig.Core->GetGraceTable().Find(Away);
		if (TestNotNull(TEXT("the grace record was created at Active entry"), Record))
		{
			TestEqual(TEXT("DisconnectedAt before ActivePhaseStart: expiry = ActivePhaseStart + G_eff"), Record->GraceExpiry, ActivePhaseStart + GEff);
			TestTrue(TEXT("the record carries the round epoch"), Record->ExpectedEpoch.RoundEpoch == FixtureRoundEpoch);
		}
		TestEqual(TEXT("one TimerFired(GraceExpiry) is scheduled"), Queue.Queue->GetPendingTimerCount(), 1);

		// The timer fires at GraceExpiry, not before: the inline Disconnect transition is the observable effect.
		Rig.Fakes.Roster.Script(TEXT("RequestTransition"), static_cast<int64>(ERosterResult::Applied));
		Rig.Fakes.Roster.Script(TEXT("ReleaseToCrowd"), static_cast<int64>(ERosterResult::Applied)); // R17.4 (story rmf-060)
		RunUntil(Rig, *Queue.Queue, ActivePhaseStart + GEff - RigFrameSec);
		TestEqual(TEXT("no conversion before GraceExpiry"), Rig.CountSince(0, TEXT("RequestTransition")), 0);
		RunUntil(Rig, *Queue.Queue, ActivePhaseStart + GEff);
		TestEqual(TEXT("the Disconnect transition is requested at GraceExpiry"), Rig.CountSince(0, TEXT("RequestTransition")), 1);
		const FRoundFakeCall* Transition = Rig.Fakes.Roster.OwnCalls.FindByPredicate([](const FRoundFakeCall& C) { return C.Call == FName(TEXT("RequestTransition")); });
		TestTrue(TEXT("the transition is Informant for Disconnect, guarded by epoch"), Transition != nullptr && Transition->Args[0] == Away.Value
			&& Transition->Args[1] == static_cast<int64>(EParticipantState::Informant) && Transition->Args[2] == static_cast<int64>(ERosterReason::Disconnect)
			&& Transition->Args[3] == FixtureRoundEpoch);
		TestTrue(TEXT("case 1: no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}

	// --- Case 2: DisconnectedAt = ActivePhaseStart + 10 ------------------------------------------------------------------
	{
		FRoundExitBRig Rig(GraceParams());
		FQueueOnRigClock Queue(Rig);
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.Fakes.Roster.SetAllStates(EParticipantState::Active); // the disconnect policy converts Active members (story rmf-060)
		Rig.EnterPreparing();
		Rig.RunPreparingToActive();
		const double ActivePhaseStart = Rig.Core->GetActivePhaseStart();
		RunUntil(Rig, *Queue.Queue, ActivePhaseStart + GraceDisconnectAfterStartSec);
		Rig.Fakes.Session.ConnectionStates[Away.Value] = ERoundConnectionState::Disconnected;
		Rig.Core->OnParticipantConnectionChanged(Away, ERoundConnectionState::Disconnected, false, Rig.Clock.GetServerTimeSeconds());
		const FRoundGraceRecord* Record = Rig.Core->GetGraceTable().Find(Away);
		if (TestNotNull(TEXT("case 2: the record was created at the disconnect"), Record))
		{
			TestEqual(TEXT("DisconnectedAt = ActivePhaseStart + 10: expiry = ActivePhaseStart + 10 + G_eff"), Record->GraceExpiry,
				ActivePhaseStart + GraceDisconnectAfterStartSec + GEff);
		}
		Rig.Fakes.Roster.Script(TEXT("RequestTransition"), static_cast<int64>(ERosterResult::Applied));
		Rig.Fakes.Roster.Script(TEXT("ReleaseToCrowd"), static_cast<int64>(ERosterResult::Applied)); // R17.4 (story rmf-060)
		RunUntil(Rig, *Queue.Queue, ActivePhaseStart + GraceDisconnectAfterStartSec + GEff - RigFrameSec);
		TestEqual(TEXT("case 2: no conversion before GraceExpiry"), Rig.CountSince(0, TEXT("RequestTransition")), 0);
		RunUntil(Rig, *Queue.Queue, ActivePhaseStart + GraceDisconnectAfterStartSec + GEff);
		TestEqual(TEXT("case 2: converted at GraceExpiry"), Rig.CountSince(0, TEXT("RequestTransition")), 1);
		TestTrue(TEXT("case 2: no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
