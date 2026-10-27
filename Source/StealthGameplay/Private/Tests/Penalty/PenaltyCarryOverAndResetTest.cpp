// Story nkp-001 (production/epics/npc-kill-penalty-informants/story-001-penalty-core-store-and-service-shell.md), AC1 /
// AC-NKP-13 (PEN-R6): a participant with 2 strikes and tally 3, (a) reclaiming inside DisconnectGraceSec (still Active,
// same pawn) and (b) returning after grace expiry (a returning informant, readmitted), keeps 2 and 3 after the return and
// has 0 and 0 after OnRosterReset; a second participant Q is untouched by P's disconnect (keyed by ParticipantId); the
// reset clears the released bodies and the excluded pawn and returns LifeState to Active; k <= t throughout.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyCarryOverAndResetTest, "StealthGame.Penalty.CarryOverAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyCarryOverAndResetTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;

	for (const bool bAfterGrace : { false, true })
	{
		const TCHAR* Case = bAfterGrace ? TEXT("(b) return after grace expiry") : TEXT("(a) reclaim inside grace");

		// Arrange: P at k = 2, t = 3 (two NPC kills and an informant kill); Q at k = 1, t = 1.
		FRig Rig;
		Rig.NpcKill(P, 900);
		Rig.NpcKill(P, 901);
		Rig.InformantKill(P, 902);
		Rig.NpcKill(Q, 903);
		Rig.Core->RecordReleasedBody(Pawn(950), Rig.Clock.GetServerTimeSeconds());
		TestEqual(FString::Printf(TEXT("%s: arranged k = 2"), Case), static_cast<int32>(Rig.Core->GetStrikeCount(P)), 2);
		TestEqual(FString::Printf(TEXT("%s: arranged t = 3"), Case), static_cast<int32>(Rig.Core->GetWrongTargetTally(P)), 3);

		// Act: P disconnects and returns.
		Rig.Core->OnConnectionChanged(P, /*bConnected*/ false);
		TestTrue(FString::Printf(TEXT("%s: k <= t while disconnected"), Case), Rig.Core->GetStrikeCount(P) <= Rig.Core->GetWrongTargetTally(P));
		if (bAfterGrace)
		{
			Rig.Roster.States[P] = EParticipantState::Informant; // grace expiry: Round releases the pawn, P is absent
			Rig.Clock.AdvanceSeconds(60.0);
		}
		Rig.Core->OnConnectionChanged(P, /*bConnected*/ true);
		Rig.Core->OnParticipantReclaimed(P);
		if (bAfterGrace)
		{
			Rig.Core->OnInformantReadmitted(P);
		}

		// Assert: the round-scoped values carried over; Q untouched.
		TestEqual(FString::Printf(TEXT("%s: k = 2 after the return"), Case), static_cast<int32>(Rig.Core->GetStrikeCount(P)), 2);
		TestEqual(FString::Printf(TEXT("%s: t = 3 after the return"), Case), static_cast<int32>(Rig.Core->GetWrongTargetTally(P)), 3);
		TestEqual(FString::Printf(TEXT("%s: Q's k untouched"), Case), static_cast<int32>(Rig.Core->GetStrikeCount(Q)), 1);
		TestEqual(FString::Printf(TEXT("%s: Q's t untouched"), Case), static_cast<int32>(Rig.Core->GetWrongTargetTally(Q)), 1);

		// Act: OnRosterReset (a new epoch).
		Rig.Core->OnRosterReset(FixtureEpoch + 1);

		// Assert: cleared for the new epoch.
		const FPenaltyRecord* Record = Rig.Core->FindRecord(P);
		if (TestNotNull(FString::Printf(TEXT("%s: P keeps a record after the reset"), Case), Record))
		{
			TestEqual(FString::Printf(TEXT("%s: k = 0 after the reset"), Case), static_cast<int32>(Record->StrikeCount), 0);
			TestEqual(FString::Printf(TEXT("%s: t = 0 after the reset"), Case), static_cast<int32>(Record->WrongTargetTally), 0);
			TestTrue(FString::Printf(TEXT("%s: LifeState Active after the reset"), Case), Record->LifeState == EInformantLifeState::Active);
			TestFalse(FString::Printf(TEXT("%s: ExcludePawn cleared"), Case), Record->ExcludePawn.IsSet());
		}
		TestEqual(FString::Printf(TEXT("%s: Q cleared too"), Case), static_cast<int32>(Rig.Core->GetWrongTargetTally(Q)), 0);
		TestEqual(FString::Printf(TEXT("%s: released bodies cleared"), Case), Rig.Core->GetReleasedBodyCount(), 0);
		TestEqual(FString::Printf(TEXT("%s: the new epoch"), Case), Rig.Core->GetRoundEpoch(), FixtureEpoch + 1);
		const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({});
		TestTrue(FString::Printf(TEXT("%s: %s"), Case, *Teardown.Describe()), Teardown.IsClean());
	}
	return true;
}

#endif
