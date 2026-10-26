// Story rmf-045 (production/epics/round-match-framework/story-045-same-tick-final-vs-elimination.md), GDD AC 15 / R11, R15, F2:
//   - team 0's final completion at Seq n and the strike that eliminates team 0's last Active at Seq n+1 in one tick:
//     ObjectiveWin(0), and the strike is Rejected_Latched;
//   - with the order swapped: EliminationWin(1), and the completion is Rejected_Latched;
//   - exactly one WinLatched entry in each case (and the loser's Rejected_Latched entry).
// Real queue and latch, stub resolvers (stories rmf-004/008), keys forced by one mailbox's FIFO in one tick.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundWinQueueRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSameTickFinalVsEliminationTest, "StealthGame.Round.SameTickFinalVsElimination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundSameTickFinalVsEliminationTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	// 2v2: team 0 = {1, 2}, team 1 = {3, 4}; member 2 is already an informant, so 1 is team 0's last Active.
	constexpr uint32 LastActive0 = 1;
	constexpr uint32 Informant0 = 2;
	constexpr uint32 Striker1 = 3;
	constexpr uint32 CompletionTag = 10;
	constexpr uint32 StrikeTag = 11;

	for (const bool bCompletionFirst : { true, false })
	{
		const TCHAR* Name = bCompletionFirst ? TEXT("completion at Seq n") : TEXT("strike at Seq n");
		FRoundWinQueueRig W;
		W.ToActive(Layout2v2.Team0, Layout2v2.Team1);
		W.Rig.Fakes.Roster.SetState(FParticipantId(Informant0), EParticipantState::Informant);
		const FQueueItem Final = W.Completion(CompletionTag, LastActive0, 0);
		const FQueueItem Kill = W.Strike(StrikeTag, Striker1, LastActive0);
		const int32 WinLatchedBefore = W.CountLogged(ERoundLogType::WinLatched);

		// Act: one tick, keys forced.
		if (bCompletionFirst)
		{
			W.DrainOneTick({ Final, Kill });
		}
		else
		{
			W.DrainOneTick({ Kill, Final });
		}

		// Assert.
		const FRoundTerminalLatch& Latch = W.Rig.Core->GetLatch();
		TestTrue(FString::Printf(TEXT("%s: the round latched"), Name), Latch.IsTerminated());
		if (bCompletionFirst)
		{
			TestTrue(TEXT("completion first: ObjectiveWin(0)"), Latch.GetOutcome().IsWin() && Latch.GetOutcome().WinningTeam == 0
				&& Latch.GetOutcome().EndReason == ERoundEndReason::ObjectiveWin);
			TestTrue(TEXT("completion first: the strike is Rejected_Latched, never resolved"), W.Strikes.Resolved.Num() == 1 && W.Strikes.Resolved[0].bRejectedLatched);
			TestTrue(TEXT("completion first: the victim is still Active (the strike never ran)"), W.IsActive(LastActive0));
		}
		else
		{
			TestTrue(TEXT("strike first: EliminationWin(1)"), Latch.GetOutcome().IsWin() && Latch.GetOutcome().WinningTeam == 1
				&& Latch.GetOutcome().EndReason == ERoundEndReason::Elimination);
			TestTrue(TEXT("strike first: the completion is Rejected_Latched, never resolved"), W.Completions.Resolved.Num() == 1 && W.Completions.Resolved[0].bRejectedLatched);
		}
		const TArray<FQueueSlotRecord>& Slots = W.Queue->GetLastDrainRecords();
		TestTrue(FString::Printf(TEXT("%s: both items in one tick with consecutive Seqs"), Name), Slots.Num() == 2
			&& Slots[0].Key.ServerTickIndex == Slots[1].Key.ServerTickIndex && Slots[1].Key.Seq == Slots[0].Key.Seq + 1);
		TestTrue(FString::Printf(TEXT("%s: the terminal key is Seq n"), Name), Slots.Num() == 2 && Latch.GetTerminalKey() == Slots[0].Key
			&& Slots[1].Outcome == EQueueSlotOutcome::Rejected_Latched);
		TestEqual(FString::Printf(TEXT("%s: exactly one WinLatched entry"), Name), W.CountLogged(ERoundLogType::WinLatched) - WinLatchedBefore, 1);
		TestEqual(FString::Printf(TEXT("%s: one Rejected_Latched entry for the loser"), Name), W.CountLogged(ERoundLogType::Rejected_Latched), 1);
		TestTrue(FString::Printf(TEXT("%s: the round-end step ran (Ended)"), Name), W.Rig.Core->GetPhase() == ERoundPhase::Ended);
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Name), W.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
