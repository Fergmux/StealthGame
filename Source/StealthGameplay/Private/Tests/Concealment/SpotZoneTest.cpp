// Stories con-015 and con-016 (production/epics/concealment-bushes/story-015-spot-zone-and-occupants.md,
// story-016-spot-pick-secret-stream.md; GDD F4, CON-R10, CON-R12, AC-CON-14; ADR-0017 section 7; ADR-0006):
//   - StealthGame.Concealment.SpotZoneAndVictim (015 AC1, AC3): WorldPoints 74 / 76 cm off a footprint side and at
//     (1.53, 1.53) / (1.55, 1.55) m from the anchor: only the first of each pair targets the bush (the zone is the footprint
//     grown by 75 cm with rounded corners; 75.0 exactly is in, the boundary is inclusive). An empty bush with a visible pawn
//     50 cm beside the planter and no PawnRef is a whiff (the bush rule is terminal); an ally-only bush is the same whiff.
//   - StealthGame.Concealment.SpotOccupants (015 AC2): GetSpotOccupants is the record minus the attacker and its allies,
//     by CommitSeq, with Exiting pawns before their crossing and non-ally co-occupants included.
//   - StealthGame.Concealment.SpotPick (016): with 2 reachable occupants and 1,000 injected seeds each is picked 450-550
//     times (one victim per strike); a PawnRef naming the concealed pawn gives the identical victim (it is not knowable,
//     so the bush rule decides); with one occupant out of reach the other is picked 1,000 of 1,000 (filter before pick);
//     the stream is GetServerSubSeed(BushSpotPick) with the "CON_SPOT" tag and UniformBelow, never modulo or RoundSeed
//     (source scan of the resolver). The pick is Assassination's FVictimResolverCore (asn-006), read here against the
//     real Concealment record.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentStrikeAdapters.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using namespace StealthConcealmentTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentSpotZoneTest, "StealthGame.Concealment.SpotZoneAndVictim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentSpotZoneTest::RunTest(const FString& Parameters)
{
	FRig Rig(TEXT("ConcealmentSpotZone"));
	const auto Targets = [&Rig](const FVector& P) { return Rig.Core->TargetsBush(P).IsSet(); };
	// Side distances (the +X side's edge is at x = 100).
	TestTrue(TEXT("74 cm off the side: in"), Targets(FVector(174.0, 0.0, 0.0)));
	TestFalse(TEXT("76 cm off the side: out"), Targets(FVector(176.0, 0.0, 0.0)));
	TestTrue(TEXT("75 cm exactly: in (inclusive boundary)"), Targets(FVector(175.0, 0.0, 0.0)));
	// Rounded corners: (1.53, 1.53) m is 74.95 cm from the corner, (1.55, 1.55) m is 77.8 cm.
	TestTrue(TEXT("(1.53, 1.53) m: in"), Targets(FVector(153.0, 153.0, 0.0)));
	TestFalse(TEXT("(1.55, 1.55) m: out"), Targets(FVector(155.0, 155.0, 0.0)));
	TestTrue(TEXT("inside the footprint: in"), Targets(FVector(10.0, -20.0, 0.0)));

	// AC3: an empty bush, a visible pawn 50 cm beside the planter, no PawnRef: a whiff, never a fall-through.
	FStrikeView View(Rig);
	AStealthPawn* Attacker = Rig.Spawn(FVector(230.0, 0.0, 86.0));
	Rig.Relations.Bind(Attacker, 1, 0);
	AStealthPawn* Bystander = Rig.Spawn(FVector(150.0, 40.0, 86.0)); // outside the footprint, inside the zone
	FVictimResolution Res = View.Strike(Attacker, FVector(150.0, 40.0, 0.0), nullptr, FQueueKey(100, 1));
	TestTrue(TEXT("the strike point is in the bush zone"), Res.bInBushZone && Res.BushId == 1);
	TestNull(TEXT("empty bush: a whiff (the visible pawn beside it is not struck)"), Res.Victim);
	// An ally-only bush gives the same empty set and the same whiff.
	AStealthPawn* Ally = Rig.SpawnAtReach(30.0, 30.0);
	Rig.Relations.Bind(Ally, 2, 0);
	Rig.EnterAndSettle(Ally);
	TArray<APawn*> Occupants;
	Rig.Core->GetSpotOccupants(1, Attacker, Occupants);
	TestEqual(TEXT("ally-only bush: an empty candidate set"), Occupants.Num(), 0);
	Res = View.Strike(Attacker, FVector(150.0, 40.0, 0.0), nullptr, FQueueKey(101, 1));
	TestNull(TEXT("ally-only bush: the identical whiff"), Res.Victim);
	(void)Bystander;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentSpotOccupantsTest, "StealthGame.Concealment.SpotOccupants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentSpotOccupantsTest::RunTest(const FString& Parameters)
{
	FRig Rig(TEXT("ConcealmentSpotOccupants"));
	AStealthPawn* Attacker = Rig.Spawn(FVector(400.0, 0.0, 86.0));
	Rig.Relations.Bind(Attacker, 1, 0);
	AStealthPawn* AllyHider = Rig.SpawnAtReach(30.0, -40.0);
	Rig.Relations.Bind(AllyHider, 2, 0);
	AStealthPawn* RivalA = Rig.SpawnAtReach(30.0, 0.0);
	Rig.Relations.Bind(RivalA, 3, 1);
	AStealthPawn* Npc = Rig.SpawnAtReach(30.0, 40.0);
	AStealthPawn* RivalB = Rig.Spawn(FVector(0.0, 130.0, 86.0));
	Rig.Relations.Bind(RivalB, 4, 1);
	for (AStealthPawn* Pawn : { AllyHider, RivalA, Npc, RivalB })
	{
		Rig.EnterAndSettle(Pawn);
	}
	TArray<APawn*> Out;
	Rig.Core->GetSpotOccupants(1, Attacker, Out);
	TestEqual(TEXT("the record minus the attacker's ally, by CommitSeq"), Out, TArray<APawn*>({ RivalA, Npc, RivalB }));
	// A rival co-occupant striking: its own ally is excluded, every non-ally (the attacker's team, the NPC) included.
	Rig.Core->GetSpotOccupants(1, RivalA, Out);
	TestEqual(TEXT("RivalA's view: the other team's hider and the NPC (no self, no ally)"), Out, TArray<APawn*>({ AllyHider, Npc }));
	// An Exiting pawn before its crossing stays a candidate; after it, it is gone.
	Rig.SubmitHide(Npc, ECommandKind::HideExit, 2);
	Rig.Tick();
	Rig.Core->GetSpotOccupants(1, Attacker, Out);
	TestTrue(TEXT("Exiting before the crossing: still an occupant"), Out.Contains(Npc));
	Rig.Run(0.5);
	Rig.Core->GetSpotOccupants(1, Attacker, Out);
	TestFalse(TEXT("after the crossing: no longer an occupant"), Out.Contains(Npc));
	TestEqual(TEXT("the others keep their order"), Out, TArray<APawn*>({ RivalA, RivalB }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentSpotPickTest, "StealthGame.Concealment.SpotPick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentSpotPickTest::RunTest(const FString& Parameters)
{
	constexpr int32 Strikes = 1000;
	FRig Rig(TEXT("ConcealmentSpotPick"));
	FStrikeView View(Rig);
	// The attacker faces +X (yaw 0) toward the anchor from the -X side, 150 cm out (F1: 115 cm to the capsule surface).
	AStealthPawn* Attacker = Rig.Spawn(FVector(-150.0, 0.0, 86.0));
	Rig.Relations.Bind(Attacker, 1, 0);
	AStealthPawn* A = Rig.SpawnAtReach(30.0, 20.0);
	AStealthPawn* B = Rig.SpawnAtReach(30.0, -20.0);
	Rig.Relations.Bind(B, 2, 1);
	Rig.EnterAndSettle(A);
	Rig.EnterAndSettle(B);
	TestTrue(TEXT("both concealed at the anchor"), Rig.Core->IsConcealed(A) && Rig.Core->IsConcealed(B));

	int32 PicksA = 0, PicksB = 0, RefMismatch = 0, NoVictim = 0;
	for (int32 Strike = 0; Strike < Strikes; ++Strike)
	{
		View.Seeds.Seed = 0x9E3779B97F4A7C15ull * static_cast<uint64>(Strike + 1);
		const FQueueKey Key(5000 + Strike, 1);
		const FVictimResolution Spot = View.Strike(Attacker, FVector(0.0, 0.0, 0.0), nullptr, Key);
		const FVictimResolution Ref = View.Strike(Attacker, FVector(0.0, 0.0, 0.0), A, Key);
		PicksA += Spot.Victim == A ? 1 : 0;
		PicksB += Spot.Victim == B ? 1 : 0;
		NoVictim += Spot.Victim == nullptr ? 1 : 0;
		RefMismatch += Ref.Victim != Spot.Victim ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("SPOTPICK A %d, B %d over %d seeds"), PicksA, PicksB, Strikes));
	TestEqual(TEXT("exactly one victim per strike"), PicksA + PicksB, Strikes);
	TestEqual(TEXT("never no victim"), NoVictim, 0);
	TestTrue(FString::Printf(TEXT("A picked %d times (450-550)"), PicksA), PicksA >= 450 && PicksA <= 550);
	TestTrue(FString::Printf(TEXT("B picked %d times (450-550)"), PicksB), PicksB >= 450 && PicksB <= 550);
	TestEqual(TEXT("TargetPawn naming the concealed pawn gives the identical victim as TargetSpot"), RefMismatch, 0);
	TestTrue(TEXT("only BushSpotPick sub-seeds were asked"), View.Seeds.Asked.Num() > 0 && !View.Seeds.Asked.ContainsByPredicate([](ESeedDomain D) { return D != ESeedDomain::BushSpotPick; }));

	// Filter before pick: B out of F1 reach (raised beyond the vertical reach), A picked every time.
	Rig.Pawns.Of(B).Location.Z += 200.0;
	int32 OnlyA = 0;
	for (int32 Strike = 0; Strike < Strikes; ++Strike)
	{
		View.Seeds.Seed = 0xC2B2AE3D27D4EB4Full * static_cast<uint64>(Strike + 1);
		OnlyA += View.Strike(Attacker, FVector::ZeroVector, nullptr, FQueueKey(9000 + Strike, 1)).Victim == A ? 1 : 0;
	}
	TestEqual(TEXT("one candidate rejected by reach: the other in 1,000 of 1,000"), OnlyA, Strikes);

	// The draw: the secret stream, the CON_SPOT tag, UniformBelow; no modulo on the result, never RoundSeed.
	FString Source;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Assassination/Private/VictimResolverCore.cpp"));
	if (TestTrue(TEXT("the resolver source is readable"), FFileHelper::LoadFileToString(Source, *Path)))
	{
		TestTrue(TEXT("GetServerSubSeed(ESeedDomain::BushSpotPick, ...)"), Source.Contains(TEXT("GetServerSubSeed(ESeedDomain::BushSpotPick")));
		TestTrue(TEXT("UniformBelow draws the index"), Source.Contains(TEXT("StealthRandom::UniformBelow(Pick")));
		TestFalse(TEXT("no RoundSeed"), Source.Contains(TEXT("RoundSeed")));
		TestFalse(TEXT("no modulo of the reachable count"), Source.Contains(TEXT("% Reachable")) || Source.Contains(TEXT("%Reachable")));
	}
	TestEqual(TEXT("the stream tag is CON_SPOT"), FVictimResolverCore::BushSpotPickTag, StealthRandom::Tag("CON_SPOT"));
	return true;
}

#endif
