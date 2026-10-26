// Story rmf-064 (production/epics/round-match-framework/story-064-last-disconnect-forfeits.md), GDD AC 24a / R15 (b),
// R17.6, F2, R20: team 1's last Active member converts at its grace expiry while team 0 has 2 Active members, 1 of them
// Connected (the F2 forfeit example): EliminationWin(0), the log-only EndReason ForfeitElimination, MatchScore[0] + 1 in
// the round-end step (a grace expiry is not a strike: the latch step is the round-end step), and the public result is
// the one elimination reason, whose banner text is "every rival eliminated for the round" (never a forfeit phrase).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundLastDisconnectForfeitsTest, "StealthGame.Round.LastDisconnectForfeits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundLastDisconnectForfeitsTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	// Arrange: 2v2 (1, 2 team 0; 3, 4 team 1). 3 is assassinated; 4 (team 1's last Active) disconnects at t; 2 (team 0)
	// disconnects later, so at 4's expiry team 0 has 2 Active members, 1 Connected.
	FRoundEndRig R;
	R.ToActive(2, 2);
	R.Transition(3, EParticipantState::Informant, ERosterReason::Assassinated);
	R.Disconnect(4);
	const double Expiry = R.Rig.Core->GetGraceTable().Find(FParticipantId(4))->GraceExpiry;
	R.RunUntil(Expiry - DisconnectAfterActiveSec * 0.5);
	R.Disconnect(2);
	R.ScriptRoundEnd();
	R.Rig.Fakes.Roster.Script(TEXT("RequestTransition"), static_cast<int64>(ERosterResult::Applied));
	R.Rig.Fakes.Roster.Script(TEXT("ReleaseToCrowd"), static_cast<int64>(ERosterResult::Applied));
	const int32 Mark = R.Rig.Mark();

	// Act.
	R.RunUntil(Expiry);

	// Assert.
	TestEqual(TEXT("team 0: 2 Active"), R.ActiveOn(ERosterTeam::Team0), 2);
	TestTrue(TEXT("EliminationWin(0)"), R.Rig.Core->GetLatch().GetOutcome().IsWin() && R.Rig.Core->GetLatch().GetOutcome().WinningTeam == 0);
	const TArray<FRoundLogEntry> Latched = R.Logged(ERoundLogType::WinLatched);
	TestTrue(TEXT("log EndReason ForfeitElimination"), Latched.Num() == 1 && Latched[0].Code == static_cast<uint16>(ERoundEndReason::ForfeitElimination));
	const FRoundPublicState Public = R.Rig.Core->GetPhaseCore().MakePublicState();
	TestTrue(TEXT("Ended at the expiry (no staging for a forfeit)"), Public.Phase == ERoundPhase::Ended && FMath::IsNearlyEqual(Public.PhaseEnteredServerTime, Expiry, 1e-6));
	TestTrue(TEXT("MatchScore[0] + 1"), Public.MatchScore[0] == 1 && Public.MatchScore[1] == 0);
	TestEqual(TEXT("the public reason is the one elimination reason"), static_cast<int32>(Public.LastResult.PublicReason), static_cast<int32>(RoundPublicReason::Elimination));
	TestEqual(TEXT("banner: every rival eliminated for the round"), GetRoundEndReasonText(Public.LastResult.PublicReason).ToString(), FString(TEXT("every rival eliminated for the round")));
	TestEqual(TEXT("EndRoundAll in the step"), R.Rig.CountSince(Mark, TEXT("EndRoundAll")), 1);
	TestTrue(TEXT("no fallback binding fault"), R.Support.Faults.GetRecords().IsEmpty());
	TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
