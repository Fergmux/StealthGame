// Story asn-014 AC2 (ADR-0018 LungeOutcomeBlind, section 2.2 last bullet, GDD ASN-R16): StealthGame.Assassination.
// LungeOutcomeBlind.
// Queue half: a hit, a whiff and a reversal with the same WorldPoint and attacker path, plus a fourth run (a hit) whose
// victim moves 150 cm during the window: from the slot to contact the attacker's replicated state carries exactly the
// slot's one FActionState write, byte-identical in all four runs: the lunge target is set once, at the slot, from the
// request and the attacker's capsule, and never re-tracks the victim.
// Engine half: the server lunge on a real Shared Pawn: WriteActionState(Strike) sets the local "StrikeContact" warp target
// at the pawn's location + WarpPoint on its UStealthMotionWarpingComponent (whose WarpTargets never replicate); moving the
// pawn afterwards (or anything else) leaves the target where the slot put it.
// Deviation (logged): the per-tick RepRootMotion / ReplicatedMovement byte capture is the IVP story 008 harness's; no
// strike clip exists before story asn-018, so the server lunge is the warp target alone (identical by construction).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "MotionWarpingComponent.h"
#include "Tests/Assassination/EncounterRoundRig.h"
#include "View/Core/StealthMotionWarpingComponent.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace LungeOutcomeBlindTestData
{
	using namespace StealthEncounterTest;

	const FVector WorldPoint(150.0, 0.0, 0.0);

	/** The attacker's writes from the slot up to contact, and the victim's travel in that window. */
	struct FLunge
	{
		TArray<FActionState> AttackerWrites;
		double VictimTravelCm = 0.0;
		bool bExecuted = false;
	};

	/** Outcome 0 hit, 1 whiff, 2 reversal; bMoveVictim moves the hit's victim 10 cm per tick in the window. */
	FLunge Run(int32 Outcome, bool bMoveVictim)
	{
		FLunge Out;
		FRoundEncounterRig R(TEXT("LungeOutcomeBlindWorld"), /*bProductionWrites*/ true);
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		APawn* A = R.Spawn(FVector::ZeroVector, 1);
		APawn* V = R.Spawn(Outcome == 1 ? FVector(150.0, 400.0, 0.0) : WorldPoint);
		APawn* Far = R.Spawn(FVector(2000.0, 0.0, 0.0));
		R.Drain({ R.Command(Outcome == 2 ? V : Far, ECommandKind::PrepareBegin) });
		R.PawnState.Writes.Reset();
		R.Drain({ R.Command(A, ECommandKind::Attack, WorldPoint) });
		Out.bExecuted = R.Core->GetResolutions().Num() == 1;
		const double Contact = Out.bExecuted ? R.Core->GetResolutions()[0].ContactServerTime : 0.0;
		const FVector Start = R.Pawns.Find(V)->Location;
		while (R.W.Rig.Clock.GetServerTimeSeconds() + 1e-9 < Contact)
		{
			if (bMoveVictim)
			{
				R.Pawns.Find(V)->Location += FVector(0.0, 10.0, 0.0);
			}
			R.Tick();
		}
		Out.VictimTravelCm = FVector::Dist(Start, R.Pawns.Find(V)->Location);
		for (const FRecordingPawnState::FWrite& Write : R.PawnState.For(A))
		{
			if (!Write.bDeath && Write.ServerTime <= Contact)
			{
				Out.AttackerWrites.Add(Write.State);
			}
		}
		return Out;
	}

	bool Same(const FLunge& A, const FLunge& B)
	{
		return A.AttackerWrites.Num() == 1 && B.AttackerWrites.Num() == 1 && A.AttackerWrites[0].Identical(&B.AttackerWrites[0], 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationLungeOutcomeBlindTest, "StealthGame.Assassination.LungeOutcomeBlind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationLungeOutcomeBlindTest::RunTest(const FString& Parameters)
{
	using namespace LungeOutcomeBlindTestData;

	// --- Queue half ------------------------------------------------------------------------------------------------------
	const FLunge Hit = Run(0, false);
	const FLunge Whiff = Run(1, false);
	const FLunge Reversal = Run(2, false);
	const FLunge Moved = Run(0, true);
	TestTrue(TEXT("all four strikes executed"), Hit.bExecuted && Whiff.bExecuted && Reversal.bExecuted && Moved.bExecuted);
	TestEqual(TEXT("hit: one attacker write from the slot to contact"), Hit.AttackerWrites.Num(), 1);
	TestTrue(TEXT("hit, whiff and reversal: the lunge write is identical"), Same(Hit, Whiff) && Same(Hit, Reversal));
	AddInfo(FString::Printf(TEXT("LUNGE moved-victim run: the victim travelled %.1f cm in the window"), Moved.VictimTravelCm));
	TestTrue(TEXT("the moving victim travelled >= 100 cm in the window"), Moved.VictimTravelCm >= 100.0);
	TestTrue(TEXT("the victim's motion changes nothing in the attacker's lunge (no re-track)"), Same(Hit, Moved));

	// --- Engine half: the server lunge's local warp target ----------------------------------------------------------------
	StealthAssassinationTest::FScopedWorld World(TEXT("LungeWarpWorld"));
	AStealthPawn* Attacker = World.SpawnPawn(0.0);
	if (!TestTrue(TEXT("the Shared Pawn has its non-replicated motion warping"), Attacker != nullptr && Attacker->GetMotionWarping() != nullptr))
	{
		return false;
	}
	const FVector Offset(150.0, -20.0, 0.0);
	FActionStateWrite Strike;
	Strike.Pose = EActionPose::Strike;
	Strike.StartServerTime = World.World->GetTimeSeconds();
	Strike.ContactServerTime = Strike.StartServerTime + 0.4;
	Strike.WarpPoint = FFixedWarpOffset::FromOffsetCm(Offset);
	const FVector SlotLocation = Attacker->GetActorLocation();
	Attacker->GetPawnCommand()->WriteActionState(Strike);
	const FMotionWarpingTarget* Target = Attacker->GetMotionWarping()->FindWarpTarget(AStealthPawn::StrikeWarpTargetName);
	TestTrue(TEXT("the server sets the local StrikeContact target at P_A + WarpPoint"), Target != nullptr && Target->GetLocation().Equals(SlotLocation + Offset, 0.05));
	Attacker->SetActorLocation(SlotLocation + FVector(0.0, 100.0, 0.0));
	Target = Attacker->GetMotionWarping()->FindWarpTarget(AStealthPawn::StrikeWarpTargetName);
	TestTrue(TEXT("re-set, never re-tracked: the target stays where the slot put it"), Target != nullptr && Target->GetLocation().Equals(SlotLocation + Offset, 0.05));
	TestTrue(TEXT("the strike pose is pose-locked in SCL"), Attacker->GetStealthMovement()->GetActionPose() == EStealthActionPose::Strike);
	return true;
}

#endif
