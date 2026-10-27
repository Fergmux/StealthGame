// Story asn-010 (production/epics/assassination-defence-reversal/story-010-reversal-counter-and-chained-strikes.md), AC1 /
// AC-ASN-03 ReversalEliminatesAttacker (ASN-R10, R14; confirmed user rule: a reversal eliminates the attacker), on the
// integration harness (the resolver over its seams with real pawn identities). A defender, a player (bound, reply sink 5)
// and in a second run an NPC (null sink), whose Prepare executed 1 s earlier: a rival's Attack on it makes the attacker
// Informant / ReversedAgainst, fires OnPawnKilled(attacker, defender, Player, Reversal) once, cancels the brace with
// Reversed, puts the defender in Countering for 1.2 s from the slot (+- 1 tick), gives the attacker FStrikeResult.Reversed
// and a bound defender FDefenceResult (none for the NPC); the defender is not eliminated and charged nothing (its stamp,
// Roster state and role-free record are unchanged) and gets exactly one FActionState write (the staged CounterPose).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationReversalEliminatesAttackerTest, "StealthGame.Assassination.ReversalEliminatesAttacker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationReversalEliminatesAttackerTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	constexpr double Tick = 1.0 / 60.0;
	constexpr uint16 DefenderSink = 5;
	for (const bool bNpc : { false, true })
	{
		const TCHAR* Name = bNpc ? TEXT("NPC defender") : TEXT("player defender");
		FRig R(TEXT("ReversalEliminates"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* D = bNpc ? R.Spawn(FVector(150.0, 0.0, 0.0)) : R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		R.Command(D, ECommandKind::PrepareBegin, {}, bNpc ? 0 : DefenderSink);
		const double Activation = R.State(D)->GetLastPrepareActivation().GetValue();
		R.Advance(1.0);
		const TOptional<FRosterVictimKind> DefenderKindBefore = R.Roster.ResolveVictimKind(D);

		const double Slot = R.Clock.GetServerTimeSeconds();
		R.AttackRef(A, D);

		const FAttackResolution& Res = R.Last();
		TestTrue(FString::Printf(TEXT("%s: a reversal"), Name), Res.Outcome == EAttackOutcome::Reversal);
		TestTrue(FString::Printf(TEXT("%s: the attacker becomes Informant / ReversedAgainst"), Name), R.Roster.Transitions.Num() == 1 && R.Roster.Transitions[0].Pawn == A
			&& R.Roster.Transitions[0].Reason == ERosterReason::ReversedAgainst && R.Roster.Members[A].State == EParticipantState::Informant);
		TestTrue(FString::Printf(TEXT("%s: OnPawnKilled(attacker, defender, Player, Reversal) once"), Name), R.Kills.Num() == 1 && R.Kills[0].Victim == A
			&& R.Kills[0].Killer == D && R.Kills[0].Kind == EVictimKind::Player && R.Kills[0].Cause == EKillCause::Reversal);
		TestTrue(FString::Printf(TEXT("%s: the brace is cancelled with Reversed"), Name), R.Events.Events.ContainsByPredicate([D](const FRecordingEvents::FEvent& E)
		{
			return E.Pawn == D && E.Type == EActionType::Prepare && E.Kind == 1 && E.Reason == ECancelReason::Reversed;
		}));
		TestTrue(FString::Printf(TEXT("%s: Countering, ending 1.2 s after the slot"), Name), R.Core->GetRole(D) == EStrikeRole::Countering
			&& FMath::IsNearlyEqual(R.Core->GetRoles().GetRoleEnd(D), Slot + R.Params.Public.CounterDurationSec, 1e-9));
		R.Clock.AdvanceSeconds(R.Params.Public.CounterDurationSec - Tick);
		TestTrue(FString::Printf(TEXT("%s: still Countering one tick before 1.2 s"), Name), R.Core->GetRole(D) == EStrikeRole::Countering);
		R.Clock.AdvanceSeconds(Tick + 1e-6); // at 1.2 s (never one ulp short)
		TestTrue(FString::Printf(TEXT("%s: Cooling at 1.2 s (cooldown still running)"), Name), R.Core->GetRole(D) == EStrikeRole::Cooling);
		TestTrue(FString::Printf(TEXT("%s: the attacker's FStrikeResult is Reversed"), Name), Res.StrikeResult.Result == EStrikeResult::Reversed);
		TestEqual(FString::Printf(TEXT("%s: FDefenceResult only for a bound defender"), Name), Res.HasDefenceResult(), !bNpc);
		TestTrue(FString::Printf(TEXT("%s: the defender is not eliminated"), Name), !R.Core->IsDead(D) && R.Roster.ResolveVictimKind(D) == DefenderKindBefore.GetValue());
		TestTrue(FString::Printf(TEXT("%s: the defender's cooldown stamp is unchanged (charged nothing)"), Name), R.State(D)->GetLastPrepareActivation().GetValue() == Activation);
		int32 CounterWrites = 0, DeathWrites = 0;
		for (const FStagedWrite& Write : R.Queue.Staged)
		{
			if (const FCounterPayload* Counter = Write.Payload.TryGet<FCounterPayload>())
			{
				CounterWrites += Counter->Defender.Pawn.Get() == D ? 1 : 0;
			}
			if (const FDeathWritePayload* Death = Write.Payload.TryGet<FDeathWritePayload>())
			{
				DeathWrites += Death->Victim.Pawn.Get() == A ? 1 : 0;
				TestTrue(FString::Printf(TEXT("%s: no death write for the defender"), Name), Death->Victim.Pawn.Get() != D);
			}
		}
		TestEqual(FString::Printf(TEXT("%s: exactly one defender FActionState write (CounterPose)"), Name), CounterWrites, 1);
		TestEqual(FString::Printf(TEXT("%s: the attacker's death write"), Name), DeathWrites, 1);
	}
	return true;
}

#endif
