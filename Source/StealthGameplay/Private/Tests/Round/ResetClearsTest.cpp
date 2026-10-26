// Story rmf-058 (production/epics/round-match-framework/story-058-reset-clears.md), GDD AC 13a / R10, R21:
// in Reset (from Ended, with a grace timer pending), by the call-order spy: admission Closed (1), then every player pawn of
// the round destroyed (4) before RequestLobbyTravel (6); the Round timer heap is empty (2); no ResetAll. The phase becomes
// Lobby (admission Open, teams unlocked) only when every participant is terminal (TravelComplete or Disconnected), or
// when TravelLoadTimeoutSec has passed; one tick before either exit the phase is still Reset.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

namespace ResetClearsTestData
{
	/** 2v2 Active, participant 4 disconnected (grace timer pending), objective win, held reset at the dwell: Reset. */
	void ToReset(FRoundEndRig& R)
	{
		R.ToActive(2, 2);
		R.Disconnect(4);
		R.ScriptRoundEnd();
		R.DrainOneTick({ R.Completion(1, 1, 0) });
		R.Rig.Clock.AdvanceSeconds(RoundTestFixtures::EndedMinDwellSec + 0.5);
		R.ScriptReset();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundResetClearsTest, "StealthGame.Round.ResetClears",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundResetClearsTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace ResetClearsTestData;

	// --- (a) the R10 order and the exit when every participant is terminal ----------------------------------------------
	{
		FRoundEndRig R;
		ToReset(R);
		TestEqual(TEXT("before Reset: the grace timer is pending"), R.Queue->GetPendingTimerCount(), 1);
		const int32 Mark = R.Rig.Mark();

		// Act.
		TestTrue(TEXT("reset in Ended executes"), R.Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Reset) == ERoundCommandResult::Executed);

		// Assert: the order.
		TestTrue(TEXT("Reset"), R.Rig.Core->GetPhase() == ERoundPhase::Reset);
		int32 Closed = INDEX_NONE;
		for (int32 Index = Mark; Index < R.Rig.Log.Calls.Num() && Closed == INDEX_NONE; ++Index)
		{
			const FRoundFakeCall& Call = R.Rig.Log.Calls[Index];
			Closed = Call.Call == FName(TEXT("SetAdmissionMode")) && Call.Args[0] == static_cast<int64>(ERoundAdmissionMode::Closed) ? Index : INDEX_NONE;
		}
		const int32 Destroy = R.IndexOf(TEXT("DestroyRoundPawns"), Mark);
		const int32 Travel = R.IndexOf(TEXT("RequestLobbyTravel"), Mark);
		TestTrue(TEXT("(1) admission Closed first"), Closed != INDEX_NONE && Closed < Destroy);
		TestTrue(TEXT("(4) pawns destroyed before (6) RequestLobbyTravel"), Destroy != INDEX_NONE && Travel != INDEX_NONE && Destroy < Travel);
		TestEqual(TEXT("(2) the Round timer heap is empty"), R.Queue->GetPendingTimerCount(), 0);
		TestEqual(TEXT("(2) no grace record"), R.Rig.Core->GetGraceTable().Num(), 0);
		TestEqual(TEXT("Reset never calls ResetAll"), R.Rig.CountSince(Mark, TEXT("ResetAll")), 0);
		TestEqual(TEXT("(4) destroyed, never released"), R.Rig.CountSince(Mark, TEXT("ReleaseToCrowd")), 0);

		// (7) participants become terminal one by one (4 is already Disconnected).
		R.Rig.Core->NotifyParticipantLobbyTravelComplete(FParticipantId(1));
		R.Rig.Core->NotifyParticipantLobbyTravelComplete(FParticipantId(2));
		R.Frame();
		TestTrue(TEXT("one participant not terminal: still Reset"), R.Rig.Core->GetPhase() == ERoundPhase::Reset);
		R.Rig.Fakes.Roster.Script(TEXT("SetTeamsLocked"), static_cast<int64>(ERosterResult::Applied));
		R.Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"));
		R.Rig.Core->NotifyParticipantLobbyTravelComplete(FParticipantId(3));
		R.Frame();
		TestTrue(TEXT("the last terminal participant: Lobby"), R.Rig.Core->GetPhase() == ERoundPhase::Lobby);
		const FRoundFakeCall* Open = R.Rig.Log.Calls.FindByPredicate([](const FRoundFakeCall& C)
		{
			return C.Call == FName(TEXT("SetAdmissionMode")) && C.Args[0] == static_cast<int64>(ERoundAdmissionMode::Open);
		});
		TestTrue(TEXT("Lobby: admission Open"), Open != nullptr);
		TestFalse(TEXT("Lobby: teams unlocked"), R.Rig.Core->AreTeamsLockedByRound());
		TestTrue(TEXT("(a) no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	}

	// --- (b) one participant never terminal: the exit at TravelLoadTimeoutSec ------------------------------------------
	{
		FRoundEndRig R;
		ToReset(R);
		R.Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Reset);
		const double ResetAt = R.Rig.Clock.GetServerTimeSeconds();
		R.Rig.Core->NotifyParticipantLobbyTravelComplete(FParticipantId(1));
		R.Rig.Core->NotifyParticipantLobbyTravelComplete(FParticipantId(2));
		R.Frame(ResetTravelLoadTimeoutSec - 2.0 * RigFrameSec);
		R.Frame();
		TestTrue(TEXT("one tick before the bound: still Reset"), R.Rig.Core->GetPhase() == ERoundPhase::Reset
			&& R.Rig.Clock.GetServerTimeSeconds() < ResetAt + ResetTravelLoadTimeoutSec);
		R.Rig.Fakes.Roster.Script(TEXT("SetTeamsLocked"), static_cast<int64>(ERosterResult::Applied));
		R.Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"));
		R.Frame(2.0 * RigFrameSec);
		TestTrue(TEXT("at TravelLoadTimeoutSec: Lobby"), R.Rig.Core->GetPhase() == ERoundPhase::Lobby);
		TestTrue(TEXT("(b) no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
