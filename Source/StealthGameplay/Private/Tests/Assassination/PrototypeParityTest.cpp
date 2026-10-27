// Story asn-027 AC1 (production/epics/assassination-defence-reversal/story-027-port-rules-reference-tests-retire-prototype.md):
// every assertion of the retired prototype tests StealthGame.Rules.Assassination and StealthGame.Rules.Defence
// (Source/StealthGame/Private/Tests/StealthRulesTests.cpp until asn-027) re-run on the production cores with the same
// inputs: the strike resolver in the queue slot (FAttackResolverCore over the EncounterTestRig fakes), the defence state
// (FDefenceStateCore), Prepare's CanBegin (UAssassinationPreparePredicate), Attack's CanBegin (UAssassinationAttackPredicate)
// and the owner's cooldown datum (UDefenceOwnerComponent). The prototype values: Cooldown 60, Duration 4, t = 10.
//
// Mapping (prototype assertion -> production row; "=" same outcome, "D" a documented, intended difference):
//   Rules.Assassination
//   A1 Unprepared active enemy is eliminated       = Kill, RivalEliminated, OnPawnKilled(Player, Strike), Roster Assassinated
//   A2 Prepared active enemy reverses the attack   = Reversal, Reversed, OnPawnKilled(attacker, defender, Player, Reversal),
//                                                    Roster ReversedAgainst on the attacker
//   A3 NPC target is an NPC kill                   = Kill, WrongTarget, OnPawnKilled(Npc, Strike), no Roster transition
//   A4 Enemy informant kill is not an NPC kill     D OnPawnKilled(Informant, Strike) (not Npc), with a WrongTarget result
//                                                    identical to an NPC kill (ASN-R10, PEN-R14); no Roster transition
//   A5 Teammates cannot be attacked                D Executed, NoContact (no victim, no kill): allies are never victims and
//                                                    nothing is rejected for its target (ASN-R1, ASN-R9, story asn-008)
//   A6 Informants cannot attack                    D the IIF legality answers NotNow (the slot's live legality re-check, the
//                                                    IIF stand-in); Assassination's own CanBegin allows every pawn
//   A7 Informants cannot attack NPCs               D the same NotNow; the NPC is untouched
//   Rules.Defence
//   D1 First use is allowed                        = Begin at t = 10 with no stamp executes (core and slot)
//   D2 Blocked while running                       D the IIF legality: Prepare(Begin) of a running pawn is NotNow (IIF R7)
//   D3 Blocked during parkour                      D the IIF legality: of a traversing pawn NotNow (IIF R7)
//   D4 Blocked for informants                      D the IIF legality: of an informant NotNow
//   D5 Cooldown runs from activation (69.9)        = Reject(Cooldown) at 69.9 after an activation at 10.0
//   D6 Usable again once per minute (70.0)         = Allow / Executed at 70.0 (inclusive); the default is still 60 s
//   D7 Cooldown remaining (40 after 10) = 30       = FDefenceStateCore::CooldownRemaining and the owner's
//                                                    UDefenceOwnerComponent::GetCooldownRemaining both 30 (story asn-016)
//   D8 No cooldown before first use = 0            = 0 with no stamp (core and owner component); 60 (full) at activation
//   D9 Defence holds within its window (13.9)      = a strike 3.9 s after the Begin is reversed (window open)
//   D10 Defence expires after its duration (14.0)  D window = DefenceDurationSec 4.0 s (a tuning key): HoldTimer(CapEnd) due
//                                                    at exactly T_act + 4.0 ends it; a strike after it kills
//   D11 Running cancels the defence                D the brace is planted (BracePlanted, PlantedBraceTest) and the cancel on
//   D12 Parkour cancels the defence                  run or parkour is the IIF's (IIF stories 033 / 034); here a Traverse
//                                                    command during the brace is refused Busy by the slot and the window
//                                                    stays open

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/AssassinationAttackPredicate.h"
#include "Server/AssassinationPreparePredicate.h"
#include "Tests/Assassination/EncounterTestRig.h"
#include "UObject/Package.h"
#include "View/DefenceOwnerComponent.h"

namespace PrototypeParityTestData
{
	using namespace StealthEncounterTest;

	/** The prototype test values (StealthRulesTestValues): that exact number IS the point. */
	constexpr double PrototypeCooldownSec = 60.0;
	constexpr double PrototypeDurationSec = 4.0;
	constexpr double PrototypeActivation = 10.0;

	/** Server time T at 60 Hz as a queue key. */
	FQueueKey KeyAt(double T, uint64 Seq = 1)
	{
		return FQueueKey(static_cast<uint64>(FMath::RoundToInt64(T * 60.0)), Seq);
	}

	/** The due time of Pawn's latest scheduled HoldTimer of Kind (unset when none). */
	TOptional<double> HoldDue(const FRig& R, const APawn* Pawn, EHoldTimerKind Kind)
	{
		for (int32 Index = R.Queue.Scheduled.Num() - 1; Index >= 0; --Index)
		{
			const FHoldTimerPayload* Hold = R.Queue.Scheduled[Index].Item.Payload.TryGet<FHoldTimerPayload>();
			if (Hold != nullptr && Hold->Pawn.Pawn.Get() == Pawn && Hold->Kind == Kind)
			{
				return R.Queue.Scheduled[Index].Due;
			}
		}
		return {};
	}

	bool IsVoided(const FEncounterCommandReply& Reply, ECommandRejectCode Code)
	{
		return !Reply.bExecuted && Reply.Code == Code;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationPrototypeParityTest, "StealthGame.Assassination.PrototypeParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationPrototypeParityTest::RunTest(const FString& Parameters)
{
	using namespace PrototypeParityTestData;
	const FVector Ahead(150.0, 0.0, 0.0); // the victim straight ahead of an attacker at the origin facing +X

	// ======== Rules.Assassination ========

	// A1 Unprepared active enemy is eliminated.
	{
		FRig R(TEXT("ParityA1"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(Ahead, 1);
		R.AttackRef(A, V);
		TestTrue(TEXT("A1: Kill with RivalEliminated"), R.Last().Outcome == EAttackOutcome::Kill && R.Last().Victim == V && R.Last().StrikeResult.Result == EStrikeResult::RivalEliminated);
		TestTrue(TEXT("A1: OnPawnKilled(V, A, Player, Strike)"), R.Kills.Num() == 1 && R.Kills[0].Victim == V && R.Kills[0].Killer == A
			&& R.Kills[0].Kind == EVictimKind::Player && R.Kills[0].Cause == EKillCause::Strike);
		TestTrue(TEXT("A1: the rival becomes an informant (Roster Assassinated)"), R.Roster.Transitions.Num() == 1 && R.Roster.Transitions[0].Pawn == V
			&& R.Roster.Transitions[0].Reason == ERosterReason::Assassinated);
	}

	// A2 Prepared active enemy reverses the attack.
	{
		FRig R(TEXT("ParityA2"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(Ahead, 1);
		TestTrue(TEXT("A2: the rival's brace executes"), R.Command(V, ECommandKind::PrepareBegin) == EResolveResult::Executed);
		R.Advance(1.0);
		R.AttackRef(A, V);
		TestTrue(TEXT("A2: Reversal with Reversed"), R.Last().Outcome == EAttackOutcome::Reversal && R.Last().StrikeResult.Result == EStrikeResult::Reversed);
		TestTrue(TEXT("A2: OnPawnKilled(A, V, Player, Reversal): the attacker falls"), R.Kills.Num() == 1 && R.Kills[0].Victim == A && R.Kills[0].Killer == V
			&& R.Kills[0].Kind == EVictimKind::Player && R.Kills[0].Cause == EKillCause::Reversal);
		TestTrue(TEXT("A2: the attacker is eliminated (Roster ReversedAgainst)"), R.Roster.Transitions.Num() == 1 && R.Roster.Transitions[0].Pawn == A
			&& R.Roster.Transitions[0].Reason == ERosterReason::ReversedAgainst);
		TestFalse(TEXT("A2: the defender lives"), R.Core->IsDead(V));
	}

	// A3 NPC target is an NPC kill; A4 an enemy informant kill is not an NPC kill but carries the same WrongTarget result.
	FAttackResolution NpcKill;
	FEncounterCommandReply NpcReply;
	{
		FRig R(TEXT("ParityA3"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Npc = R.Spawn(Ahead);
		R.AttackRef(A, Npc);
		NpcKill = R.Last();
		NpcReply = R.LastReply();
		TestTrue(TEXT("A3: Kill with WrongTarget"), NpcKill.Outcome == EAttackOutcome::Kill && NpcKill.Victim == Npc && NpcKill.StrikeResult.Result == EStrikeResult::WrongTarget);
		TestTrue(TEXT("A3: OnPawnKilled(Npc, Strike)"), R.Kills.Num() == 1 && R.Kills[0].Kind == EVictimKind::Npc && R.Kills[0].Cause == EKillCause::Strike);
		TestEqual(TEXT("A3: no Roster transition"), R.Roster.Transitions.Num(), 0);
	}
	{
		FRig R(TEXT("ParityA4"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Informant = R.Spawn(Ahead, 1);
		R.Roster.Members[Informant].State = EParticipantState::Informant;
		R.AttackRef(A, Informant);
		TestTrue(TEXT("A4: OnPawnKilled(Informant, Strike), not Npc"), R.Kills.Num() == 1 && R.Kills[0].Victim == Informant
			&& R.Kills[0].Kind == EVictimKind::Informant && R.Kills[0].Cause == EKillCause::Strike);
		TestTrue(TEXT("A4 (difference): the WrongTarget result is identical to an NPC kill's"), R.Last().Outcome == NpcKill.Outcome
			&& R.Last().StrikeResult.Result == EStrikeResult::WrongTarget && R.Last().StrikeResult.Result == NpcKill.StrikeResult.Result
			&& R.Last().StrikeResult.TallyAfter == NpcKill.StrikeResult.TallyAfter && R.Last().StrikeResult.bExiled == NpcKill.StrikeResult.bExiled
			&& R.LastReply().SameReplyAs(NpcReply));
		TestEqual(TEXT("A4: an informant kill is not a Roster transition"), R.Roster.Transitions.Num(), 0);
	}

	// A5 Teammates cannot be attacked -> NoContact, executed (no rejection).
	{
		FRig R(TEXT("ParityA5"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Mate = R.Spawn(Ahead, 0);
		R.AttackRef(A, Mate);
		TestTrue(TEXT("A5 (difference): Executed, not rejected"), R.LastReply().bExecuted);
		TestTrue(TEXT("A5 (difference): NoContact, no victim"), R.Last().Victim == nullptr && R.Last().StrikeResult.Result == EStrikeResult::NoContact);
		TestTrue(TEXT("A5: the teammate is untouched"), R.Kills.Num() == 0 && !R.Core->IsDead(Mate) && R.Roster.Transitions.Num() == 0);
	}

	// A6 / A7 Informants cannot attack (an Active enemy, an NPC) -> the IIF legality's NotNow; Assassination's CanBegin allows.
	for (const bool bNpcTarget : { false, true })
	{
		const TCHAR* Row = bNpcTarget ? TEXT("A7") : TEXT("A6");
		FRig R(TEXT("ParityA6A7"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Target = bNpcTarget ? R.Spawn(Ahead) : R.Spawn(Ahead, 1);
		R.Roster.Members[A].State = EParticipantState::Informant;
		TestTrue(FString::Printf(TEXT("%s (difference): Assassination's own CanBegin allows every pawn"), Row), UAssassinationAttackPredicate::Evaluate() == FPredicateResult::Allow());
		TestTrue(FString::Printf(TEXT("%s: the Attack is voided"), Row), R.AttackRef(A, Target) == EResolveResult::Voided);
		TestTrue(FString::Printf(TEXT("%s (difference): the legality answers NotNow"), Row), IsVoided(R.LastReply(), ECommandRejectCode::NotNow));
		TestTrue(FString::Printf(TEXT("%s: nothing executed, nobody fell"), Row), R.Core->GetResolutions().Num() == 0 && R.Kills.Num() == 0 && !R.Core->IsDead(Target));
	}

	// ======== Rules.Defence ========
	const FAssassinationTuning_Public Tuning;
	TestEqual(TEXT("the production DefenceCooldownSec default is still the prototype's 60 s"), static_cast<double>(Tuning.DefenceCooldownSec), PrototypeCooldownSec);
	TestEqual(TEXT("the production DefenceDurationSec default is the prototype's 4.0 s"), static_cast<double>(Tuning.DefenceDurationSec), PrototypeDurationSec);
	const double C = Tuning.DefenceCooldownSec;
	const FPredicateResult Cooldown = FPredicateResult::Reject(ECommandRejectCode::Cooldown);

	// D1, D5 - D8 on the core, the predicate and the owner datum at the prototype's own times.
	{
		FDefenceStateCore State;
		UDefenceOwnerComponent* Owner = NewObject<UDefenceOwnerComponent>(GetTransientPackage());
		TestTrue(TEXT("D1: first use is allowed (no stamp)"), UAssassinationPreparePredicate::Evaluate(&State, PrototypeActivation, C) == FPredicateResult::Allow());
		TestEqual(TEXT("D8: no cooldown before first use (core)"), State.CooldownRemaining(40.0, C), 0.0);
		TestEqual(TEXT("D8: no cooldown before first use (owner datum)"), Owner->GetCooldownRemaining(40.0), 0.0);
		TestTrue(TEXT("D1: the Begin at t = 10 executes"), State.BeginPrepare(PrototypeActivation, C, KeyAt(PrototypeActivation)) == EDefenceBeginResult::Executed);
		Owner->SetCooldownEndServerTime(State.GetCooldownEndServerTime(C).Get(-1.0));
		TestEqual(TEXT("D8: the full cooldown at activation"), State.CooldownRemaining(PrototypeActivation, C), PrototypeCooldownSec);
		TestTrue(TEXT("D5: cooldown runs from activation: 69.9 is Cooldown"), UAssassinationPreparePredicate::Evaluate(&State, 69.9, C) == Cooldown);
		TestTrue(TEXT("D5: a Begin executing at 69.9 is refused and leaves the stamp"), State.BeginPrepare(69.9, C, KeyAt(69.9)) == EDefenceBeginResult::Cooldown
			&& State.GetLastPrepareActivation() == TOptional<double>(PrototypeActivation));
		TestEqual(TEXT("D7: cooldown remaining at 40 is 30 (core)"), State.CooldownRemaining(40.0, C), 30.0);
		TestEqual(TEXT("D7: cooldown remaining at 40 is 30 (the owner's display datum)"), Owner->GetCooldownRemaining(40.0), 30.0);
		TestEqual(TEXT("D7: remaining is 0 at 70.0"), State.CooldownRemaining(70.0, C), 0.0);
		TestTrue(TEXT("D6: usable again at 70.0 (inclusive)"), UAssassinationPreparePredicate::Evaluate(&State, 70.0, C) == FPredicateResult::Allow());
		TestTrue(TEXT("D6: the Begin at 70.0 executes"), State.BeginPrepare(70.0, C, KeyAt(70.0)) == EDefenceBeginResult::Executed);
	}

	// D1 at the slot, D2 - D4: the IIF legality (the slot's stand-in) for a running, a traversing and an informant pawn.
	{
		FRig R(TEXT("ParityD1D4"));
		APawn* Fresh = R.Spawn(FVector::ZeroVector, 0);
		APawn* Runner = R.Spawn(FVector(0.0, 400.0, 0.0), 0);
		APawn* Climber = R.Spawn(FVector(0.0, 800.0, 0.0), 0);
		APawn* Informant = R.Spawn(FVector(0.0, 1200.0, 0.0), 1);
		R.Facts(Runner).bRunning = true;
		R.Facts(Climber).bTraversing = true;
		R.Roster.Members[Informant].State = EParticipantState::Informant;
		TestTrue(TEXT("D1: a first brace executes at the slot"), R.Command(Fresh, ECommandKind::PrepareBegin) == EResolveResult::Executed);
		R.Command(Runner, ECommandKind::PrepareBegin);
		TestTrue(TEXT("D2 (difference): running -> the IIF's NotNow"), IsVoided(R.LastReply(), ECommandRejectCode::NotNow));
		R.Command(Climber, ECommandKind::PrepareBegin);
		TestTrue(TEXT("D3 (difference): parkour -> the IIF's NotNow"), IsVoided(R.LastReply(), ECommandRejectCode::NotNow));
		R.Command(Informant, ECommandKind::PrepareBegin);
		TestTrue(TEXT("D4 (difference): an informant -> the IIF's NotNow"), IsVoided(R.LastReply(), ECommandRejectCode::NotNow));
		TestTrue(TEXT("D2 - D4: no blocked brace stamped a cooldown"), R.State(Runner) == nullptr && R.State(Climber) == nullptr && R.State(Informant) == nullptr);
	}

	// D9 Defence holds within its window: a strike 3.9 s after the Begin is reversed.
	{
		FRig R(TEXT("ParityD9"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(Ahead, 1);
		const double TAct = R.Clock.GetServerTimeSeconds();
		R.Command(V, ECommandKind::PrepareBegin);
		TestEqual(TEXT("D10 (difference): the window's CapEnd is due at exactly T_act + DefenceDurationSec"), HoldDue(R, V, EHoldTimerKind::CapEnd).Get(-1.0), TAct + PrototypeDurationSec);
		R.Advance(3.9);
		R.AttackRef(A, V);
		TestTrue(TEXT("D9: within the window (T_act + 3.9) the strike is reversed"), R.Last().Outcome == EAttackOutcome::Reversal);
	}

	// D10 Defence expires after its duration: CapEnd at T_act + 4.0 ends the window; a strike after it kills.
	{
		FRig R(TEXT("ParityD10"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(Ahead, 1);
		R.Command(V, ECommandKind::PrepareBegin);
		R.Advance(PrototypeDurationSec);
		TestTrue(TEXT("D10: the CapEnd resolves at T_act + 4.0"), R.FireHold(V, EHoldTimerKind::CapEnd, R.NextKey()) == EResolveResult::Executed);
		TestTrue(TEXT("D10: the window is closed"), R.State(V) != nullptr && R.State(V)->GetWindow().EndKey.IsSet());
		R.AttackRef(A, V);
		TestTrue(TEXT("D10: after the window the strike kills"), R.Last().Outcome == EAttackOutcome::Kill && R.Last().Victim == V);
	}

	// D11 / D12 Running or parkour cancels the defence -> the brace is planted; a Traverse during it is refused Busy.
	{
		FRig R(TEXT("ParityD11D12"));
		APawn* V = R.Spawn(FVector::ZeroVector, 1);
		R.Command(V, ECommandKind::PrepareBegin);
		R.Advance(1.0);
		R.Command(V, ECommandKind::Traverse);
		TestTrue(TEXT("D12 (difference): a Traverse during the brace is refused Busy by the slot"), IsVoided(R.LastReply(), ECommandRejectCode::Busy));
		TestTrue(TEXT("D11 / D12 (difference): the window stays open (the cancel on run or parkour is the IIF's, stories 033 / 034)"),
			R.State(V) != nullptr && !R.State(V)->GetWindow().EndKey.IsSet() && R.Core->GetRole(V) == EStrikeRole::Bracing);
		TestTrue(TEXT("D11 (difference): the planted stance is set (BracePlanted; PlantedBraceTest proves no displacement)"), R.Stance.Braced.FindRef(V));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
