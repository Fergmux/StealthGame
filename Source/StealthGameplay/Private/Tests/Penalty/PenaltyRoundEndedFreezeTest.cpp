// Story nkp-001 (production/epics/npc-kill-penalty-informants/story-001-penalty-core-store-and-service-shell.md), AC4
// (derived, ADR-0019 section 3.3 "any" rows; the RoundEnded freeze): P at k = 1, t = 2; a tally write inside the deciding
// slot before OnRosterRoundEnded still succeeds; after OnRosterRoundEnded a strike, a GetTallyForResult(WrongTarget) and a
// released-body record are no-ops, k, t and LifeState (RoundEnded) are unchanged; a second OnRosterRoundEnded is
// idempotent; OnRosterReset unfreezes. The respawn-schedule half (OnInformantAwaitingBody and the item resolve returning
// Rejected_Latched, pending-item count 0) needs story nkp-009's provider and resolver and is asserted there.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyRoundEndedFreezeTest, "StealthGame.Penalty.RoundEndedFreeze",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyRoundEndedFreezeTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;

	// Arrange: P at k = 1, t = 2.
	FRig Rig;
	Rig.NpcKill(P, 900);
	Rig.InformantKill(P, 901);

	// The deciding slot: Q's tally write lands before the round end (the exiling kill's result needs TallyAfter).
	const FStrikeTally Deciding = Rig.NpcKill(Q, 902);
	TestEqual(TEXT("a tally write in the deciding slot before the round end succeeds"), static_cast<int32>(Deciding.TallyAfter), 1);

	// Act: the round ends (fired after the deciding slot's Resolve returned), then mutations are attempted.
	Rig.Core->OnRosterRoundEnded();
	EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::Strike;
	const FStrikeTally AfterEnd = Rig.Attack(P, FPenaltyKill{ Pawn(903), P, EVictimKind::Npc, EKillCause::Strike }, EStrikeResult::WrongTarget, &Outcome);
	Rig.Core->RecordReleasedBody(Pawn(904), Rig.Clock.GetServerTimeSeconds());
	Rig.Core->OnRosterRoundEnded(); // idempotent

	// Assert: frozen.
	TestTrue(TEXT("the store is frozen"), Rig.Core->IsFrozen());
	TestTrue(TEXT("a strike after the round end is a no-op"), Outcome == EPenaltyKillOutcome::NoStrike);
	TestEqual(TEXT("k unchanged"), static_cast<int32>(Rig.Core->GetStrikeCount(P)), 1);
	TestEqual(TEXT("t unchanged (the tally query answers the frozen value)"), static_cast<int32>(AfterEnd.TallyAfter), 2);
	TestEqual(TEXT("t unchanged in the store"), static_cast<int32>(Rig.Core->GetWrongTargetTally(P)), 2);
	TestFalse(TEXT("no exile after the round end"), AfterEnd.bExiled);
	TestEqual(TEXT("no released body recorded after the round end"), Rig.Core->GetReleasedBodyCount(), 0);
	const FPenaltyRecord* Record = Rig.Core->FindRecord(P);
	TestTrue(TEXT("LifeState RoundEnded"), Record != nullptr && Record->LifeState == EInformantLifeState::RoundEnded);

	// OnRosterReset unfreezes: the next round counts again.
	Rig.Core->OnRosterReset(FixtureEpoch + 1);
	TestFalse(TEXT("the reset unfreezes"), Rig.Core->IsFrozen());
	Rig.NpcKill(P, 905);
	TestEqual(TEXT("a strike counts after the reset"), static_cast<int32>(Rig.Core->GetStrikeCount(P)), 1);
	const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({});
	TestTrue(Teardown.Describe(), Teardown.IsClean());
	return true;
}

#endif
