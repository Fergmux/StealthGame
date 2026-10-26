// Story rmf-061 (production/epics/round-match-framework/story-061-rejoin-inside-grace.md), GDD AC 23a / R17.3, R18:
// a rejoin 1 s before the grace expiry (fake clock): the participant stays Active (no transition requested), the grace
// timer is cancelled (no record; the timer item finds none and changes nothing), and control of the same pawn returns
// to the human: exactly one hand-back request for the pawn id that was taken over, with no spawn, no bind and no card move.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundRejoinInsideGraceTest, "StealthGame.Round.RejoinInsideGrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundRejoinInsideGraceTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	// Arrange: participant 2's bound pawn is FixtureBoundPawnId; it disconnects and is taken over.
	FRoundEndRig R;
	R.ToActive(2, 2);
	const uint32 Away = 2;
	R.Rig.Fakes.PawnControl.BoundPawnIds.Add(Away, FixtureBoundPawnId);
	const int32 Mark = R.Rig.Mark();
	const int32 SpawnsBefore = R.Rig.Fakes.Spawner.Spawns.Num();
	R.Disconnect(Away);
	const FRoundGraceRecord* Record = R.Rig.Core->GetGraceTable().Find(FParticipantId(Away));
	const double Expiry = Record != nullptr ? Record->GraceExpiry : 0.0;
	TestTrue(TEXT("a grace record exists"), Record != nullptr);

	// Act: the rejoin completes 1 s before the expiry.
	R.RunUntil(Expiry - RejoinBeforeExpirySec);
	R.Rig.Fakes.Session.Generations.Add(Away, 1); // a rejoin bumps the ConnectionGeneration
	R.Rejoin(Away);
	R.RunUntil(Expiry + 1.0);

	// Assert.
	TestTrue(TEXT("still Active"), R.IsActive(Away));
	TestEqual(TEXT("no transition requested"), R.Rig.CountSince(Mark, TEXT("RequestTransition")), 0);
	TestEqual(TEXT("the grace timer is cancelled (no record)"), R.Rig.Core->GetGraceTable().Num(), 0);
	const int32 Takeover = R.IndexOf(TEXT("RequestTakeover"), Mark);
	const int32 HandBack = R.IndexOf(TEXT("RequestHandBack"), Mark);
	TestEqual(TEXT("exactly one hand-back"), R.Rig.CountSince(Mark, TEXT("RequestHandBack")), 1);
	TestTrue(TEXT("the same pawn id taken over and handed back"), Takeover != INDEX_NONE && HandBack != INDEX_NONE
		&& R.Rig.Log.Calls[Takeover].Args[1] == FixtureBoundPawnId && R.Rig.Log.Calls[HandBack].Args[1] == FixtureBoundPawnId);
	TestEqual(TEXT("no spawn"), R.Rig.Fakes.Spawner.Spawns.Num(), SpawnsBefore);
	TestEqual(TEXT("no bind and no card move"), R.Rig.CountSince(Mark, TEXT("BindPawn")) + R.Rig.CountSince(Mark, TEXT("RecordAppearance")), 0);
	TestFalse(TEXT("no longer AI-driven"), R.Rig.Core->IsAiDriven(FParticipantId(Away)));
	TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
