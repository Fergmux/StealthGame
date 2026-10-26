// Story rmf-052 (production/epics/round-match-framework/story-052-ended-latch-effects.md), GDD AC 12a / R9, R15, F2:
// a latch by an objective win: in the latch step, which is also the round-end step (the staged RoundEndStep is consumed
// inline at slot end), FRoundResult has no duration field (reflection over the USTRUCT), MatchScore[Winner] rose by
// exactly 1, EndRoundAll ran once and its batch event produced no evaluation, every pending item is Rejected_Latched
// (one submitted after the latch too, logged), OnRoundEnded fired once and every viewer resolves as PostRound. A second
// TryTerminate returns false, logs LateWinSuppressed and re-runs none of it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundEndedLatchEffectsTest, "StealthGame.Round.EndedLatchEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundEndedLatchEffectsTest::RunTest(const FString& Parameters)
{
	// (1) FRoundResult carries exactly the R9 (a) tuple {Winner, EndReason, RoundIndex, TeamSummary[2]}: no duration.
	{
		TArray<FString> Names;
		bool bAnyTime = false;
		for (TFieldIterator<FProperty> It(FRoundResult::StaticStruct()); It; ++It)
		{
			Names.Add(It->GetName());
			bAnyTime |= It->IsA<FDoubleProperty>() || It->IsA<FFloatProperty>() || It->GetName().Contains(TEXT("Time")) || It->GetName().Contains(TEXT("Duration"));
		}
		TestEqual(TEXT("FRoundResult: four fields"), Names.Num(), 4);
		TestTrue(TEXT("FRoundResult: Winner, EndReason, RoundIndex, TeamSummary"), Names.Contains(GET_MEMBER_NAME_CHECKED(FRoundResult, WinningTeam).ToString())
			&& Names.Contains(GET_MEMBER_NAME_CHECKED(FRoundResult, PublicReason).ToString()) && Names.Contains(GET_MEMBER_NAME_CHECKED(FRoundResult, RoundIndex).ToString())
			&& Names.Contains(GET_MEMBER_NAME_CHECKED(FRoundResult, TeamSummary).ToString()));
		TestFalse(TEXT("FRoundResult: no duration or time field"), bAnyTime);
	}

	// Arrange: 2v2 Active; a final completion by participant 1 (team 0), then two strikes in the same tick.
	FRoundEndRig R;
	R.ToActive(2, 2);
	R.ScriptRoundEnd();
	const uint32 Member = 1;
	TestTrue(TEXT("before the latch: InRound viewers"), R.Rig.Core->GetViewerClass(FParticipantId(Member)) == ERoundViewerClass::InRound);
	const int32 Mark = R.Rig.Mark();
	// The completion stub also submits one more item after declaring the win (it is stamped at the next drain).
	R.Completions.OnResolve = [&R](const FQueueItem& Item, FQueueKey Key)
	{
		const bool bWon = R.Rig.Core->SubmitFinalCompletion(FParticipantId(1), 0, FRoundCompleterControl(), true, Key);
		R.Queue->Submit(FMailboxId::ServerLocal(), R.Strike(9, 2, 4));
		return bWon ? EResolveResult::Executed : EResolveResult::Voided;
	};

	// Act.
	R.DrainOneTick({ R.Completion(1, 1, 0), R.Strike(2, 1, 3), R.Strike(3, 2, 4) });

	// Assert: one step.
	const FRoundPublicState Public = R.Rig.Core->GetPhaseCore().MakePublicState();
	TestTrue(TEXT("the latch step is the round-end step: Ended in the same drain"), R.Rig.Core->GetPhase() == ERoundPhase::Ended);
	TestTrue(TEXT("FRoundResult: team 0, objective, this round"), Public.LastResult.WinningTeam == 0 && Public.LastResult.PublicReason == RoundPublicReason::Objective
		&& Public.LastResult.RoundIndex == 1);
	TestTrue(TEXT("MatchScore[Winner] + 1, the loser's unchanged"), Public.MatchScore[0] == 1 && Public.MatchScore[1] == 0);
	TestEqual(TEXT("EndRoundAll ran once"), R.Rig.CountSince(Mark, TEXT("EndRoundAll")), 1);
	TestEqual(TEXT("the two pending strikes resolved Rejected_Latched (logged)"), R.CountLogged(ERoundLogType::Rejected_Latched), 2);
	TestEqual(TEXT("no strike was resolved"), R.Logged(ERoundLogType::StateTransition).Num(), 0);
	TestEqual(TEXT("OnRoundEnded fired once"), R.RoundEndedCount, 1);
	for (const FRoundRosterMember& Viewer : R.Rig.Fakes.Roster.Members)
	{
		TestTrue(TEXT("every viewer resolves as PostRound"), R.Rig.Core->GetViewerClass(Viewer.Participant) == ERoundViewerClass::PostRound);
	}

	// The item submitted after the latch within that drain: Rejected_Latched at the next drain.
	R.Frame();
	TestEqual(TEXT("the item submitted after the latch is Rejected_Latched"), R.CountLogged(ERoundLogType::Rejected_Latched), 3);

	// EndRoundAll's batch event is never evaluated as an elimination (F2 proof item 5).
	FRoundTransitionEvent Batch;
	Batch.bBatch = true;
	Batch.Participant = FParticipantId(3);
	Batch.Team = ERosterTeam::Team1;
	Batch.Old = EParticipantState::Active;
	Batch.New = EParticipantState::RoundEnded;
	const int32 Suppressed = R.Rig.Core->GetLatch().GetSuppressedCount();
	R.Rig.Core->OnRosterTransition(Batch);
	TestEqual(TEXT("the batch event: no evaluation"), R.Rig.Core->GetLatch().GetSuppressedCount(), Suppressed);

	// A second latch attempt re-runs nothing.
	const int32 Late = R.CountLogged(ERoundLogType::LateWinSuppressed);
	TestFalse(TEXT("a second TryTerminate returns false"), R.Rig.Core->TryTerminate(FRoundTerminalOutcome::Win(1, ERoundEndReason::Elimination), R.Rig.Core->DecisionKey()));
	TestEqual(TEXT("LateWinSuppressed logged"), R.CountLogged(ERoundLogType::LateWinSuppressed) - Late, 1);
	TestTrue(TEXT("second attempt: score unchanged"), R.Rig.Core->GetPhaseCore().GetMatchScore(0) == 1 && R.Rig.Core->GetPhaseCore().GetMatchScore(1) == 0);
	TestEqual(TEXT("second attempt: EndRoundAll not called again"), R.Rig.CountSince(Mark, TEXT("EndRoundAll")), 1);
	TestEqual(TEXT("second attempt: no second OnRoundEnded"), R.RoundEndedCount, 1);
	TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
