// Story asn-013 AC3 (AC-ASN-48, ADR-0018 section 3.4): StealthGame.Assassination.VictimUntouchedUntilWrite.
// Queue half (FRoundEncounterRig, production writes): a victim running across the attacker (d0 = 200 cm ahead, 250 cm/s
// sideways) is killed through an honoured PawnRef; from the slot to the death write the core writes nothing on it (no
// FActionState, no FDeathState, no freeze); the first write on the victim is the death write at T_dw. In a reversal the
// defender is never written dead (never Disabled): its only write is Counter.
// Engine half (a real AStealthPawn on a floor, AI-possessed, through the production FEncounterPawnStateWriter): a walking
// victim keeps its steady speed until the death write, then brakes by its own gait baseline (speed falls, the pawn still
// slides: no instant stop), with the freeze from the write's time; a reversed attacker whose Strike lunge is before contact
// records the freeze at its ContactServerTime and is not frozen yet; a Counter write never freezes the defender; two
// writes in one frame force one net update (ADR-0003 cap).
// Deviation (logged): the bystander-client replicated-movement comparison against an un-struck control run is a PIE
// capture (IVP story 008 harness); the server-side guarantee it rests on (nothing written on the victim before T_dw, the
// freeze only from the write) is what this test asserts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "AIController.h"
#include "Server/EncounterService.h"
#include "Tests/Assassination/EncounterRoundRig.h"
#include "Tests/Locomotion/LocomotionTestWorld.h"

namespace VictimUntouchedTestData
{
	constexpr int32 WindowTicks = 30;
	constexpr int32 WalkFrames = 60;
	constexpr int32 BrakeFrames = 20;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationVictimUntouchedUntilWriteTest, "StealthGame.Assassination.VictimUntouchedUntilWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationVictimUntouchedUntilWriteTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace VictimUntouchedTestData;
	const FEncounterParams Params = FEncounterParams::Defaults();
	const double Tick = RoundTestFixtures::RigFrameSec;

	// --- Queue half: kill of a moving victim, and a reversal -----------------------------------------------------------
	for (const bool bReversal : { false, true })
	{
		const TCHAR* Name = bReversal ? TEXT("reversal") : TEXT("kill");
		FRoundEncounterRig R(TEXT("VictimUntouchedWorld"), /*bProductionWrites*/ true);
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		APawn* A = R.Spawn(FVector::ZeroVector, 1);
		APawn* V = R.Spawn(FVector(200.0, 0.0, 0.0), 3);
		if (bReversal)
		{
			R.Drain({ R.Command(V, ECommandKind::PrepareBegin) });
		}
		else
		{
			R.Pawns.Find(V)->Velocity = FVector(0.0, 250.0, 0.0); // running across the attacker
		}
		R.PawnState.Writes.Reset();
		R.Drain({ R.AttackAt(A, V) });
		if (!TestTrue(FString::Printf(TEXT("%s: executed"), Name), R.Core->GetResolutions().Num() == 1))
		{
			continue;
		}
		const FAttackResolution& Resolution = R.Core->GetResolutions()[0];
		TestTrue(FString::Printf(TEXT("%s: the scripted outcome"), Name), Resolution.Outcome == (bReversal ? EAttackOutcome::Reversal : EAttackOutcome::Kill));
		const double DeathWriteTime = Resolution.ContactServerTime - Params.Server.StrikeNetLeadSec;
		for (int32 Index = 0; Index < WindowTicks; ++Index)
		{
			R.Tick();
		}
		const TArray<FRecordingPawnState::FWrite> OnV = R.PawnState.For(V);
		const int32 Early = OnV.FilterByPredicate([&](const FRecordingPawnState::FWrite& W) { return W.ServerTime < DeathWriteTime - Tick; }).Num();
		TestEqual(FString::Printf(TEXT("%s: nothing is written on the victim/defender between the slot and the death write"), Name), Early, 0);
		if (!bReversal)
		{
			TestTrue(TEXT("kill: the victim's first write is its death write, at T_dw +- 1 tick"), OnV.Num() >= 1 && OnV[0].bDeath
				&& FMath::Abs(OnV[0].ServerTime - DeathWriteTime) <= Tick + 1e-9);
			TestTrue(TEXT("kill: KillServerTime = GetScheduledDeathServerTime = ContactServerTime"), R.Kills.Num() == 1 && R.Kills[0].ScheduledDeath.IsSet()
				&& R.Kills[0].ScheduledDeath.GetValue() == Resolution.ContactServerTime);
		}
		else
		{
			TestTrue(TEXT("reversal: the defender is never written dead (never Disabled)"), !OnV.ContainsByPredicate([](const FRecordingPawnState::FWrite& W) { return W.bDeath; }));
			TestTrue(TEXT("reversal: its one write is Counter, at T_dw +- 1 tick"), OnV.Num() == 1 && OnV[0].State.Pose == EActionPose::Counter
				&& FMath::Abs(OnV[0].ServerTime - DeathWriteTime) <= Tick + 1e-9);
		}
	}

	// --- Engine half: the freeze on a real pawn -------------------------------------------------------------------------
	{
		StealthLocomotionTest::FWorld TestWorld(TEXT("VictimUntouchedPawnWorld"));
		AStealthPawn* Victim = TestWorld.SpawnPawn(FVector2D::ZeroVector);
		AStealthPawn* Attacker = TestWorld.SpawnPawn(FVector2D(0.0, 600.0));
		AStealthPawn* Defender = TestWorld.SpawnPawn(FVector2D(0.0, -600.0));
		for (AStealthPawn* Pawn : { Victim, Attacker, Defender })
		{
			AAIController* Controller = TestWorld.World->SpawnActor<AAIController>();
			Controller->Possess(Pawn);
			for (int32 Frame = 0; Frame < 30; ++Frame)
			{
				StealthLocomotionTest::StepPawn(Pawn); // settle on the floor
			}
		}
		FEncounterPawnStateWriter Writer;

		// The victim walks at a steady speed, untouched; then the death write.
		double SpeedBefore = 0.0;
		for (int32 Frame = 0; Frame < WalkFrames; ++Frame)
		{
			StealthLocomotionTest::DriveMove(Victim, 0.0f, 1.0f, EGait::Walk);
			StealthLocomotionTest::StepPawn(Victim);
			SpeedBefore = StealthLocomotionTest::Speed2D(Victim);
		}
		TestFalse(TEXT("before the write: not frozen"), Victim->GetStealthMovement()->IsCommandedMotionFrozen());
		FDeathState Death;
		Death.DeathServerTime = TestWorld.World->GetTimeSeconds() + Params.Server.StrikeNetLeadSec;
		const uint32 ForcedBefore = Victim->GetForcedNetUpdateCount();
		TestTrue(TEXT("the death write applies on the server"), Writer.WriteDeath(Victim, Death));
		TestTrue(TEXT("FDeathState written once"), Victim->HasDeathWrite() && Victim->GetDeathState().DeathServerTime == Death.DeathServerTime && !Writer.WriteDeath(Victim, Death));
		TestEqual(TEXT("one forced net update (DeathWrite)"), Victim->GetForcedNetUpdateCount() - ForcedBefore, 1u);
		TestTrue(TEXT("the victim is frozen from the write"), Victim->GetStealthMovement()->IsCommandedMotionFrozen());
		const FVector At = Victim->GetActorLocation();
		double SpeedAfter = SpeedBefore;
		bool bMonotonic = true;
		for (int32 Frame = 0; Frame < BrakeFrames; ++Frame)
		{
			StealthLocomotionTest::DriveMove(Victim, 0.0f, 1.0f, EGait::Walk); // its input keeps coming: ignored
			StealthLocomotionTest::StepPawn(Victim);
			const double Speed = StealthLocomotionTest::Speed2D(Victim);
			bMonotonic &= Speed <= SpeedAfter + 1e-3;
			SpeedAfter = Speed;
		}
		AddInfo(FString::Printf(TEXT("UNTOUCHED victim speed %.1f cm/s before the write, %.1f after %d frames, slid %.1f cm"), SpeedBefore, SpeedAfter, BrakeFrames, FVector::Dist2D(At, Victim->GetActorLocation())));
		TestTrue(TEXT("walking steadily before the write"), SpeedBefore > 100.0);
		TestTrue(TEXT("after the write it brakes (speed never rises, and falls)"), bMonotonic && SpeedAfter < SpeedBefore - 10.0);
		TestTrue(TEXT("by its baseline braking, not an instant stop"), FVector::Dist2D(At, Victim->GetActorLocation()) > 1.0);

		// A reversed attacker mid-lunge: the freeze waits for its contact.
		FActionStateWrite Strike;
		Strike.Pose = EActionPose::Strike;
		Strike.StartServerTime = TestWorld.World->GetTimeSeconds();
		Strike.ContactServerTime = Strike.StartServerTime + Params.Public.StrikeContactSec;
		const uint32 AttackerForced = Attacker->GetForcedNetUpdateCount();
		Attacker->GetPawnCommand()->WriteActionState(Strike);
		FActionStateWrite Again = Strike;
		Attacker->GetPawnCommand()->WriteActionState(Again); // the same frame
		TestEqual(TEXT("two writes in one frame force one net update"), Attacker->GetForcedNetUpdateCount() - AttackerForced, 1u);
		TestEqual(TEXT("Seq advanced per write"), static_cast<int32>(Attacker->GetActionState().Seq), 3);
		FDeathState Reversed;
		Reversed.DeathServerTime = Strike.ContactServerTime;
		Writer.WriteDeath(Attacker, Reversed);
		TestTrue(TEXT("reversed attacker: the freeze is recorded at its ContactServerTime"), Attacker->GetStealthMovement()->GetFrozenFromServerTime() == Strike.ContactServerTime);
		TestFalse(TEXT("reversed attacker: not frozen before contact (the lunge is never interrupted)"), Attacker->GetStealthMovement()->IsCommandedMotionFrozen());

		// The defender's Counter never freezes it.
		FActionStateWrite Counter;
		Counter.Pose = EActionPose::Counter;
		Counter.ContactServerTime = Strike.ContactServerTime;
		Writer.WriteActionState(Defender, Counter);
		TestTrue(TEXT("defender: Counter written, mapped to the SCL Counter pose"), Defender->GetActionState().Pose == EActionPose::Counter
			&& Defender->GetStealthMovement()->GetActionPose() == EStealthActionPose::Counter);
		TestTrue(TEXT("defender: never Disabled"), !Defender->HasDeathWrite() && Defender->GetStealthMovement()->GetFrozenFromServerTime() < 0.0);
	}
	return true;
}

#endif
