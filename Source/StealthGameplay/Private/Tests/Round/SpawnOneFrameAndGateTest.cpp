// Story rmf-039 (production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md), GDD AC 41 / R6 P6:
//   - at P6 all D pawns are spawned in one server frame (one ServerTickIndex) in sigma order;
//   - a participant still Connecting at P6 gets an Ai-driven, unpossessed pawn behind a closed spawn gate (not relevant
//     to its connection); once Session moves it to Connected the gate opens (relevant and possessed); a participant
//     Connected from the start is not gated.
// Representativeness: [U] through the Round core and the recording spawner (the gate is the spawner's closed-gate
// request and its OpenSpawnGate call). The two-client PIE half needs Session's Connecting state at P6, which does not
// exist before the session-travel epic (no travel, no loading clients); recorded as a deviation in the story.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundFormulas.h"
#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSpawnOneFrameAndGateTest, "StealthGame.Round.SpawnOneFrameAndGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundSpawnOneFrameAndGateTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	const FParticipantId Loading(FRoundExitBRig::HostIdValue + 2);

	// Arrange: 2v2 in Preparing; one participant is still Connecting (its client is loading) at P6.
	FRoundExitBRig Rig;
	Rig.OpenLobby();
	Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
	Rig.EnterPreparing();
	Rig.Fakes.Session.ConnectionStates[Loading.Value] = ERoundConnectionState::Connecting;
	Rig.ScriptPreparingSequence();
	Rig.Core->NotifyTuningReady(PreparingRigTuningHash);
	Rig.Core->NotifyLayoutReady(1, Rig.Core->GetPreparing().GetSeed());
	Rig.AckAll(false);

	// Act: the frame that runs P6.
	Rig.Frame();

	// Assert: D pawns, one frame, sigma order.
	const TArray<FRoundFakeStartSpawner::FSpawn>& Spawns = Rig.Fakes.Spawner.Spawns;
	const int32 D = Layout2v2.Team0 + Layout2v2.Team1 + PreparingRigNpcCount;
	TestEqual(TEXT("all D pawns spawned"), Spawns.Num(), D);
	bool bOneFrame = Spawns.Num() > 0;
	for (const FRoundFakeStartSpawner::FSpawn& Spawn : Spawns)
	{
		bOneFrame = bOneFrame && Spawn.Tick == Spawns[0].Tick;
	}
	TestTrue(TEXT("every pawn has the same spawn frame"), bOneFrame);
	TArray<int32> Sigma;
	StealthRoundFormulas::ComputeSpawnOrder(D, PreparingRigSpawnSubSeed, Sigma);
	TArray<int32> Order;
	for (const FRoundFakeStartSpawner::FSpawn& Spawn : Spawns)
	{
		Order.Add(Spawn.Entry.Slot);
	}
	TestTrue(TEXT("creation order equals sigma"), Order == Sigma);
	TestEqual(TEXT("ExpectedPawnCount = D is published"), Rig.Core->GetPhaseCore().MakePublicState().ExpectedPawnCount, D);

	// The gate.
	for (const FRoundFakeStartSpawner::FSpawn& Spawn : Spawns)
	{
		if (!Spawn.Entry.IsPlayer())
		{
			continue;
		}
		if (Spawn.Entry.Participant == Loading)
		{
			TestTrue(TEXT("the Connecting participant's pawn is Ai-driven (unpossessed) behind a closed gate"),
				Spawn.Entry.Control == ERoundSpawnControl::Ai && Spawn.Entry.bGateClosed);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("participant %u Connected from the start is not gated"), Spawn.Entry.Participant.Value),
				Spawn.Entry.Control == ERoundSpawnControl::Player && !Spawn.Entry.bGateClosed);
		}
	}
	TestTrue(TEXT("the gate is closed until Connected"), Rig.Core->GetPreparing().IsGateClosed(Loading) && Rig.Fakes.Spawner.OpenedGates.Num() == 0);

	// After Connected: the pawn becomes relevant and possessed (the gate opens once).
	Rig.Fakes.Session.ConnectionStates[Loading.Value] = ERoundConnectionState::Connected;
	Rig.Core->OnParticipantConnectionChanged(Loading, ERoundConnectionState::Connected, false, Rig.Clock.GetServerTimeSeconds());
	TestTrue(TEXT("Connected: the gate opened for that participant"), Rig.Fakes.Spawner.OpenedGates.Num() == 1 && Rig.Fakes.Spawner.OpenedGates[0] == Loading);
	TestFalse(TEXT("Connected: no longer gated"), Rig.Core->GetPreparing().IsGateClosed(Loading));
	Rig.Core->OnParticipantConnectionChanged(Loading, ERoundConnectionState::Connected, false, Rig.Clock.GetServerTimeSeconds());
	TestEqual(TEXT("the gate opens once"), Rig.Fakes.Spawner.OpenedGates.Num(), 1);
	TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
