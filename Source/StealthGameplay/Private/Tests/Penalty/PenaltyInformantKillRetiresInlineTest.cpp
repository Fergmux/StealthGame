// Story nkp-008 (production/epics/npc-kill-penalty-informants/story-008-informant-kill-inline-retire.md), AC1 /
// AC-NKP-15 (PEN-R7; ADR-0019 section 2.1; decision D10): GIVEN two strikes on the same embodied informant in one drain,
// WHEN both resolve in key order, THEN the first is a kill (VictimKind = Informant) whose binding Penalty retires inline in
// the slot (RetireBinding, then the victim's respawn scheduled once with cause InformantKilled at the slot time) and the
// second resolves NotTargetable(AlreadyResolved): NoContact, no tally, no strike, no second OnPawnKilled and no second
// respawn. Different attackers in one drain (the real queue, strike resolver and Round core, PenaltyEncounterRig.h), and
// the same attacker again once its lockout ended, while the body still falls.
// The Roster half on the REAL Roster core (PenaltyRealRosterRig.h): the informant kill retires the binding without a
// transition: the state stays Informant, no Roster event, EventSeq unchanged, the body resolves AlreadyResolved and the
// tombstone carries the death write's time (RetireAtServerTime).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyEncounterRig.h"
#include "Tests/Penalty/PenaltyRealRosterRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyInformantKillRetiresInlineTest, "StealthGame.Penalty.InformantKillRetiresInline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyInformantKillRetiresInlineTest::RunTest(const FString& Parameters)
{
	// --- The drain: two strikes on one informant, different attackers, then the same attacker after its lockout. ---
	{
		using namespace StealthPenaltyEncounterTest;
		const FParticipantId First(1);
		const FParticipantId Second(2);
		const FParticipantId Informant(4);
		FPenaltyEncounterRig Rig(TEXT("PenaltyInformantTwoStrikes"));
		Rig.R.SetInformant(Informant.Value);
		APawn* A1 = Rig.R.Spawn(FVector::ZeroVector, First.Value);
		APawn* A2 = Rig.R.Spawn(FVector(300.0, 0.0, 0.0), Second.Value, 180.0);
		APawn* Body = Rig.R.Spawn(FVector(150.0, 0.0, 0.0), Informant.Value);

		Rig.R.Drain({ Rig.R.AttackAt(A1, Body), Rig.R.AttackAt(A2, Body) });
		const FQueueSlotRecord* FirstSlot = Rig.R.CommandRecord(0);
		const TArray<FAttackResolution>& Res = Rig.R.Core->GetResolutions();

		TestTrue(TEXT("the first strike kills the informant (VictimKind = Informant, WrongTarget)"), Res.Num() == 2 && Res[0].Victim == Body
			&& Res[0].FallenKind == EVictimKind::Informant && Res[0].StrikeResult.Result == EStrikeResult::WrongTarget);
		TestTrue(TEXT("the binding is retired inline in the first slot"), Rig.Bodies.Count(TEXT("RetireBinding")) == 1 && FirstSlot != nullptr
			&& Rig.Bodies.Calls.ContainsByPredicate([FirstSlot, Body](const FRigBodies::FCall& C) { return C.Name == TEXT("RetireBinding") && C.Body == Body && C.SlotKey == FirstSlot->Key; }));
		TestTrue(TEXT("the body now resolves NotTargetable(AlreadyResolved)"), Rig.R.Roster.ResolveVictimKind(Body)
			== FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::AlreadyResolved));
		TestTrue(TEXT("the second strike is NoContact"), Res.Num() == 2 && Res[1].Attacker == A2 && Res[1].Victim == nullptr && Res[1].StrikeResult.Result == EStrikeResult::NoContact);
		TestEqual(TEXT("one OnPawnKilled"), Rig.R.Kills.Num(), 1);
		TestEqual(TEXT("the first attacker's tally + 1 (WrongTarget)"), static_cast<int32>(Rig.Penalty->GetWrongTargetTally(First)), 1);
		TestEqual(TEXT("no tally for the NoContact"), static_cast<int32>(Rig.Penalty->GetWrongTargetTally(Second)), 0);
		TestEqual(TEXT("no strike for either attacker"), Rig.Penalty->GetStrikeCount(First) + Rig.Penalty->GetStrikeCount(Second), 0);
		if (TestEqual(TEXT("the victim's respawn scheduled once"), Rig.Respawns.Calls.Num(), 1))
		{
			TestTrue(TEXT("respawn: the informant participant, cause InformantKilled"), Rig.Respawns.Calls[0].Participant == Informant
				&& Rig.Respawns.Calls[0].Cause == ERespawnCause::InformantKilled);
		}
		TestTrue(TEXT("the informant participant stays Informant (not a transition)"), Rig.RosterQuery.GetState(Informant).Get(EParticipantState::Active) == EParticipantState::Informant);

		// The same attacker once its lockout ended: the body is still Dead (falling); NoContact again, nothing counted.
		Rig.SettleLockout();
		Rig.R.Drain({ Rig.R.AttackAt(A1, Body) });
		TestTrue(TEXT("same attacker, later: NoContact"), Rig.R.Core->GetResolutions().Last().Attacker == A1 && Rig.R.Core->GetResolutions().Last().StrikeResult.Result == EStrikeResult::NoContact);
		TestEqual(TEXT("same attacker, later: tally unchanged"), static_cast<int32>(Rig.Penalty->GetWrongTargetTally(First)), 1);
		TestEqual(TEXT("same attacker, later: still one retire and one respawn"), Rig.Bodies.Count(TEXT("RetireBinding")) + Rig.Respawns.Calls.Num(), 2);
		TestEqual(TEXT("no Penalty fault"), Rig.PenaltyFaults.GetRecords().Num(), 0);
	}

	// --- The Roster half on the real Roster core. ---
	{
		using namespace StealthPenaltyRealRoster;
		FRig Rig;
		const int32 Victim = 3; // team 1
		const FParticipantId VictimId = Rig.Ids[Victim];
		// Make the victim an informant with a body: assassinated (its body retired), then a fresh body bound.
		FRosterTransitionOptions Options;
		Options.ExpectedEpoch = Rig.Roster->GetEpochToken();
		TestTrue(TEXT("fixture: the victim becomes an informant"), Rig.Roster->RequestTransition(VictimId, EParticipantState::Informant, ERosterReason::Assassinated, Options) == ERosterResult::Applied);
		const FPenaltyPawnKey InformantBody(200);
		TestTrue(TEXT("fixture: the informant's body binds"), Rig.Roster->BindPawn(VictimId, HandleOf(InformantBody)) == ERosterResult::Applied);
		const uint64 SeqBefore = Rig.Roster->GetEventSeq();
		const int32 EventsBefore = Rig.Events.Events.Num();
		TestTrue(TEXT("fixture: the body resolves Informant"), Rig.Roster->ResolveVictimKind(HandleOf(InformantBody)) == FRosterVictimKind::Make(ERosterVictimKind::Informant));

		EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::Strike;
		const FStrikeTally Tally = Rig.KillBody(0, InformantBody, &Outcome);

		TestTrue(TEXT("real Roster: no strike for an informant kill"), Outcome == EPenaltyKillOutcome::NoStrike && Rig.Penalty->GetStrikeCount(Rig.Ids[0]) == 0);
		TestEqual(TEXT("real Roster: the tally follows WrongTarget"), static_cast<int32>(Tally.TallyAfter), 1);
		TestTrue(TEXT("real Roster: the body resolves AlreadyResolved"), Rig.Roster->ResolveVictimKind(HandleOf(InformantBody))
			== FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::AlreadyResolved));
		TestTrue(TEXT("real Roster: the state stays Informant"), Rig.Roster->FindRecord(VictimId)->State == EParticipantState::Informant);
		TestTrue(TEXT("real Roster: no event, no EventSeq"), Rig.Roster->GetEventSeq() == SeqBefore && Rig.Events.Events.Num() == EventsBefore);
		TestFalse(TEXT("real Roster: the informant awaits a body (no live pawn)"), Rig.Roster->FindRecord(VictimId)->HasLivePawn());
		const FRosterBindingEntry* Tombstone = Rig.Roster->FindBinding(HandleOf(InformantBody));
		const int64 DeathMs = static_cast<int64>(FMath::RoundToDouble((Rig.Clock.GetServerTimeSeconds() + 0.4) * 1000.0));
		TestTrue(TEXT("real Roster: the tombstone carries the death write's time"), Tombstone != nullptr && Tombstone->RetireAtServerTimeMs == DeathMs);
		TestTrue(TEXT("real Roster: one respawn, InformantKilled"), Rig.Respawns.Calls.Num() == 1 && Rig.Respawns.Calls[0].Participant == VictimId
			&& Rig.Respawns.Calls[0].Cause == ERespawnCause::InformantKilled);
		TestEqual(TEXT("real Roster: no Roster fault"), Rig.RosterFaults.GetRecords().Num(), 0);
		TestEqual(TEXT("real Roster: no Penalty fault"), Rig.PenaltyFaults.GetRecords().Num(), 0);
	}
	return true;
}

#endif
