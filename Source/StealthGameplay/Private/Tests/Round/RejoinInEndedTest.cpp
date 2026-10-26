// Story rmf-063 (production/epics/round-match-framework/story-063-rejoin-in-ended.md), GDD AC 23c / R18, R9:
// a rejoin during Ended (the participant disconnected before the round ended, its grace still running): admitted
// (ShouldAdmitExpired true for an InRound participant in Ended), no pawn (no spawn, takeover or BindPawn, no Crowd or
// pawn-control call), no transition, and not the engine spectator state (Round's gateways carry no spectator call and
// Round makes none). Decision (story Open item): Round writes no Rejoined entry for a rejoin in Ended; the admission is
// Session's.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundRejoinInEndedTest, "StealthGame.Round.RejoinInEnded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundRejoinInEndedTest::RunTest(const FString& Parameters)
{
	// Arrange: participant 4 disconnects; team 0 wins by the objective while 4 is away.
	FRoundEndRig R;
	R.ToActive(2, 2);
	const uint32 Away = 4;
	R.Disconnect(Away);
	R.ScriptRoundEnd();
	R.DrainOneTick({ R.Completion(1, 1, 0) });
	TestTrue(TEXT("Ended"), R.Rig.Core->GetPhase() == ERoundPhase::Ended);
	const int32 Mark = R.Rig.Mark();
	const int32 Spawns = R.Rig.Fakes.Spawner.Spawns.Num();
	const int32 Entries = R.Rig.EventLog.Entries.Num();

	// Act.
	R.Rejoin(Away);
	R.Frame();

	// Assert.
	TestTrue(TEXT("admitted: ShouldAdmitExpired in Ended"), R.Rig.Core->ShouldAdmitExpired(FParticipantId(Away)));
	TestEqual(TEXT("no pawn: no spawn"), R.Rig.Fakes.Spawner.Spawns.Num(), Spawns);
	for (const TCHAR* Call : { TEXT("BindPawn"), TEXT("RequestTakeover"), TEXT("RequestHandBack"), TEXT("RequestTransition"), TEXT("DriveBoundPawn"),
		TEXT("ReleaseControl"), TEXT("OnInformantReadmitted") })
	{
		TestEqual(FString::Printf(TEXT("no %s"), Call), R.Rig.CountSince(Mark, Call), 0);
	}
	TestEqual(TEXT("no mutating call at all (no spectator state, no Crowd call)"), R.Rig.CallsSince(Mark).Num(), 0);
	TestEqual(TEXT("no log entry beyond the admission (Session's)"), R.Rig.EventLog.Entries.Num(), Entries);
	TestFalse(TEXT("ShouldAdmitExpired is false for a participant never in the round"), R.Rig.Core->ShouldAdmitExpired(FParticipantId(99)));
	TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
