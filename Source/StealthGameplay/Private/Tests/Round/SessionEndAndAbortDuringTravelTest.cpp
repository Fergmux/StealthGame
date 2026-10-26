// Story rmf-059 (production/epics/round-match-framework/story-059-session-end-and-abort-during-travel.md), GDD AC 43 /
// R8, R17.8, R22, Edge Cases. (1) OnSessionEnding in each of Lobby, Preparing, Active, Ended and Reset: one SessionEnded
// entry is logged, the phase is unchanged, nothing is released (no ReleaseToCrowd, no pawn destroyed), and processing
// stops (with the fake clock advanced past every pending timer and dwell: no item resolves, no timer fires, no phase
// change, host commands are refused). (2) A host abort while the round travels (before P1): Session is asked to cancel
// the travel; Reset then runs at P1, or on Session's travel cancel, with 0 pawns spawned. Decision (story Open item): the
// pre-P1 abort is a host Preparing abort (EndReason Aborted, bCalledOff); its slice is archived with no header.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

namespace SessionEndAndAbortDuringTravelTestData
{
	/** Drives the round-end rig to Phase (Active and later through the real Preparing; Active has a pending grace timer). */
	void DriveTo(FRoundEndRig& R, ERoundPhase Phase)
	{
		if (Phase == ERoundPhase::Lobby)
		{
			R.Rig.OpenLobby();
			R.Rig.SetLayout(2, 2);
			return;
		}
		if (Phase == ERoundPhase::Preparing)
		{
			R.Rig.OpenLobby();
			R.Rig.SetLayout(2, 2);
			R.Rig.EnterPreparing();
			return;
		}
		R.ToActive(2, 2);
		R.Disconnect(4);
		if (Phase == ERoundPhase::Active)
		{
			return;
		}
		R.ScriptRoundEnd();
		R.DrainOneTick({ R.Completion(1, 1, 0) });
		// A reset held for the dwell's end (it must not run after the session end).
		R.Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Reset);
		if (Phase == ERoundPhase::Ended)
		{
			return;
		}
		R.ScriptReset();
		R.Rig.Clock.AdvanceSeconds(RoundTestFixtures::EndedMinDwellSec + 0.5);
		R.Frame();
	}

	int32 CountSessionEnded(const FRoundEndRig& R)
	{
		return R.Rig.EventLog.Entries.FilterByPredicate([](const FRoundLogEntry& E)
		{
			return E.Type == ERoundLogType::WinLatched && E.Code == static_cast<uint16>(ERoundEndReason::SessionEnded);
		}).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSessionEndAndAbortDuringTravelTest, "StealthGame.Round.SessionEndAndAbortDuringTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundSessionEndAndAbortDuringTravelTest::RunTest(const FString& Parameters)
{
	using namespace SessionEndAndAbortDuringTravelTestData;

	// --- (1) OnSessionEnding in every phase ---------------------------------------------------------------------------
	for (const ERoundPhase Phase : { ERoundPhase::Lobby, ERoundPhase::Preparing, ERoundPhase::Active, ERoundPhase::Ended, ERoundPhase::Reset })
	{
		FRoundEndRig R;
		DriveTo(R, Phase);
		const TCHAR* Name = LexRoundPhase(Phase);
		TestTrue(FString::Printf(TEXT("%s: arranged"), Name), R.Rig.Core->GetPhase() == Phase);
		const uint32 Changes = R.Rig.Core->GetPhaseCore().GetPhaseChangeCount();
		const int32 Mark = R.Rig.Mark();

		// Act.
		R.Rig.Core->OnSessionEnding();
		R.Rig.Core->OnSessionEnding(); // a second notification is ignored

		// Assert.
		TestEqual(FString::Printf(TEXT("%s: one SessionEnded entry"), Name), CountSessionEnded(R), 1);
		TestTrue(FString::Printf(TEXT("%s: no phase change"), Name), R.Rig.Core->GetPhase() == Phase && R.Rig.Core->GetPhaseCore().GetPhaseChangeCount() == Changes);
		const int32 EntriesAfter = R.Rig.EventLog.Entries.Num();
		// Processing stops: well past every timer (grace 30 s, Preparing watchdog, Ended dwell, lobby travel bound).
		R.Frame(RoundTestFixtures::NoTimerJumpSec);
		R.Frame();
		TestTrue(FString::Printf(TEXT("%s: still no phase change"), Name), R.Rig.Core->GetPhase() == Phase && R.Rig.Core->GetPhaseCore().GetPhaseChangeCount() == Changes);
		TestEqual(FString::Printf(TEXT("%s: no timer fired, nothing logged"), Name), R.Rig.EventLog.Entries.Num(), EntriesAfter);
		TestTrue(FString::Printf(TEXT("%s: no queue item resolved"), Name), R.Queue->GetLastDrainRecords().Num() == 0);
		TestEqual(FString::Printf(TEXT("%s: no pending timer"), Name), R.Queue->GetPendingTimerCount(), 0);
		TestTrue(FString::Printf(TEXT("%s: host commands are refused"), Name),
			R.Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Reset) == ERoundCommandResult::Unavailable);
		TestEqual(FString::Printf(TEXT("%s: nothing released"), Name), R.Rig.CountSince(Mark, TEXT("ReleaseToCrowd")) + R.Rig.CountSince(Mark, TEXT("DestroyRoundPawns")), 0);
		TestEqual(FString::Printf(TEXT("%s: no mutating call at all"), Name), R.Rig.CallsSince(Mark).Num(), 0);
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Name), R.Rig.Log.Unexpected.IsEmpty());
	}

	// --- (2) a host abort before P1 -----------------------------------------------------------------------------------
	for (const bool bAtP1 : { true, false })
	{
		const TCHAR* Name = bAtP1 ? TEXT("P1 reported after the abort") : TEXT("Session cancels the travel");
		FRoundEndRig R;
		R.Rig.OpenLobby();
		R.Rig.SetLayout(2, 2);
		R.Rig.ScriptStartToTravel();
		R.Rig.Core->Start(FRoundExitBRig::HostCaller());
		TestTrue(FString::Printf(TEXT("%s: travelling (Lobby, travel pending)"), Name), R.Rig.Core->GetPhase() == ERoundPhase::Lobby && R.Rig.Core->IsTravelPending());
		const int32 Mark = R.Rig.Mark();
		R.Rig.Fakes.Session.Script(TEXT("CancelRoundTravel"));

		// Act: the abort, then P1 (the travel accepted) or Session's cancel.
		TestTrue(FString::Printf(TEXT("%s: the abort is executed (marked)"), Name),
			R.Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Abort) == ERoundCommandResult::Executed);
		TestEqual(FString::Printf(TEXT("%s: Session is asked to cancel the travel"), Name), R.Rig.CountSince(Mark, TEXT("CancelRoundTravel")), 1);
		TestTrue(FString::Printf(TEXT("%s: still Lobby until P1 or the cancel"), Name), R.Rig.Core->GetPhase() == ERoundPhase::Lobby);
		R.Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 2);
		R.Rig.Fakes.CharacterPool.Script(TEXT("DiscardDeal"));
		if (bAtP1)
		{
			R.Rig.Core->NotifyRoundTravelAccepted(RoundTestFixtures::PreparingRigSeed);
		}
		else
		{
			R.Rig.Core->NotifyRoundTravelCancelled();
		}

		// Assert.
		TestTrue(FString::Printf(TEXT("%s: Reset runs at once"), Name), R.Rig.Core->GetPhase() == ERoundPhase::Reset);
		TestEqual(FString::Printf(TEXT("%s: 0 pawns spawned"), Name), R.Rig.Fakes.Spawner.Spawns.Num(), 0);
		TestEqual(FString::Printf(TEXT("%s: the readiness sequence never began"), Name), R.Rig.CountSince(Mark, TEXT("SignalPreparingEntered")), 0);
		TestEqual(FString::Printf(TEXT("%s: RequestLobbyTravel"), Name), R.Rig.CountSince(Mark, TEXT("RequestLobbyTravel")), 1);
		TestTrue(FString::Printf(TEXT("%s: called off"), Name), R.Rig.Core->GetPhaseCore().WasLastRoundCalledOff());
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Name), R.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
