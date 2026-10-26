// Story rmf-033 (production/epics/round-match-framework/story-033-stale-ack-dropped.md), GDD AC 8c / R6 P5:
//   - with the barrier satisfied except for one client, an acknowledgement carrying an old nonce (an earlier Preparing)
//     and one delivered by a replaced controller (an older ConnectionGeneration) are each dropped: the barrier stays
//     unsatisfied and the phase stays Preparing;
//   - a dropped acknowledgement starts, extends or restarts no deadline (the watchdog deadline is unchanged);
//   - a valid acknowledgement afterwards satisfies the barrier.
// Open point (GDD R6 P5, ADR-0002 section 8.6): neither names logging for a dropped acknowledgement, so nothing is logged;
// the test asserts that no Event Log entry is appended by the drops.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundStaleAckDroppedTest, "StealthGame.Round.StaleAckDropped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundStaleAckDroppedTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	const FParticipantId Late(FRoundExitBRig::HostIdValue + 3);

	// Arrange: a first Preparing aborted by the host, so an earlier nonce exists; then the round under test.
	FRoundExitBRig Rig;
	Rig.OpenLobby();
	Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
	Rig.EnterPreparing();
	const FRoundAckEpoch OldEpoch = Rig.Core->GetPreparing().GetAckEpoch();
	Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"));
	Rig.Fakes.CharacterPool.Script(TEXT("DiscardDeal"));
	Rig.Fakes.Session.Script(TEXT("RequestLobbyTravel"));
	Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Abort);
	Rig.Fakes.Roster.Script(TEXT("SetTeamsLocked"), static_cast<int64>(ERosterResult::Applied));
	Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"));
	Rig.Core->Tick(); // Reset -> Lobby
	Rig.EnterPreparing();
	TestTrue(TEXT("a new Preparing has a new nonce"), Rig.Core->GetPreparing().GetAckEpoch() != OldEpoch);

	Rig.Fakes.Session.Generations.Add(Late.Value, 2); // the late client's current connection is its second
	Rig.ScriptPreparingSequence();
	Rig.Core->NotifyTuningReady(PreparingRigTuningHash);
	Rig.Core->NotifyLayoutReady(1, Rig.Core->GetPreparing().GetSeed());
	Rig.AckAll(/*bWorldReady*/ false, Late);
	const double DeadlineBefore = Rig.Core->GetPreparing().GetWatchdogDeadline();
	const int32 EntriesBefore = Rig.EventLog.Entries.Num();

	// Act 1: an acknowledgement with the previous round's nonce.
	FRoundTravelAck OldNonce = Rig.MakeAck(Late);
	OldNonce.Epoch = OldEpoch;
	TestFalse(TEXT("an old-nonce acknowledgement is dropped"), Rig.Core->NotifyParticipantTravelComplete(OldNonce));
	Rig.Frame();
	TestFalse(TEXT("old nonce: the barrier is not satisfied"), Rig.Core->GetPreparing().IsBarrierSatisfied());
	TestTrue(TEXT("old nonce: still Preparing, nothing spawned"), Rig.Core->GetPhase() == ERoundPhase::Preparing && Rig.Fakes.Spawner.Spawns.Num() == 0);

	// Act 2: an acknowledgement delivered by the replaced controller (its first connection, generation 1).
	FRoundTravelAck Replaced = Rig.MakeAck(Late);
	Replaced.ConnectionGeneration = 1;
	TestFalse(TEXT("a replaced controller's acknowledgement is dropped"), Rig.Core->NotifyParticipantTravelComplete(Replaced));
	Rig.Frame();
	TestFalse(TEXT("replaced controller: the barrier is not satisfied"), Rig.Core->GetPreparing().IsBarrierSatisfied());
	TestTrue(TEXT("replaced controller: still Preparing, nothing spawned"), Rig.Core->GetPhase() == ERoundPhase::Preparing && Rig.Fakes.Spawner.Spawns.Num() == 0);

	// Bounded, not extended; nothing logged.
	TestEqual(TEXT("the dropped acknowledgements moved no deadline"), Rig.Core->GetPreparing().GetWatchdogDeadline(), DeadlineBefore);
	TestEqual(TEXT("the dropped acknowledgements logged nothing"), Rig.EventLog.Entries.Num(), EntriesBefore);

	// A valid acknowledgement afterwards satisfies the barrier.
	TestTrue(TEXT("the valid acknowledgement is accepted"), Rig.Core->NotifyParticipantTravelComplete(Rig.MakeAck(Late)));
	TestTrue(TEXT("the barrier is satisfied"), Rig.Core->GetPreparing().IsBarrierSatisfied());
	Rig.Frame();
	TestTrue(TEXT("P6 ran after the valid acknowledgement"), Rig.Fakes.Spawner.Spawns.Num() > 0);
	TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
