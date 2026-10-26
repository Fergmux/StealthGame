// Story rmf-032 (production/epics/round-match-framework/story-032-preparing-barrier.md), GDD AC 8b / R6 P4, P5, P7:
//   - each of layout, one client's single round acknowledgement and P7 world-ready (until WorldReadyTimeoutSec), withheld
//     alone while the other two are satisfied, holds the phase in Preparing; releasing it reaches Active;
//   - a client that misses Session's deadline (Session moves it to Disconnected) leaves the wait set and Active is reached
//     without it;
//   - each client acknowledges only once.
// Fakes for layout (NotifyLayoutReady), the acknowledgement and world-ready (FRoundTravelAck), advanced by the fake clock.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

namespace PreparingBarrierTestData
{
	enum class EWithheld : uint8
	{
		Layout,
		Acknowledgement,
		WorldReady,
	};

	/** Frames run while the withheld input is held (well inside WorldReadyTimeoutSec and the watchdog). */
	constexpr int32 HoldFrames = 120;

	/** A 2v2 rig in Preparing with tuning ready; returns the rig. */
	TUniquePtr<FRoundExitBRig> MakePreparing()
	{
		TUniquePtr<FRoundExitBRig> Rig = MakeUnique<FRoundExitBRig>();
		Rig->OpenLobby();
		Rig->SetLayout(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		Rig->EnterPreparing();
		Rig->ScriptPreparingSequence();
		Rig->Core->NotifyTuningReady(RoundTestFixtures::PreparingRigTuningHash);
		return Rig;
	}

	void HoldFor(FRoundExitBRig& Rig, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Rig.Frame();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundPreparingBarrierTest, "StealthGame.Round.PreparingBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundPreparingBarrierTest::RunTest(const FString& Parameters)
{
	using namespace PreparingBarrierTestData;
	const FParticipantId Straggler(FRoundExitBRig::HostIdValue + 2);

	for (const EWithheld Withheld : { EWithheld::Layout, EWithheld::Acknowledgement, EWithheld::WorldReady })
	{
		const TCHAR* Name = Withheld == EWithheld::Layout ? TEXT("layout") : Withheld == EWithheld::Acknowledgement ? TEXT("acknowledgement") : TEXT("world-ready");
		TUniquePtr<FRoundExitBRig> Rig = MakePreparing();

		// Arrange: the other two satisfied, this one withheld.
		if (Withheld != EWithheld::Layout)
		{
			Rig->Core->NotifyLayoutReady(1, Rig->Core->GetPreparing().GetSeed());
		}
		Rig->AckAll(/*bWorldReady*/ false, Withheld == EWithheld::Acknowledgement ? Straggler : FParticipantId());
		Rig->Frame();
		if (Withheld == EWithheld::WorldReady)
		{
			Rig->AckAll(/*bWorldReady*/ true, Straggler);
		}
		else if (Rig->Core->GetPreparing().GetStep() == ERoundPreparingStep::AwaitWorldReady)
		{
			Rig->AckAll(/*bWorldReady*/ true);
		}

		// Act + Assert: held in Preparing while withheld (world-ready only until WorldReadyTimeoutSec).
		HoldFor(*Rig, HoldFrames);
		TestTrue(FString::Printf(TEXT("%s withheld: still Preparing"), Name), Rig->Core->GetPhase() == ERoundPhase::Preparing);
		TestEqual(FString::Printf(TEXT("%s withheld: no pawn spawned before the barrier"), Name), Rig->Fakes.Spawner.Spawns.Num() > 0 ? 1 : 0,
			Withheld == EWithheld::WorldReady ? 1 : 0);

		// Release.
		switch (Withheld)
		{
		case EWithheld::Layout:
			Rig->Core->NotifyLayoutReady(1, Rig->Core->GetPreparing().GetSeed());
			Rig->Frame();
			Rig->AckAll(/*bWorldReady*/ true);
			break;
		case EWithheld::Acknowledgement:
			TestTrue(TEXT("the straggler's acknowledgement is accepted"), Rig->Core->NotifyParticipantTravelComplete(Rig->MakeAck(Straggler)));
			TestFalse(TEXT("each client acknowledges only once"), Rig->Core->NotifyParticipantTravelComplete(Rig->MakeAck(Straggler)));
			Rig->Frame();
			Rig->AckAll(/*bWorldReady*/ true);
			break;
		case EWithheld::WorldReady:
			Rig->Core->NotifyParticipantWorldReady(Rig->MakeAck(Straggler));
			break;
		}
		Rig->Frame(); // P7: ActiveAtServerTime
		TestTrue(FString::Printf(TEXT("%s released: ActiveAtServerTime is set"), Name), Rig->Core->GetPreparing().GetActiveAtServerTime() > 0.0);
		Rig->ScriptActiveEntry();
		Rig->Frame(Rig->Core->GetParams().Preparing.StartLeadSec);
		TestTrue(FString::Printf(TEXT("%s released: Active"), Name), Rig->Core->GetPhase() == ERoundPhase::Active);
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Name), Rig->Log.Unexpected.IsEmpty());
	}

	// World-ready past its timeout: the start proceeds without the slow client (it is not disconnected).
	{
		TUniquePtr<FRoundExitBRig> Rig = MakePreparing();
		Rig->Core->NotifyLayoutReady(1, Rig->Core->GetPreparing().GetSeed());
		Rig->AckAll(false);
		Rig->Frame();
		Rig->AckAll(true, Straggler);
		Rig->Frame(Rig->Core->GetParams().Preparing.WorldReadyTimeoutSec);
		TestTrue(TEXT("at WorldReadyTimeoutSec the start is scheduled without the slow client"), Rig->Core->GetPreparing().GetActiveAtServerTime() > 0.0);
		TestEqual(TEXT("the slow client stays Connected (never disconnected by the timeout)"), static_cast<int32>(Rig->Fakes.Session.ConnectionStates[Straggler.Value]),
			static_cast<int32>(ERoundConnectionState::Connected));
	}

	// A client that misses Session's deadline leaves the wait set: Active is reached without its acknowledgement.
	{
		TUniquePtr<FRoundExitBRig> Rig = MakePreparing();
		Rig->Core->NotifyLayoutReady(1, Rig->Core->GetPreparing().GetSeed());
		Rig->AckAll(false, Straggler);
		HoldFor(*Rig, HoldFrames);
		TestTrue(TEXT("the straggler holds the barrier"), Rig->Core->GetPhase() == ERoundPhase::Preparing && Rig->Fakes.Spawner.Spawns.Num() == 0);
		Rig->Fakes.Session.ConnectionStates[Straggler.Value] = ERoundConnectionState::Disconnected; // Session F7 Deadline: TravelTimeout
		Rig->Frame();
		TestTrue(TEXT("the barrier released once the straggler left the wait set"), Rig->Fakes.Spawner.Spawns.Num() > 0);
		Rig->AckAll(true);
		Rig->Frame();
		Rig->ScriptActiveEntry();
		Rig->Frame(Rig->Core->GetParams().Preparing.StartLeadSec);
		TestTrue(TEXT("Active without the timed-out client"), Rig->Core->GetPhase() == ERoundPhase::Active);
		const FRoundFakeStartSpawner::FSpawn* Spawn = Rig->Fakes.Spawner.FindSpawn(Straggler);
		TestTrue(TEXT("the timed-out client still has a (gated, Ai-driven) pawn"), Spawn != nullptr && Spawn->Entry.Control == ERoundSpawnControl::Ai && Spawn->Entry.bGateClosed);
		TestTrue(TEXT("no unexpected call"), Rig->Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
