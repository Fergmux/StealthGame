// Story rmf-055 (production/epics/round-match-framework/story-055-ended-hold.md), GDD AC 12b (rmf-ac-overrides) / R9 (f):
// in Ended every command is rejected (Round publishes the RoundEnded class: AdmitCommand refuses; items reaching the
// closed queue are Rejected_Latched; Look and Move are continuous inputs, never queue items), and in the round-end step
// the Crowd spy receives Halt, then exactly one stop request per NPC, AI-driven, human-driven and in-hand-over pawn, each
// carrying RunStopSec as its bound; a pawn listed twice (AI-driven and in the hand-over table) gets one request; later
// drains send none. The speed clause (< 1 cm/s within RunStopSec + one frame) is story rmf-085's PIE run.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"
#include "View/CommandReplyTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundEndedHoldTest, "StealthGame.Round.EndedHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundEndedHoldTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	// Arrange: the four pawn kinds; pawn 12 appears twice (AI-driven, and pending in the hand-over table).
	FRoundEndRig R;
	R.ToActive(2, 2);
	R.ScriptRoundEnd();
	R.Rig.Fakes.Spawner.LivePawnList = {
		{ FPawnHandle(), 11, ERoundPawnDriver::Npc }, { FPawnHandle(), 12, ERoundPawnDriver::AiDriven },
		{ FPawnHandle(), 13, ERoundPawnDriver::HumanDriven }, { FPawnHandle(), 14, ERoundPawnDriver::InHandOver },
		{ FPawnHandle(), 12, ERoundPawnDriver::InHandOver } };
	TestFalse(TEXT("Active: commands admitted"), R.Rig.Core->AdmitCommand().IsSet());
	const int32 Mark = R.Rig.Mark();

	// Act: an objective win (the round-end step in the latch step).
	R.DrainOneTick({ R.Completion(1, 1, 0) });

	// Assert: the stop set.
	TestTrue(TEXT("Ended"), R.Rig.Core->GetPhase() == ERoundPhase::Ended);
	const int32 HaltIndex = R.IndexOf(TEXT("Halt"), Mark);
	TestTrue(TEXT("Halt once"), HaltIndex != INDEX_NONE && R.Rig.CountSince(Mark, TEXT("Halt")) == 1);
	TMap<int64, int32> Requests;
	bool bBoundsOk = true;
	bool bAfterHalt = true;
	for (int32 Index = Mark; Index < R.Rig.Log.Calls.Num(); ++Index)
	{
		const FRoundFakeCall& Call = R.Rig.Log.Calls[Index];
		if (Call.Call == FName(TEXT("RequestPawnStop")))
		{
			++Requests.FindOrAdd(Call.Args[0]);
			bBoundsOk &= Call.Args[2] == FMath::RoundToInt64(RunStopSec * 1000000.0);
			bAfterHalt &= Index > HaltIndex;
		}
	}
	TestEqual(TEXT("one stop request per pawn (four pawns)"), Requests.Num(), 4);
	for (const TPair<int64, int32>& Pair : Requests)
	{
		TestEqual(FString::Printf(TEXT("pawn %lld: exactly one stop request"), Pair.Key), Pair.Value, 1);
	}
	TestTrue(TEXT("every request carries RunStopSec as its bound"), bBoundsOk);
	TestTrue(TEXT("the stops follow Halt in the step"), bAfterHalt);

	// Every command is rejected in Ended.
	const TOptional<ECommandRejectCode> Admit = R.Rig.Core->AdmitCommand();
	TestTrue(TEXT("Ended: commands are refused"), Admit.IsSet() && Admit.GetValue() == ECommandRejectCode::NotNow);
	const int32 Rejected = R.CountLogged(ERoundLogType::Rejected_Latched);
	R.DrainOneTick({ R.Strike(5, 1, 3), R.Completion(6, 2, 0) });
	TestEqual(TEXT("Ended: discrete commands reaching the queue are Rejected_Latched"), R.CountLogged(ERoundLogType::Rejected_Latched) - Rejected, 2);

	// Later drains send no further stop request.
	const int32 Stops = R.Rig.CountSince(Mark, TEXT("RequestPawnStop"));
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		R.Frame();
	}
	TestEqual(TEXT("later drains: no further stop request"), R.Rig.CountSince(Mark, TEXT("RequestPawnStop")), Stops);
	TestEqual(TEXT("later drains: no further Halt"), R.Rig.CountSince(Mark, TEXT("Halt")), 1);
	TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
