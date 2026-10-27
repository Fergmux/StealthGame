// Story nkp-003 (production/epics/npc-kill-penalty-informants/story-003-tally-rule-and-strike-result-supply.md), AC3 /
// AC-NKP-27 (PEN-R3, ADR-0019 section 2.2a): with tally 2 and LastResultSeq n, a reclaim records exactly one
// SetOwnerTally(P, 2, n, epoch) at the Roster owner-view feed seam and none for any other participant; across three
// ordinary strikes no owner write of the tally happens. Edge cases: the re-push also happens on a rejoin (OnViewRebound)
// and on OnRosterReset (0 and the new epoch). "No other connection receives it" on the wire is the Roster's audience
// (roster story psr-038) and story nkp-016.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyTallyRepushOnReclaimTest, "StealthGame.Penalty.TallyRepushOnReclaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyTallyRepushOnReclaimTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;

	// Three ordinary strikes (the third may be the exile's): no owner write of the tally, ever.
	{
		FRig Rig;
		Rig.NpcKill(P, 900);
		Rig.NpcKill(P, 901);
		Rig.NpcKill(Q, 902);
		TestEqual(TEXT("three strikes: zero SetOwnerTally calls"), Rig.OwnerView.Calls.Num(), 0);
	}

	// Arrange: t = 2 with LastResultSeq n (an informant kill then an NPC kill; Q has its own counters).
	FRig Rig;
	Rig.InformantKill(P, 900);
	Rig.NpcKill(P, 901);
	Rig.NpcKill(Q, 902);
	const FPenaltyRecord* Record = Rig.Core->FindRecord(P);
	if (!TestNotNull(TEXT("P has a record"), Record))
	{
		return false;
	}
	const FClientCommandSeq LastSeq = Record->LastResultSeq;
	TestEqual(TEXT("arranged t = 2"), static_cast<int32>(Record->WrongTargetTally), 2);
	TestEqual(TEXT("arranged: no owner write so far"), Rig.OwnerView.Calls.Num(), 0);

	// Act: P reclaims.
	Rig.Core->OnParticipantReclaimed(P);

	// Assert: exactly one write, P's, with (2, n) and the current epoch.
	TestEqual(TEXT("reclaim: exactly one SetOwnerTally"), Rig.OwnerView.Calls.Num(), 1);
	if (Rig.OwnerView.Calls.Num() == 1)
	{
		const FRecordingOwnerView::FCall& Call = Rig.OwnerView.Calls[0];
		TestTrue(TEXT("reclaim: the write is P's"), Call.Participant == P);
		TestEqual(TEXT("reclaim: tally 2"), static_cast<int32>(Call.Tally), 2);
		TestTrue(TEXT("reclaim: LastResultSeq n"), Call.LastResultSeq == LastSeq && LastSeq.Seq != 0);
		TestEqual(TEXT("reclaim: the current epoch"), Call.Epoch, FixtureEpoch);
	}
	TestEqual(TEXT("reclaim: nothing for Q"), Rig.OwnerView.CountFor(Q), 0);

	// Edge: a rejoin (OnViewRebound) re-pushes the same values.
	Rig.OwnerView.Calls.Reset();
	Rig.Core->OnViewRebound(P);
	TestTrue(TEXT("rejoin: one SetOwnerTally(P, 2, n)"), Rig.OwnerView.Calls.Num() == 1 && Rig.OwnerView.Calls[0].Participant == P
		&& Rig.OwnerView.Calls[0].Tally == 2 && Rig.OwnerView.Calls[0].LastResultSeq == LastSeq);

	// Edge: OnRosterReset re-pushes every participant at 0 with the new epoch.
	Rig.OwnerView.Calls.Reset();
	Rig.Core->OnRosterReset(FixtureEpoch + 1);
	TestEqual(TEXT("reset: one write per participant"), Rig.OwnerView.Calls.Num(), 2);
	for (const FRecordingOwnerView::FCall& Call : Rig.OwnerView.Calls)
	{
		TestTrue(TEXT("reset: tally 0, no sequence, the new epoch"), Call.Tally == 0 && Call.LastResultSeq == FClientCommandSeq() && Call.Epoch == FixtureEpoch + 1);
	}
	return true;
}

#endif
