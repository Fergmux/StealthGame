// Story rmf-051 (production/epics/round-match-framework/story-051-no-timer-ends-active-round.md), GDD AC 47 / R16 (4):
// with no items, a fake clock advanced 24 h in Active (one jump, one drain, never a tick loop) leaves the phase Active,
// no outcome, no WinLatched, and every pending timer in the due-time heap of kind GraceExpiry or PawnLossRetry (a subset,
// which covers the empty heap). A second round with one disconnect shows the only Active-phase timer Round schedules is
// the grace timer (the read-only heap inspection is FResolutionQueueCore::GetPendingTimerKinds).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

namespace NoTimerEndsActiveRoundTestData
{
	bool OnlyContestTimers(const FResolutionQueueCore& Queue)
	{
		TArray<ETimerKind> Kinds;
		Queue.GetPendingTimerKinds(Kinds);
		for (const ETimerKind Kind : Kinds)
		{
			if (Kind != ETimerKind::GraceExpiry && Kind != ETimerKind::PawnLossRetry)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundNoTimerEndsActiveRoundTest, "StealthGame.Round.NoTimerEndsActiveRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundNoTimerEndsActiveRoundTest::RunTest(const FString& Parameters)
{
	using namespace NoTimerEndsActiveRoundTestData;
	{
		// Arrange: Active through story rmf-042's entry, nothing submitted.
		FRoundEndRig R;
		R.ToActive(2, 2);
		TestTrue(TEXT("Active entry: only grace / pawn-loss timers pending"), OnlyContestTimers(*R.Queue));

		// Act: one jump of 24 h and one drain.
		R.Frame(RoundTestFixtures::NoTimerJumpSec);

		// Assert.
		TestTrue(TEXT("24 h later: phase Active"), R.Rig.Core->GetPhase() == ERoundPhase::Active);
		TestFalse(TEXT("24 h later: no outcome"), R.Rig.Core->IsTerminated());
		TestEqual(TEXT("24 h later: no WinLatched"), R.CountLogged(ERoundLogType::WinLatched), 0);
		TestTrue(TEXT("24 h later: the pending timer kinds are a subset of {GraceExpiry, PawnLossRetry}"), OnlyContestTimers(*R.Queue));
		TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	}
	{
		// A disconnect schedules the grace timer, and nothing else.
		FRoundEndRig R;
		R.ToActive(2, 2);
		R.Disconnect(2);
		TArray<ETimerKind> Kinds;
		R.Queue->GetPendingTimerKinds(Kinds);
		TestTrue(TEXT("one disconnect: exactly one GraceExpiry timer"), Kinds.Num() == 1 && Kinds[0] == ETimerKind::GraceExpiry);
		TestTrue(TEXT("one disconnect: still only contest timers"), OnlyContestTimers(*R.Queue));
	}
	return true;
}

#endif
