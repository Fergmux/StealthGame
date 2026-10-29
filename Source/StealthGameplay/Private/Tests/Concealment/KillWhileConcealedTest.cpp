// Stories con-017 and con-018 (production/epics/concealment-bushes/story-017-kill-while-concealed-server-rules.md,
// story-018-no-immunity-while-entering.md; GDD CON-R9, AC-CON-25, AC-CON-26, TR-con-012; ADR-0017 sections 3 and 8 B):
//   - StealthGame.Concealment.KillSlotRules (017 AC1): a concealed victim killed in the outcome slot leaves the record in
//     that slot (a second strike on the bush in the same tick draws from the survivors), is no longer a candidate, no cull
//     change happens in the slot, and the owner/team flags stay true.
//   - StealthGame.Concealment.DeathWrite (017 AC2): the un-cull happens at the death write, for an NPC and a player victim,
//     and bDiedConcealed is due only for a victim that was Concealed (culled) at the kill; never earlier, never for an
//     Entering or Outside victim; FDeathState is still exactly 65 bits.
//   - StealthGame.Concealment.BodyRemovalOrRebind (017 AC3): the flags clear on whichever comes first of the body's removal
//     and the participant's rebind, exactly once; a victim's own Hide(Exit) between the slot and the death write is NotNow.
//   - StealthGame.Concealment.NoImmunityWhileEntering (018): struck at +0.3 s of BushEnter the pawn dies visible (no record,
//     no cull, the Commit dropped by generation); at +0.6 s it is a spot candidate; a hider in the same bush can strike the
//     entrant both ways, though the entrant's own Hide is Busy until +0.9 s.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentStrikeAdapters.h"
#include "Serialization/BitWriter.h"

using namespace StealthConcealmentTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentKillSlotRulesTest, "StealthGame.Concealment.KillSlotRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentKillSlotRulesTest::RunTest(const FString& Parameters)
{
	FRig Rig(TEXT("ConcealmentKillSlot"));
	FStrikeView View(Rig);
	AStealthPawn* Attacker = Rig.Spawn(FVector(-150.0, 0.0, 86.0));
	Rig.Relations.Bind(Attacker, 1, 0);
	AStealthPawn* Victim = Rig.SpawnAtReach(30.0, 20.0);
	Rig.Relations.Bind(Victim, 2, 1);
	AStealthPawn* Survivor = Rig.SpawnAtReach(30.0, -20.0);
	Rig.EnterAndSettle(Victim);
	Rig.EnterAndSettle(Survivor);
	const int32 CullCalls = Rig.Cull.Calls.Num();

	// The outcome slot: the kill (Assassination's RemoveOccupant / OnPawnKilled fan-out).
	Rig.Core->OnPawnKilled(*Victim, FQueueKey(7000, 1));
	TArray<APawn*> Record;
	Rig.Core->GetOccupancy(1, Record);
	TestFalse(TEXT("the victim left the record in the slot"), Record.Contains(Victim));
	TestFalse(TEXT("... and is no longer a candidate (IsConcealed false)"), Rig.Core->IsConcealed(Victim));
	TestEqual(TEXT("no cull change in the outcome slot"), Rig.Cull.Calls.Num(), CullCalls);
	TestTrue(TEXT("owner and team flags stay true"), Rig.Flags.OwnerOf(2) && Rig.Flags.TeamOf(2));
	TestEqual(TEXT("one Killed log row"), Rig.Count(EConcealmentLogEvent::Killed), 1);
	// A second strike on the same bush in the same tick draws from the survivors only.
	int32 SurvivorPicks = 0;
	for (int32 Seed = 1; Seed <= 50; ++Seed)
	{
		View.Seeds.Seed = static_cast<uint64>(Seed) * 0x2545F4914F6CDD1Dull;
		SurvivorPicks += View.Strike(Attacker, FVector::ZeroVector, nullptr, FQueueKey(7000, 2)).Victim == Survivor ? 1 : 0;
	}
	TestEqual(TEXT("the same-tick second strike always finds the survivor"), SurvivorPicks, 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentDeathWriteTest, "StealthGame.Concealment.DeathWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentDeathWriteTest::RunTest(const FString& Parameters)
{
	for (const bool bPlayer : { false, true })
	{
		const TCHAR* Who = bPlayer ? TEXT("player") : TEXT("NPC");
		FRig Rig(bPlayer ? TEXT("ConcealmentDeathWritePlayer") : TEXT("ConcealmentDeathWriteNpc"));
		AStealthPawn* Victim = Rig.SpawnAtReach(30.0);
		if (bPlayer)
		{
			Rig.Relations.Bind(Victim, 5, 1);
		}
		Rig.EnterAndSettle(Victim);
		TestFalse(FString::Printf(TEXT("%s: bDiedConcealed is false before the kill"), Who), Rig.Core->ShouldMarkDiedConcealed(*Victim));
		Rig.Core->OnPawnKilled(*Victim, FQueueKey(1, 1));
		TestTrue(FString::Printf(TEXT("%s: after the kill, the death write will carry bDiedConcealed"), Who), Rig.Core->ShouldMarkDiedConcealed(*Victim));
		TestEqual(FString::Printf(TEXT("%s: still culled until the death write"), Who), Rig.Cull.CountFor(Victim, false), 0);
		TestTrue(FString::Printf(TEXT("%s: IsConcealedFrom an opponent until the death write"), Who), Rig.Core->IsConcealedFrom(FParticipantId(99), Victim));
		// The death write (the staging pass, ContactServerTime - StrikeNetLeadSec): the un-cull in the same step.
		TestTrue(FString::Printf(TEXT("%s: the death write un-culls"), Who), Rig.Core->OnDeathWrite(*Victim));
		TestEqual(FString::Printf(TEXT("%s: exactly one un-cull"), Who), Rig.Cull.CountFor(Victim, false), 1);
		TestFalse(FString::Printf(TEXT("%s: re-opened: no longer concealed from anyone"), Who), Rig.Core->IsConcealedFrom(FParticipantId(99), Victim));
		TestFalse(FString::Printf(TEXT("%s: a second death write changes nothing"), Who), Rig.Core->OnDeathWrite(*Victim));
	}
	// Entering and Outside victims never get bDiedConcealed.
	{
		FRig Rig(TEXT("ConcealmentDeathWriteEntering"));
		AStealthPawn* Entrant = Rig.SpawnAtReach(30.0);
		AStealthPawn* Walker = Rig.Spawn(FVector(600.0, 0.0, 86.0));
		Rig.SubmitHide(Entrant, ECommandKind::HideEnter, 1);
		Rig.Tick();
		Rig.Core->OnPawnKilled(*Entrant, FQueueKey(2, 1));
		Rig.Core->OnPawnKilled(*Walker, FQueueKey(2, 2));
		TestFalse(TEXT("an Entering victim: no bDiedConcealed"), Rig.Core->ShouldMarkDiedConcealed(*Entrant));
		TestFalse(TEXT("an Outside victim: no bDiedConcealed"), Rig.Core->ShouldMarkDiedConcealed(*Walker));
		TestFalse(TEXT("an Entering victim's death write un-culls nothing"), Rig.Core->OnDeathWrite(*Entrant));
		TestEqual(TEXT("no cull call at all"), Rig.Cull.Calls.Num(), 0);
	}
	// FDeathState is still exactly 65 bits (no new field).
	FDeathState State;
	State.DeathServerTime = 12.5;
	State.bDiedConcealed = true;
	FBitWriter Writer(256, true);
	bool bOk = false;
	State.NetSerialize(Writer, nullptr, bOk);
	TestEqual(TEXT("FDeathState serialises to exactly 65 bits"), static_cast<int32>(Writer.GetNumBits()), FDeathState::WireBits);
	TestEqual(TEXT("... and WireBits is 65"), FDeathState::WireBits, 65);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentBodyRemovalOrRebindTest, "StealthGame.Concealment.BodyRemovalOrRebind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentBodyRemovalOrRebindTest::RunTest(const FString& Parameters)
{
	for (const int32 Order : { 0, 1, 2 }) // 0 removal first, 1 rebind first, 2 both on one tick
	{
		FRig Rig(*FString::Printf(TEXT("ConcealmentRemovalOrRebind%d"), Order));
		AStealthPawn* Victim = Rig.SpawnAtReach(30.0);
		Rig.Relations.Bind(Victim, 6, 0);
		Rig.EnterAndSettle(Victim);
		Rig.Core->OnPawnKilled(*Victim, FQueueKey(3, 1));
		// The victim's own Hide(Exit) between the kill slot and the death write: NotNow.
		Rig.SubmitHide(Victim, ECommandKind::HideExit, 0);
		Rig.Tick();
		TestEqual(FString::Printf(TEXT("order %d: the victim's own exit is NotNow"), Order), Rig.RigQueue.CountRejected(ECommandRejectCode::NotNow), 1);
		Rig.Core->OnDeathWrite(*Victim);
		TestTrue(FString::Printf(TEXT("order %d: flags still set after the death write"), Order), Rig.Flags.OwnerOf(6) && Rig.Flags.TeamOf(6));
		const int32 Before = Rig.Flags.Writes;
		AStealthPawn* NewBody = Rig.Spawn(FVector(900.0, 0.0, 86.0));
		if (Order == 0 || Order == 2)
		{
			Rig.Core->OnBodyRemoved(*Victim);
		}
		if (Order == 1 || Order == 2)
		{
			Rig.Core->OnParticipantRebound(FParticipantId(6), NewBody);
		}
		if (Order == 1)
		{
			Rig.Core->OnBodyRemoved(*Victim); // the later removal changes nothing
		}
		TestFalse(FString::Printf(TEXT("order %d: the owner flag cleared"), Order), Rig.Flags.OwnerOf(6));
		TestFalse(FString::Printf(TEXT("order %d: the team flag cleared"), Order), Rig.Flags.TeamOf(6));
		TestEqual(FString::Printf(TEXT("order %d: cleared exactly once (one owner and one team write)"), Order), Rig.Flags.Writes - Before, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentNoImmunityWhileEnteringTest, "StealthGame.Concealment.NoImmunityWhileEntering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentNoImmunityWhileEnteringTest::RunTest(const FString& Parameters)
{
	// +0.3 s: killed as a visible pawn.
	{
		FRig Rig(TEXT("ConcealmentNoImmunityEarly"));
		FStrikeView View(Rig);
		AStealthPawn* Hider = Rig.SpawnAtReach(30.0, 40.0);
		Rig.EnterAndSettle(Hider); // a hider already in the bush, the striker of AC2
		AStealthPawn* Entrant = Rig.SpawnAtReach(30.0, -30.0);
		Rig.Relations.Bind(Entrant, 8, 1);
		Rig.SubmitHide(Entrant, ECommandKind::HideEnter, 1);
		Rig.Tick();
		Rig.Run(0.3);
		TestEqual(TEXT("+0.3 s: still Entering"), Rig.StateOf(Entrant), EConcealState::Entering);
		// The hider strikes it as a visible TargetPawn (knowable: it is not concealed).
		Rig.Pawns.Of(Hider).Location = FVector(0.0, 0.0, 86.0);
		const FVictimResolution Hit = View.Strike(Hider, Rig.Pawns.Of(Entrant).Location, Entrant, FQueueKey(50, 1));
		TestTrue(TEXT("+0.3 s: the Entering pawn is a visible TargetPawn victim"), Hit.Victim == Entrant && Hit.Rule == EVictimRule::PawnRef);
		Rig.Core->OnPawnKilled(*Entrant, FQueueKey(50, 1));
		Rig.Pawns.Of(Entrant).bLive = false;
		TestEqual(TEXT("+0.3 s kill: Outside (an ordinary visible kill)"), Rig.StateOf(Entrant), EConcealState::Outside);
		Rig.Run(1.0);
		TArray<APawn*> Record;
		Rig.Core->GetOccupancy(1, Record);
		TestFalse(TEXT("no record for the victim (the Commit dropped by generation)"), Record.Contains(Entrant));
		TestEqual(TEXT("no cull for the victim"), Rig.Cull.CountFor(Entrant, true), 0);
		TestFalse(TEXT("no flags for the victim"), Rig.Flags.OwnerOf(8));
	}
	// +0.6 s: after the commit, a spot candidate; the entrant's own Hide stays Busy until +0.9 s.
	{
		FRig Rig(TEXT("ConcealmentNoImmunityLate"));
		FStrikeView View(Rig);
		AStealthPawn* Hider = Rig.SpawnAtReach(30.0, 40.0);
		Rig.EnterAndSettle(Hider);
		AStealthPawn* Entrant = Rig.SpawnAtReach(30.0, -30.0);
		Rig.Relations.Bind(Entrant, 9, 1);
		Rig.SubmitHide(Entrant, ECommandKind::HideEnter, 1);
		Rig.Tick();
		Rig.Run(0.6);
		TestEqual(TEXT("+0.6 s: Concealed (committed at +0.5 s)"), Rig.StateOf(Entrant), EConcealState::Concealed);
		TArray<APawn*> Candidates;
		Rig.Core->GetSpotOccupants(1, Hider, Candidates);
		TestTrue(TEXT("+0.6 s: a spot candidate of the hider in the same bush"), Candidates.Contains(Entrant));
		const FVictimResolution Ref = View.Strike(Hider, FVector::ZeroVector, Entrant, FQueueKey(60, 1));
		TestTrue(TEXT("+0.6 s: a TargetPawn naming it resolves by the bush spot rule"), Ref.Rule == EVictimRule::BushSpot);
		TestTrue(TEXT("the entrant's own Hide(Exit) before +0.9 s is Busy (its enter holds the slot)"),
			Rig.Core->CanBeginHide(*Entrant, ECommandKind::HideExit, 0) == FPredicateResult::Reject(ECommandRejectCode::Busy));
		Rig.Run(0.35);
		TestTrue(TEXT("... and allowed after +0.9 s"), Rig.Core->CanBeginHide(*Entrant, ECommandKind::HideExit, 0) == FPredicateResult::Allow());
	}
	return true;
}

#endif
