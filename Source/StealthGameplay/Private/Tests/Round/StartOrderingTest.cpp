// Story rmf-024 (production/epics/round-match-framework/story-024-start-ordering.md), GDD AC 6a / R5 (decided rule
// C2): an accepted Start records exactly SetAdmissionMode(RejoinOnly), SetTeamsLocked(true), ResetAll(RoundsPlayedCount
// + 1), RequestRoundTravel, in that order, with nothing mutating between them; the phase is Lobby until Session
// accepts the travel and Preparing only after it. Spy rule (story open point): the spy compares mutating calls only;
// side-effect-free queries (CanRequestRoundTravel, GetConnectionState, ...) are recorded but excluded.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundStartOrderingTest, "StealthGame.Round.StartOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundStartOrderingTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	// Arrange: a valid 2v2 Lobby; RoundsPlayedCount 0.
	FRoundExitBRig Rig;
	Rig.OpenLobby();
	Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
	Rig.ScriptStartToTravel(ERoundTravelRequestResult::Accepted);
	const uint32 RoundsPlayed = Rig.Core->GetPhaseCore().GetRoundsPlayedCount();
	const int32 Mark = Rig.Mark();

	// Act: Start (Session's acceptance is deferred: RequestRoundTravel accepts the request, the travel completes later).
	const FStartRefusal Result = Rig.Core->Start(FRoundExitBRig::HostCaller());

	// Assert: exactly the four calls, in order.
	TestFalse(TEXT("Start is accepted"), Result.IsRefused());
	const TArray<FRoundFakeCall> Calls = Rig.CallsSince(Mark);
	if (TestEqual(TEXT("exactly four mutating calls"), Calls.Num(), 4))
	{
		TestTrue(TEXT("1: SetAdmissionMode(RejoinOnly)"), Calls[0].Gateway == ERoundFakeGateway::Session && Calls[0].Call == FName(TEXT("SetAdmissionMode"))
			&& Calls[0].Args[0] == static_cast<int64>(ERoundAdmissionMode::RejoinOnly));
		TestTrue(TEXT("2: SetTeamsLocked(true)"), Calls[1].Gateway == ERoundFakeGateway::Roster && Calls[1].Call == FName(TEXT("SetTeamsLocked")) && Calls[1].Args[0] == 1);
		TestTrue(TEXT("3: ResetAll(RoundsPlayedCount + 1)"), Calls[2].Call == FName(TEXT("ResetAll")) && Calls[2].Args[0] == static_cast<int64>(RoundsPlayed + 1));
		TestTrue(TEXT("4: RequestRoundTravel(RoundIndex)"), Calls[3].Gateway == ERoundFakeGateway::Session && Calls[3].Call == FName(TEXT("RequestRoundTravel"))
			&& Calls[3].Args[0] == static_cast<int64>(RoundsPlayed + 1));
	}
	TestTrue(TEXT("the phase is still Lobby before the travel is accepted"), Rig.Core->GetPhase() == ERoundPhase::Lobby);
	TestEqual(TEXT("RoundsPlayedCount does not advance at Start"), Rig.Core->GetPhaseCore().GetRoundsPlayedCount(), RoundsPlayed);

	// Act: Session accepts the travel.
	Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"));
	TestTrue(TEXT("travel acceptance is taken"), Rig.Core->NotifyRoundTravelAccepted());

	// Assert
	TestTrue(TEXT("the phase is Preparing only after acceptance"), Rig.Core->GetPhase() == ERoundPhase::Preparing);
	TestEqual(TEXT("the public RoundIndex is RoundsPlayedCount + 1"), Rig.Core->GetPhaseCore().MakePublicState().RoundIndex, RoundsPlayed + 1);
	TestFalse(TEXT("a second acceptance is ignored"), Rig.Core->NotifyRoundTravelAccepted());
	TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
