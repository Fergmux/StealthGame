// Story rmf-065 (production/epics/round-match-framework/story-065-forfeit-void-cases.md), GDD AC 24b / R15 (b), (c), R8:
// team 1's last Active member converts at its grace expiry, (a) with every Active member of team 0 disconnected too:
// Void(Abandoned): phase Reset, no score, no FRoundResult, bCalledOff; (b) when that last conversion was a host kick:
// Void(KickedLastActive), whether or not team 0 has anyone Connected; (c) decision D32: a kick on the live grace record in
// the frame of its expiry is processed first (the record is marked kicked, no second timer), so the same conversion
// gives Void(KickedLastActive) and cannot flip on frame order. A void stages no RoundEndStep and RoundsPlayedCount keeps
// its Active-entry increment.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

namespace ForfeitVoidCasesTestData
{
	enum class ECase : uint8
	{
		Abandoned,
		Kicked,
		KickInExpiryFrame,
	};

	/** The void, from 2v2 Active (3 already assassinated; 4 is team 1's last Active member). */
	void Run(FAutomationTestBase& Test, ECase Case, ERoundEndReason Expected, const TCHAR* Name)
	{
		using namespace RoundTestFixtures;
		FRoundEndRig R;
		R.ToActive(2, 2);
		R.Transition(3, EParticipantState::Informant, ERosterReason::Assassinated);
		R.Disconnect(4, Case == ECase::Kicked ? ERoundDisconnectReason::Kicked : ERoundDisconnectReason::Quit);
		const double Expiry = R.Rig.Core->GetGraceTable().Find(FParticipantId(4))->GraceExpiry;
		if (Case == ECase::Abandoned)
		{
			// Every Active member of team 0 is disconnected too (their own expiries come later than 4's).
			R.Frame();
			R.Disconnect(1);
			R.Disconnect(2);
		}
		const FRoundPublicState Before = R.Rig.Core->GetPhaseCore().MakePublicState();
		const uint32 Played = R.Rig.Core->GetPhaseCore().GetRoundsPlayedCount();
		R.Rig.Fakes.Roster.Script(TEXT("RequestTransition"), static_cast<int64>(ERosterResult::Applied));
		R.Rig.Fakes.Roster.Script(TEXT("ReleaseToCrowd"), static_cast<int64>(ERosterResult::Applied));
		R.ScriptReset();
		R.RunUntil(Expiry - RigFrameSec);
		if (Case == ECase::KickInExpiryFrame)
		{
			// The kick lands in the expiry's frame, before the drain (Session events precede the queue's drain).
			R.Rig.Clock.Advance(RigFrameSec, 1);
			R.Rig.Core->OnParticipantDisconnected(FParticipantId(4), ERoundDisconnectReason::Kicked, R.Rig.Clock.GetServerTimeSeconds());
			Test.TestEqual(FString::Printf(TEXT("%s: no second timer"), Name), R.Queue->GetPendingTimerCount(), 1);
			Test.TestTrue(FString::Printf(TEXT("%s: the live record is marked kicked"), Name), R.Rig.Core->GetGraceTable().Find(FParticipantId(4))->bKicked);
			R.Rig.Core->Tick();
			R.Queue->Drain();
		}
		else
		{
			R.RunUntil(Expiry);
		}

		const FRoundPublicState After = R.Rig.Core->GetPhaseCore().MakePublicState();
		const TArray<FRoundLogEntry> Latched = R.Logged(ERoundLogType::WinLatched);
		Test.TestTrue(FString::Printf(TEXT("%s: the void's WinLatched"), Name), Latched.Num() == 1 && Latched[0].Code == static_cast<uint16>(Expected));
		Test.TestTrue(FString::Printf(TEXT("%s: phase Reset (no Ended)"), Name), After.Phase == ERoundPhase::Reset);
		Test.TestTrue(FString::Printf(TEXT("%s: no score"), Name), After.MatchScore[0] == Before.MatchScore[0] && After.MatchScore[1] == Before.MatchScore[1]);
		Test.TestTrue(FString::Printf(TEXT("%s: no FRoundResult"), Name), After.LastResult == Before.LastResult);
		Test.TestTrue(FString::Printf(TEXT("%s: bCalledOff"), Name), After.bCalledOff);
		Test.TestEqual(FString::Printf(TEXT("%s: RoundsPlayedCount kept"), Name), R.Rig.Core->GetPhaseCore().GetRoundsPlayedCount(), Played);
		Test.TestEqual(FString::Printf(TEXT("%s: no round-end step"), Name), R.Rig.CountSince(0, TEXT("EndRoundAll")), 0);
		Test.TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Name), R.Rig.Log.Unexpected.IsEmpty());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundForfeitVoidCasesTest, "StealthGame.Round.ForfeitVoidCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundForfeitVoidCasesTest::RunTest(const FString& Parameters)
{
	using namespace ForfeitVoidCasesTestData;
	Run(*this, ECase::Abandoned, ERoundEndReason::VoidAbandoned, TEXT("every team 0 member disconnected"));
	Run(*this, ECase::Kicked, ERoundEndReason::VoidKickedLastActive, TEXT("the last conversion was a host kick"));
	Run(*this, ECase::KickInExpiryFrame, ERoundEndReason::VoidKickedLastActive, TEXT("D32: a kick in the expiry's frame"));
	return true;
}

#endif
