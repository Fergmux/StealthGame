// Story rmf-060 (production/epics/round-match-framework/story-060-grace-timeline.md), GDD AC 22 / R17.1, R17.2, R17.4, F4:
// with a fake clock and an Active participant: a client silent at t is taken over when Session raises Unresponsive,
// inside [t + 3.0, t + 3.3] s (Session F2's L_min..L_max, both endpoints); a quit at t is taken over at t; the
// participant becomes an Informant (Disconnect) at exactly DisconnectedAt + G_eff, with ReleaseToCrowd in the same slot
// and GraceExpired logged; a kill on the pawn before the expiry is a Player kill (the pawn is still bound: no release)
// and the grace timer then fires as a logged no-op (Voided(TimerFired, NoOp_AlreadyInState)); after the expiry the pawn
// was released to the crowd, so a kill resolves Npc. The victim kind is the Roster's classification, modelled here from
// the fake's record of ReleaseToCrowd. The [PIE-2] +/-1 s smoke is covered by the batch's run-and-observe session.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundFormulas.h"
#include "StealthRoundEndRig.h"

namespace GraceTimelineTestData
{
	constexpr uint32 Away = 2; // team 0

	/** The Roster's victim kind for a kill on Away's pawn now: Player while bound, Npc once released to the crowd. */
	const TCHAR* VictimKind(const FRoundEndRig& R)
	{
		return R.Rig.CountSince(0, TEXT("ReleaseToCrowd")) > 0 ? TEXT("Npc") : TEXT("Player");
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundGraceTimelineTest, "StealthGame.Round.GraceTimeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundGraceTimelineTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace GraceTimelineTestData;
	const double GEff = StealthRoundFormulas::EffectiveGraceSec(GraceDisconnectSec, GraceRejoinWindowSec);

	// --- Silent drop: the takeover at Session's Unresponsive, at both F2 endpoints -----------------------------------
	for (const double Detect : { SoftDetectMinSec, SoftDetectMaxSec })
	{
		FRoundEndRig R;
		R.ToActive(2, 2);
		const double T = R.Rig.Clock.GetServerTimeSeconds();
		R.RunUntil(T + Detect - RigFrameSec);
		TestEqual(TEXT("silent: no takeover before Unresponsive"), R.Rig.CountSince(0, TEXT("RequestTakeover")), 0);
		R.RunUntil(T + Detect);
		R.Rig.Core->OnParticipantUnresponsive(FParticipantId(Away), R.Rig.Clock.GetServerTimeSeconds());
		const double TakeoverAt = R.Rig.Clock.GetServerTimeSeconds();
		TestEqual(TEXT("silent: one takeover request"), R.Rig.CountSince(0, TEXT("RequestTakeover")), 1);
		TestTrue(FString::Printf(TEXT("silent (%.1f s): the takeover in [t + 3.0, t + 3.3]"), Detect),
			TakeoverAt >= T + SoftDetectMinSec - 1e-6 && TakeoverAt <= T + SoftDetectMaxSec + 1e-6);
		TestTrue(TEXT("silent: AI-driven, still Active (no state change)"), R.Rig.Core->IsAiDriven(FParticipantId(Away)) && R.IsActive(Away));
		TestEqual(TEXT("silent: no grace record before Session's Disconnected"), R.Rig.Core->GetGraceTable().Num(), 0);
	}

	// --- Quit: the takeover at t; the conversion at exactly DisconnectedAt + G_eff -----------------------------------
	{
		FRoundEndRig R;
		R.ToActive(2, 2);
		R.RunUntil(R.Rig.Clock.GetServerTimeSeconds() + DisconnectAfterActiveSec);
		const double T = R.Rig.Clock.GetServerTimeSeconds();
		const int32 Mark = R.Rig.Mark();
		R.Disconnect(Away);
		TestEqual(TEXT("quit: the takeover at t"), R.Rig.CountSince(Mark, TEXT("RequestTakeover")), 1);
		const FRoundGraceRecord* Record = R.Rig.Core->GetGraceTable().Find(FParticipantId(Away));
		TestTrue(TEXT("quit: GraceExpiry = DisconnectedAt + G_eff"), Record != nullptr && FMath::IsNearlyEqual(Record->GraceExpiry, T + GEff, 1e-9));
		R.Rig.Fakes.Roster.Script(TEXT("RequestTransition"), static_cast<int64>(ERosterResult::Applied));
		R.Rig.Fakes.Roster.Script(TEXT("ReleaseToCrowd"), static_cast<int64>(ERosterResult::Applied));
		R.RunUntil(T + GEff - RigFrameSec);
		TestEqual(TEXT("quit: no conversion before GraceExpiry"), R.Rig.CountSince(Mark, TEXT("RequestTransition")), 0);
		TestTrue(TEXT("before expiry: a kill would be a Player kill"), FCString::Strcmp(VictimKind(R), TEXT("Player")) == 0);
		R.RunUntil(T + GEff);
		TestTrue(TEXT("at GraceExpiry: Informant (Disconnect)"), R.Member(Away)->State == EParticipantState::Informant);
		const int32 TransitionIndex = R.IndexOf(TEXT("RequestTransition"), Mark);
		const int32 ReleaseIndex = R.IndexOf(TEXT("ReleaseToCrowd"), Mark);
		TestTrue(TEXT("the guarded Disconnect transition"), TransitionIndex != INDEX_NONE && R.Rig.Log.Calls[TransitionIndex].Args[2] == static_cast<int64>(ERosterReason::Disconnect)
			&& R.Rig.Log.Calls[TransitionIndex].Args[4] >= 0);
		bool bNothingBetween = ReleaseIndex > TransitionIndex;
		for (int32 Index = TransitionIndex + 1; bNothingBetween && Index < ReleaseIndex; ++Index)
		{
			bNothingBetween = FRoundExitBRig::IsQuery(R.Rig.Log.Calls[Index].Call); // only F2's count queries in between
		}
		TestTrue(TEXT("ReleaseToCrowd in the same call stack, right after the Applied transition"), bNothingBetween);
		const TArray<FRoundLogEntry> Expired = R.Logged(ERoundLogType::GraceExpired);
		TestTrue(TEXT("GraceExpired logged at exactly DisconnectedAt + G_eff"), Expired.Num() == 1 && FMath::IsNearlyEqual(Expired[0].ServerTime, T + GEff, 1e-6));
		TestTrue(TEXT("after expiry: a kill resolves Npc"), FCString::Strcmp(VictimKind(R), TEXT("Npc")) == 0);
		TestTrue(TEXT("one conversion does not end a 2v2"), R.Rig.Core->GetPhase() == ERoundPhase::Active);
		TestTrue(TEXT("quit: no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	}

	// --- A kill before the expiry: Player kill; the timer then fires as a logged no-op ----------------------------------
	{
		FRoundEndRig R;
		R.ToActive(2, 2);
		const double T = R.Rig.Clock.GetServerTimeSeconds();
		R.Disconnect(Away);
		R.DrainOneTick({ R.Strike(1, 3, Away) }); // the AI-driven, still bound pawn is struck
		TestTrue(TEXT("kill before expiry: a Player kill (bound, not released)"), R.Member(Away)->State == EParticipantState::Informant
			&& FCString::Strcmp(VictimKind(R), TEXT("Player")) == 0);
		R.Rig.Fakes.Roster.Script(TEXT("RequestTransition"), static_cast<int64>(ERosterResult::NoOp_AlreadyInState));
		R.RunUntil(T + GEff);
		const FRoundLogEntry* NoOp = R.Rig.EventLog.Entries.FindByPredicate([](const FRoundLogEntry& E)
		{
			return E.Type == ERoundLogType::Voided && E.Data[0] == static_cast<uint32>(EQueueItemType::TimerFired)
				&& E.Data[1] == static_cast<uint32>(ERosterResult::NoOp_AlreadyInState);
		});
		TestNotNull(TEXT("the grace timer fires as a logged no-op: Voided(TimerFired, NoOp_AlreadyInState)"), NoOp);
		TestEqual(TEXT("never logged GraceExpired"), R.CountLogged(ERoundLogType::GraceExpired), 0);
		TestEqual(TEXT("no release for a no-op"), R.Rig.CountSince(0, TEXT("ReleaseToCrowd")), 0);
		TestTrue(TEXT("kill: no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
