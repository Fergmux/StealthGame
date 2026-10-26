// Story rmf-001 (production/epics/round-match-framework/story-001-test-seam-fake-clock.md):
// GDD test seam 1. A fake IClock (IStealthClock, story tdc-007) injected into the resolution queue supplies
// both server time and ServerTickIndex; only the test advances them, and the queue reads no wall clock.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthQueueTestRig.h"

namespace FakeClockSeamTestData
{
	constexpr double StartSeconds = 50.0;
	constexpr uint64 StartTick = 600;
	constexpr double TimerLeadSeconds = 0.5;
	constexpr int32 BusyWorkIterations = 200000;

	/** Unrelated work whose result is used, so real time passes without a wall-clock wait. */
	uint32 DoUnrelatedWork()
	{
		uint32 Hash = 2166136261u;
		for (int32 I = 0; I < BusyWorkIterations; ++I)
		{
			Hash = (Hash ^ static_cast<uint32>(I)) * 16777619u;
		}
		return Hash;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundFakeClockSeamTest, "StealthGame.Round.FakeClockSeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundFakeClockSeamTest::RunTest(const FString& Parameters)
{
	using namespace FakeClockSeamTestData;

	// Arrange: the queue consumes the fake clock by constructor injection (FStealthQueueTestRig).
	FStealthQueueTestRig Rig;
	Rig.Clock.Advance(StartSeconds, StartTick);
	FStealthStubResolver Stub;
	Rig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Stub);
	FStealthStubResolver TimerStub;
	Rig.Queue->RegisterTimerResolver(ETimerKind::GraceExpiry, &TimerStub);

	// Act 1: read twice with real time passing in between (no advance).
	const IStealthClock& Clock = Rig.Clock;
	const double SecondsA = Clock.GetServerTimeSeconds();
	const uint64 TickA = Clock.GetServerTickIndex();
	const uint32 Work = DoUnrelatedWork();
	const double SecondsB = Clock.GetServerTimeSeconds();
	const uint64 TickB = Clock.GetServerTickIndex();
	TestNotEqual(TEXT("unrelated work ran"), Work, 0u);
	TestTrue(TEXT("server time identical across two reads with no advance"), SecondsA == SecondsB && SecondsA == StartSeconds);
	TestTrue(TEXT("ServerTickIndex identical across two reads with no advance"), TickA == TickB && TickA == StartTick);

	// The queue stamps from the injected tick: two drains with no advance stamp the same tick.
	Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 1));
	Rig.Queue->Drain();
	DoUnrelatedWork();
	Rig.Queue->Submit(FMailboxId::ServerLocal(), Rig.MakeItem(EQueueItemType::PawnCommand, 2));
	Rig.Queue->Drain();
	if (TestEqual(TEXT("both items resolved"), Stub.Resolved.Num(), 2))
	{
		TestEqual(TEXT("first key uses the injected tick"), Stub.Resolved[0].Key.ServerTickIndex, StartTick);
		TestEqual(TEXT("second key uses the same tick (nothing advanced it)"), Stub.Resolved[1].Key.ServerTickIndex, StartTick);
	}

	// Act 2: a timer due 0.5 s ahead does not fire on real time, only when the test advances server time.
	Rig.Queue->ScheduleAt(StartSeconds + TimerLeadSeconds, Rig.MakeTimer(ETimerKind::GraceExpiry, 9));
	DoUnrelatedWork();
	Rig.Queue->Drain();
	TestEqual(TEXT("timer not due while only real time passes"), TimerStub.Resolved.Num(), 0);

	// Edge: a tick advance without a time advance does not make it due either.
	Rig.Clock.AdvanceTicks(5);
	Rig.Queue->Drain();
	TestEqual(TEXT("tick advance alone leaves the timer pending"), TimerStub.Resolved.Num(), 0);
	TestEqual(TEXT("tick moved by exactly 5"), Clock.GetServerTickIndex(), StartTick + 5);
	TestTrue(TEXT("time unchanged by a tick-only advance"), Clock.GetServerTimeSeconds() == StartSeconds);

	// Edge: a time advance without a tick advance makes it due, stamped on the unchanged tick.
	Rig.Clock.AdvanceSeconds(TimerLeadSeconds);
	Rig.Queue->Drain();
	if (TestEqual(TEXT("time advance alone fires the timer"), TimerStub.Resolved.Num(), 1))
	{
		TestEqual(TEXT("the timer is stamped on the test's tick"), TimerStub.Resolved[0].Key.ServerTickIndex, StartTick + 5);
	}

	// Act 3: Advance(dt, ticks) moves both by exactly the amounts.
	Rig.Clock.Advance(0.25, 3);
	TestTrue(TEXT("Advance moved time by exactly dt"), Clock.GetServerTimeSeconds() == StartSeconds + TimerLeadSeconds + 0.25);
	TestEqual(TEXT("Advance moved the tick by exactly the count"), Clock.GetServerTickIndex(), StartTick + 8);

	const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({});
	TestTrue(Teardown.Describe(), Teardown.IsClean());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
