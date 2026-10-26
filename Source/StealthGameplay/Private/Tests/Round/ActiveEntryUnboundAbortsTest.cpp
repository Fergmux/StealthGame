// Story rmf-041 (production/epics/round-match-framework/story-041-active-entry-unbound-aborts.md), GDD AC 42 / R7, R8:
//   - GetUnboundActiveParticipants() non-empty at Active entry (the first drain at or after ActiveAtServerTime) aborts
//     BindFailed through the one Preparing abort path: the phase never becomes Active, OnRoundActive never fires, no
//     Crowd start, no ActivePhaseStart stamp, RoundsPlayedCount unchanged;
//   - with an empty unbound set Active is entered normally.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundActiveEntryUnboundAbortsTest, "StealthGame.Round.ActiveEntryUnboundAborts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundActiveEntryUnboundAbortsTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	for (const bool bUnbound : { true, false })
	{
		const TCHAR* Name = bUnbound ? TEXT("one unbound participant") : TEXT("empty unbound set");
		FRoundExitBRig Rig;
		int32 ActiveEvents = 0;
		Rig.Core->OnRoundActive = [&ActiveEvents](uint32, double) { ++ActiveEvents; };
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		const uint32 PlayedBefore = Rig.Core->GetPhaseCore().GetRoundsPlayedCount();
		Rig.EnterPreparing();
		Rig.ScriptPreparingSequence();
		Rig.Core->NotifyTuningReady(PreparingRigTuningHash);
		Rig.Core->NotifyLayoutReady(1, Rig.Core->GetPreparing().GetSeed());
		Rig.AckAll(false);
		Rig.Frame();
		Rig.AckAll(true);
		Rig.Frame();
		TestTrue(FString::Printf(TEXT("%s: the start is scheduled"), Name), Rig.Core->GetPreparing().GetStep() == ERoundPreparingStep::AwaitActiveAt);

		// Arrange the guard's answer (the fake Roster leaves one participant without a live bound pawn, Roster I4).
		if (bUnbound)
		{
			Rig.Fakes.Roster.Script(TEXT("GetUnboundActiveParticipants"), 1);
			Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"));
			Rig.Fakes.CharacterPool.Script(TEXT("DiscardDeal"));
			Rig.Fakes.Session.Script(TEXT("RequestLobbyTravel"));
		}
		else
		{
			Rig.ScriptActiveEntry();
		}

		// Act: the first drain at or after ActiveAtServerTime.
		Rig.Frame(Rig.Core->GetParams().Preparing.StartLeadSec);

		// Assert.
		if (bUnbound)
		{
			TestTrue(TEXT("Active is never entered (aborted into Reset)"), Rig.Core->GetPhase() == ERoundPhase::Reset);
			TestEqual(TEXT("OnRoundActive never fired"), ActiveEvents, 0);
			TestEqual(TEXT("no Crowd start"), Rig.CountSince(0, TEXT("StartRound")), 0);
			TestEqual(TEXT("no ActivePhaseStart stamp"), Rig.Core->GetActivePhaseStart(), 0.0);
			TestEqual(TEXT("RoundsPlayedCount unchanged"), Rig.Core->GetPhaseCore().GetRoundsPlayedCount(), PlayedBefore);
			const TArray<FRoundArchivedSlice>& Archive = Rig.Core->GetLogArchive().GetArchive();
			TestTrue(TEXT("the abort is BindFailed (PreparingFailed)"), Archive.Num() == 1 && Archive[0].EndReason == ERoundEndReason::PreparingFailed
				&& Archive[0].AbortCode == static_cast<uint8>(ERoundPreparingAbortCode::BindFailed));
		}
		else
		{
			TestTrue(TEXT("empty unbound set: Active is entered normally"), Rig.Core->GetPhase() == ERoundPhase::Active);
			TestEqual(TEXT("empty unbound set: OnRoundActive fired once"), ActiveEvents, 1);
		}
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Name), Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
