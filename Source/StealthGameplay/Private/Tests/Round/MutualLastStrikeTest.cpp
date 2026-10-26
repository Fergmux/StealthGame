// Story rmf-046 (production/epics/round-match-framework/story-046-mutual-last-strike.md), GDD AC 16 / R14, R15:
//   both teams' last Active players strike each other in one tick; in both key permutations the winner is the team of
//   the attacker whose strike has the lower key, the other strike is Rejected_Latched, and there is exactly one
//   WinLatched entry. Real queue and latch, stub strike resolver, keys forced by one mailbox's FIFO.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundWinQueueRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundMutualLastStrikeTest, "StealthGame.Round.MutualLastStrike",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundMutualLastStrikeTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	// 2v2: team 0 = {1, 2}, team 1 = {3, 4}; 2 and 4 are informants, so 1 and 3 are each team's last Active.
	constexpr uint32 Last0 = 1;
	constexpr uint32 Last1 = 3;
	constexpr uint32 StrikeBy0 = 20;
	constexpr uint32 StrikeBy1 = 21;

	for (const bool bTeam0First : { true, false })
	{
		const TCHAR* Name = bTeam0First ? TEXT("team 0's strike has the lower key") : TEXT("team 1's strike has the lower key");
		FRoundWinQueueRig W;
		W.ToActive(Layout2v2.Team0, Layout2v2.Team1);
		W.Rig.Fakes.Roster.SetState(FParticipantId(2), EParticipantState::Informant);
		W.Rig.Fakes.Roster.SetState(FParticipantId(4), EParticipantState::Informant);
		const FQueueItem A = W.Strike(StrikeBy0, Last0, Last1);
		const FQueueItem B = W.Strike(StrikeBy1, Last1, Last0);
		const int32 WinLatchedBefore = W.CountLogged(ERoundLogType::WinLatched);

		if (bTeam0First)
		{
			W.DrainOneTick({ A, B });
		}
		else
		{
			W.DrainOneTick({ B, A });
		}

		const FRoundTerminalLatch& Latch = W.Rig.Core->GetLatch();
		const uint8 ExpectedWinner = bTeam0First ? 0 : 1;
		TestTrue(FString::Printf(TEXT("%s: the lower-key attacker's team wins by elimination"), Name), Latch.IsTerminated() && Latch.GetOutcome().IsWin()
			&& Latch.GetOutcome().WinningTeam == ExpectedWinner && Latch.GetOutcome().EndReason == ERoundEndReason::Elimination);
		TestTrue(FString::Printf(TEXT("%s: the other strike is Rejected_Latched"), Name), W.Strikes.Resolved.Num() == 2
			&& !W.Strikes.Resolved[0].bRejectedLatched && W.Strikes.Resolved[1].bRejectedLatched);
		TestTrue(FString::Printf(TEXT("%s: the lower-key attacker survives"), Name), W.IsActive(bTeam0First ? Last0 : Last1));
		TestEqual(FString::Printf(TEXT("%s: exactly one WinLatched entry"), Name), W.CountLogged(ERoundLogType::WinLatched) - WinLatchedBefore, 1);
		TestEqual(FString::Printf(TEXT("%s: one Rejected_Latched entry"), Name), W.CountLogged(ERoundLogType::Rejected_Latched), 1);
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Name), W.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
