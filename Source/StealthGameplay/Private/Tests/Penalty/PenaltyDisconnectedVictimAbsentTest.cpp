// Story nkp-010 (production/epics/npc-kill-penalty-informants/story-010-absent-path-readmission-and-lost-body.md), AC1 /
// AC-NKP-48 (PEN-R8, cross-review S-B3; ADR-0019 sections 3.2, 3.3): GIVEN an Active participant with Connection =
// Disconnected (grace running, AI-driven pawn) assassinated at 100.0, WHEN the slot resolves, THEN Penalty is Absent with
// t_lost = 100.0 and no TimerFired(InformantRespawn) item exists; AND reclaimed at 110.0 -> T_due 115.0 (Awaiting); AND
// reclaimed instead at 130.0 -> Picking starts at once (the fake LSC is asked in that slot); AND the grace timer firing at
// 130.0 changes nothing in Penalty (Round's own Voided row is its integration run's to assert); AND GIVEN the same kill on
// an Unresponsive participant (the Roster projection collapses Unresponsive to Connected) THEN Awaiting with T_due 115.0.
// Edges: a reclaim at exactly T_due goes to Picking; t_floor is kept across the Absent period.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyDisconnectedVictimAbsentTestData
{
	inline constexpr double KillAt = 100.0;
	inline constexpr double StrikeContactSec = 0.40;

	/** P (team 0, disconnected unless bConnected) assassinated by Q at KillAt; Round's provider call after its check. */
	void AssassinateAt100(StealthPenaltyTest::FRig& Rig, bool bConnected)
	{
		using namespace StealthPenaltyTest;
		Rig.Roster.Teams.Add(P, ERosterTeam::Team0);
		if (!bConnected)
		{
			Rig.Roster.Disconnected.Add(P);
		}
		Rig.SetTime(KillAt);
		Rig.KillPlayer(P, Q, KillAt + StrikeContactSec);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::Assassinated, KillAt);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyDisconnectedVictimAbsentTest, "StealthGame.Penalty.DisconnectedVictimAbsent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyDisconnectedVictimAbsentTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyDisconnectedVictimAbsentTestData;

	// (i) the slot resolves: Absent, t_lost = 100.0, no item; (ii) reclaimed at 110.0: Awaiting, T_due 115.0.
	{
		FRig Rig(nullptr, false, /*bWithRespawnGateway*/ true);
		AssassinateAt100(Rig, /*bConnected*/ false);
		const FPenaltyRecord* Record = Rig.Core->FindRecord(P);
		TestTrue(TEXT("(i) Absent"), Rig.Core->GetLifeState(P) == EInformantLifeState::Absent);
		TestTrue(TEXT("(i) t_lost = 100.0"), Record != nullptr && Record->LostServerTime == KillAt);
		TestEqual(TEXT("(i) no TimerFired(InformantRespawn) item"), Rig.Respawns.Calls.Num(), 0);
		TestEqual(TEXT("(i) no RespawnScheduled row"), Rig.Rows<FPenaltyRespawnScheduledRow>().Num(), 0);
		const double KeptFloor = Record != nullptr ? Record->FloorServerTime : 0.0;
		TestTrue(TEXT("(i) the dying body's floor is kept (death write + fall + fade)"), KeptFloor > KillAt);

		Rig.SetTime(110.0);
		Rig.Roster.Disconnected.Remove(P);
		Rig.Core->OnInformantReadmitted(P);
		TestTrue(TEXT("(ii) reclaimed at 110.0: Awaiting"), Rig.Core->GetLifeState(P) == EInformantLifeState::Awaiting);
		TestEqual(TEXT("(ii) T_due 115.0 (kept t_lost, not the reclaim time)"), Rig.Core->GetDueServerTime(P).Get(0.0), 115.0);
		TestTrue(TEXT("(ii) one item at 115.0"), Rig.Respawns.PendingFor(P).Num() == 1 && Rig.Respawns.PendingFor(P)[0].DueServerTime == 115.0);
		TestEqual(TEXT("(ii) t_floor kept across the Absent period"), Rig.Core->FindRecord(P)->FloorServerTime, KeptFloor);
	}

	// (iii) reclaimed instead at 130.0: Picking starts at once (the pick runs in the readmission's slot); (iv) the grace timer
	// firing at 130.0 changes nothing in Penalty (Round voids it: P is already Informant).
	{
		FRig Rig(nullptr, false, true);
		AssassinateAt100(Rig, false);
		Rig.SetTime(130.0);
		Rig.Roster.Disconnected.Remove(P);
		Rig.Gateway.Picks.Add(FPenaltyRespawnPick()); // None: the request stays observable in Picking
		const FQueueKey ReadmissionSlot = Rig.Slot.CurrentKey();
		Rig.Core->OnInformantReadmitted(P);
		TestTrue(TEXT("(iii) reclaimed at 130.0: Picking at once"), Rig.Core->GetLifeState(P) == EInformantLifeState::Picking);
		TestEqual(TEXT("(iii) the pick ran in the readmission's call (one pick)"), Rig.Gateway.Count(TEXT("Pick")), 1);
		TestTrue(TEXT("(iii) still in the readmission's slot"), Rig.Slot.CurrentKey() == ReadmissionSlot);
		const FPenaltyRecord Before = *Rig.Core->FindRecord(P);
		// Round R17.4 for an already-Informant participant: its own logged Voided no-op; no Penalty call is made.
		const FPenaltyRecord& After = *Rig.Core->FindRecord(P);
		TestTrue(TEXT("(iv) the grace timer at 130.0 changes nothing in Penalty"), After.LifeState == Before.LifeState && After.DueServerTime == Before.DueServerTime
			&& After.RespawnRequestSeq == Before.RespawnRequestSeq && After.LostServerTime == Before.LostServerTime);
	}

	// Edge: a reclaim at exactly T_due (115.0) goes to Picking.
	{
		FRig Rig(nullptr, false, true);
		AssassinateAt100(Rig, false);
		Rig.SetTime(115.0);
		Rig.Roster.Disconnected.Remove(P);
		Rig.Gateway.Picks.Add(FPenaltyRespawnPick());
		Rig.Core->OnInformantReadmitted(P);
		TestTrue(TEXT("edge: a reclaim at exactly T_due -> Picking"), Rig.Core->GetLifeState(P) == EInformantLifeState::Picking);
	}

	// (v) the same kill on an Unresponsive participant: connected in the Roster projection -> Awaiting, T_due 115.0.
	{
		FRig Rig(nullptr, false, true);
		AssassinateAt100(Rig, /*bConnected: Unresponsive collapses to Connected*/ true);
		TestTrue(TEXT("(v) Unresponsive: Awaiting"), Rig.Core->GetLifeState(P) == EInformantLifeState::Awaiting);
		TestEqual(TEXT("(v) Unresponsive: T_due 115.0"), Rig.Core->GetDueServerTime(P).Get(0.0), 115.0);
		TestEqual(TEXT("(v) Unresponsive: one item"), Rig.Respawns.PendingFor(P).Num(), 1);
	}
	return true;
}

#endif
