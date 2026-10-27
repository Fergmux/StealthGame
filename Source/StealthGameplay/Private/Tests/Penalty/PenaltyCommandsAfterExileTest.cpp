// Story nkp-005 (production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md), AC2 /
// AC-NKP-16 (regression guard: Penalty adds nothing that lets a command reach the released body). On the real round queue
// and strike resolver with Penalty wired (PenaltyEncounterRig.h): P's exiling Attack, then, in the exile's own drain, P's
// Attack, Prepare, Interact, Hide and Traverse commands for the body it was driving. Each is voided (none executes), the
// released body receives none of them (the non-strike fallback records 0 deliveries, no second strike resolves, the second
// NPC lives) and Penalty counts nothing more.
// Reject code: inside the exile's frame the body is in its strike lockout, so the slot rule answers Busy (ASN-R14) before
// any legality check. NotNow for a command with no bound pawn is the IIF's stage-2 check (iif-027, legality class iif-057)
// and its possession-epoch gate (iif-025); in production the factory's hand-over commit (UStealthPawnFactory::
// RequestHandOver) also leaves the player's controller pawnless, so ingress drops every later command (DroppedNoPawn).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyEncounterRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyCommandsAfterExileTest, "StealthGame.Penalty.CommandsAfterExile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyCommandsAfterExileTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyEncounterTest;
	const FParticipantId P(1);

	// Arrange: P at k = L - 1 (a teammate still Active, so the round runs on), an NPC to kill and a second NPC beside it.
	FPenaltyEncounterRig Rig(TEXT("PenaltyCommandsAfterExile"));
	APawn* BodyOfP = Rig.R.Spawn(FVector::ZeroVector, P.Value);
	Rig.ArrangeNpcKills(BodyOfP, Rig.Penalty->GetLatchedStrikeLimit() - 1);
	APawn* Npc = Rig.NpcInFrontOf(BodyOfP);
	APawn* SecondNpc = Rig.NpcInFrontOf(BodyOfP, 40.0);
	const int32 ResolutionsBefore = Rig.R.Core->GetResolutions().Num();
	const int32 StrikesAtExile = Rig.Penalty->GetLatchedStrikeLimit();

	// Act: the exiling Attack, then the five commands of P's client for the body, in the same drain.
	TArray<FQueueItem> Items;
	Items.Add(Rig.R.AttackAt(BodyOfP, Npc));
	Items.Add(Rig.R.AttackAt(BodyOfP, SecondNpc));
	Items.Add(Rig.R.Command(BodyOfP, ECommandKind::PrepareBegin));
	Items.Add(Rig.R.Command(BodyOfP, ECommandKind::InteractBegin));
	Items.Add(Rig.R.Command(BodyOfP, ECommandKind::HideEnter));
	Items.Add(Rig.R.Command(BodyOfP, ECommandKind::Traverse));
	Rig.R.Drain(Items);

	// Assert: the exile happened in the first slot.
	TestEqual(TEXT("the exile ran"), Rig.Bodies.Count(TEXT("RequestTransition")), 1);
	TestTrue(TEXT("the exiling Attack's plate carries bExiled"), Rig.Tallies.Num() > 0 && Rig.Tallies.Last().Value.bExiled);
	// Every later command is voided and reaches nothing.
	const TCHAR* Names[] = { TEXT("Attack"), TEXT("Prepare"), TEXT("Interact"), TEXT("Hide"), TEXT("Traverse") };
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Names)); ++Index)
	{
		const FQueueSlotRecord* Record = Rig.R.CommandRecord(Index + 1);
		TestTrue(FString::Printf(TEXT("%s after the exile is voided"), Names[Index]), Record != nullptr && Record->Outcome == EQueueSlotOutcome::Voided);
	}
	TestEqual(TEXT("no second strike resolved"), Rig.R.Core->GetResolutions().Num(), ResolutionsBefore + 1);
	TestFalse(TEXT("the second NPC lives"), Rig.R.Core->IsDead(SecondNpc));
	TestEqual(TEXT("the released body received 0 non-strike deliveries"), Rig.Fallback.CountFor(BodyOfP), 0);
	TestFalse(TEXT("the released body is not bracing"), Rig.R.Core->GetRole(BodyOfP) == EStrikeRole::Bracing);
	TestEqual(TEXT("Penalty counted nothing more"), static_cast<int32>(Rig.Penalty->GetStrikeCount(P)), StrikesAtExile);
	TestEqual(TEXT("L tally answers in all: the L - 1 arranged kills and the exiling Attack, none for the voided commands"), Rig.Tallies.Num(), StrikesAtExile);
	TestEqual(TEXT("no Penalty fault"), Rig.PenaltyFaults.GetRecords().Num(), 0);
	return true;
}

#endif
