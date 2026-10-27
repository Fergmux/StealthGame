// Story nkp-009 (production/epics/npc-kill-penalty-informants/story-009-respawn-due-time-scheduling-and-cancellation.md),
// AC2 / AC-NKP-25 (PEN-R11, F2): GIVEN an embodied informant that disconnects at t and reclaims at t + 5 s, WHEN its
// respawn is scheduled, THEN T_due = t + InformantRespawnDelaySec, and the body it held was lost (released) at t: Penalty
// records t_lost = t_floor = t for the Embodied -> Absent row (the release itself is Round R17.5 / Roster psr-030's, not
// Penalty's). Edges: the delay is never restarted at the reclaim (a reclaim at t + 5 does not give t + 5 + D) and never
// skipped (a reclaim after T_due, at t + 20, re-pawns at the reclaim time, never earlier).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyDisconnectNeverShortensTestData
{
	/** t: the disconnect time (any time after the embodiment). */
	inline constexpr double DisconnectAt = 200.0;
	inline constexpr double EarlyReclaimSec = 5.0;
	inline constexpr double LateReclaimSec = 20.0;

	/** P embodied through one committed respawn (StrikeLimit at 150.0, the fake gateway's pawn). */
	void Embody(StealthPenaltyTest::FRig& Rig, FParticipantId P)
	{
		Rig.SetTime(150.0);
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, 150.0);
		Rig.Fire(P);
		Rig.Roster.LivePawns.Add(P);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyDisconnectNeverShortensTest, "StealthGame.Penalty.DisconnectNeverShortens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyDisconnectNeverShortensTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyDisconnectNeverShortensTestData;

	for (const double ReclaimAfter : { EarlyReclaimSec, LateReclaimSec })
	{
		// Arrange: an embodied informant.
		FRig Rig(nullptr, false, /*bWithRespawnGateway*/ true);
		Embody(Rig, P);
		TestTrue(TEXT("fixture: embodied"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied);
		const double D = Rig.Core->GetLatchedParams().InformantRespawnDelaySec;

		// Act: disconnect at t (the pawn released in that slot), reclaim later.
		Rig.SetTime(DisconnectAt);
		Rig.Roster.Disconnected.Add(P);
		Rig.Core->OnConnectionChanged(P, false);
		Rig.Roster.LivePawns.Remove(P);
		const FPenaltyRecord* Absent = Rig.Core->FindRecord(P);
		TestTrue(TEXT("the disconnect makes P Absent with no item"), Rig.Core->GetLifeState(P) == EInformantLifeState::Absent && Rig.Respawns.PendingFor(P).Num() == 0);
		TestTrue(TEXT("the body was lost at t: t_lost = t_floor = t"), Absent != nullptr && Absent->LostServerTime == DisconnectAt && Absent->FloorServerTime == DisconnectAt);

		Rig.SetTime(DisconnectAt + ReclaimAfter);
		Rig.Roster.Disconnected.Remove(P);
		Rig.Gateway.Picks.Add(FPenaltyRespawnPick()); // a late reclaim's pick answers None: its T_due stays observable
		++Rig.Gateway.Frame;
		Rig.Core->OnInformantReadmitted(P);

		// Assert.
		if (ReclaimAfter < D)
		{
			TestEqual(TEXT("reclaim at t + 5: T_due = t + InformantRespawnDelaySec (never restarted at the reclaim)"), Rig.Core->GetDueServerTime(P).Get(0.0), DisconnectAt + D);
			TestTrue(TEXT("reclaim at t + 5: Awaiting with one item at that T_due"), Rig.Core->GetLifeState(P) == EInformantLifeState::Awaiting
				&& Rig.Respawns.PendingFor(P).Num() == 1 && Rig.Respawns.PendingFor(P)[0].DueServerTime == DisconnectAt + D);
		}
		else
		{
			TestEqual(TEXT("reclaim at t + 20 (after T_due): re-pawned at the reclaim time, never earlier (never skipped)"), Rig.Core->GetDueServerTime(P).Get(0.0), DisconnectAt + ReclaimAfter);
			TestTrue(TEXT("reclaim after T_due: Picking at once"), Rig.Core->GetLifeState(P) == EInformantLifeState::Picking);
		}
		TestEqual(TEXT("no strike and no tally change across the disconnect"), static_cast<int32>(Rig.Core->GetStrikeCount(P)) + Rig.Core->GetWrongTargetTally(P), 0);
	}
	return true;
}

#endif
