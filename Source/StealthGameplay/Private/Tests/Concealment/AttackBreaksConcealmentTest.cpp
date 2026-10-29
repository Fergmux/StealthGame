// Story con-014 (production/epics/concealment-bushes/story-014-attack-breaks-concealment.md; GDD CON-R9, F3(c), AC-CON-13
// server part; ADR-0017 section 3):
//   - StealthGame.Concealment.BreakAtExecution (AC1): a concealed pawn's Attack execution (any target, a co-occupant, or a
//     whiff: the break does not depend on the outcome) un-culls it in the Attack's own slot and makes it Emerging; the
//     emerge side is the open side nearest the aim yaw (tie to the lower index).
//   - StealthGame.Concealment.EmergeTiming (AC2): the crossing is at ContactServerTime - StrikeContactSec = execution +
//     BushStrikeEmergeSec (0.35 s); the pawn leaves the record and the flags clear at the crossing; it ends Outside when the
//     strike's recovery ends (AttackAttemptLockoutSec), the re-entry cooldown starting there.
//   - StealthGame.Concealment.InsideInputs (AC3): a Prepare or an Interact submitted inside is no break (the pawn stays
//     Concealed, nothing un-culls); Run inside does nothing (Move is blocked; no dash-out).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "View/ConcealCollision.h"

using namespace StealthConcealmentTest;

namespace ConcealmentAttackBreakTestData
{
	/** Submits an Attack whose slot runs the Assassination-side hook: the break at execution (hub OnActionStarted(Attack)). */
	void SubmitAttack(FRig& Rig, APawn* Attacker, double ContactAfterSec)
	{
		Rig.Resolver.OnOtherCommand = [&Rig, ContactAfterSec](const FQueueItem& Item, FQueueKey Key)
		{
			const FCommandPayload& Cmd = Item.Payload.Get<FCommandPayload>();
			if (static_cast<ECommandKind>(Cmd.Kind) == ECommandKind::Attack)
			{
				Rig.Core->OnAttackExecuted(*Cmd.Pawn.Pawn.Get(), Rig.Now() + ContactAfterSec, Key, Item);
				return EResolveResult::Executed;
			}
			return EResolveResult::Voided; // Prepare / Interact: not Concealment's (and no break)
		};
		FCommandPayload Cmd;
		Cmd.Pawn.Pawn = Attacker;
		Cmd.Kind = static_cast<uint8>(ECommandKind::Attack);
		Rig.Q.Queue->Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::PawnCommand, Rig.Q.Epoch.Current, FMailboxId::ServerLocal(), Cmd));
	}

	double ContactAfter(const FConcealmentParams& P) { return static_cast<double>(P.Public.BushStrikeEmergeSec) + P.StrikeContactSec; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentBreakAtExecutionTest, "StealthGame.Concealment.BreakAtExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentBreakAtExecutionTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentAttackBreakTestData;
	// Aim yaws and the expected emerge side (all four sides open; 45 is a tie between 0 and 1: the lower index).
	struct FCase
	{
		double AimYaw;
		uint8 Side;
	};
	for (const FCase Case : { FCase{ 0.0, 0 }, FCase{ 90.0, 1 }, FCase{ 200.0, 2 }, FCase{ -80.0, 3 }, FCase{ 45.0, 0 } })
	{
		FRig Rig(*FString::Printf(TEXT("ConcealmentBreak%.0f"), Case.AimYaw));
		AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
		Rig.EnterAndSettle(Hider);
		Rig.Pawns.Of(Hider).LookYawDeg = Case.AimYaw;
		SubmitAttack(Rig, Hider, ContactAfter(Rig.Params));
		Rig.Tick();
		const FQueueKey AttackKey = Rig.Q.Queue->GetLastDrainRecords().Last().Key;
		TestEqual(FString::Printf(TEXT("aim %.0f: Emerging at execution"), Case.AimYaw), Rig.StateOf(Hider), EConcealState::Emerging);
		TestEqual(FString::Printf(TEXT("aim %.0f: un-culled once"), Case.AimYaw), Rig.Cull.CountFor(Hider, false), 1);
		TestTrue(FString::Printf(TEXT("aim %.0f: in the Attack's own slot"), Case.AimYaw), Rig.Cull.Calls.Last().Key == AttackKey);
		TestEqual(FString::Printf(TEXT("aim %.0f: emerge side"), Case.AimYaw), Rig.Core->FindRecord(Hider)->Side, Case.Side);
	}
	// A whiff and a strike at a co-occupant break identically (the break reads no outcome; the hook never sees one).
	FRig Rig(TEXT("ConcealmentBreakWhiff"));
	AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
	AStealthPawn* CoOccupant = Rig.SpawnAtReach(30.0, 30.0);
	Rig.EnterAndSettle(Hider);
	Rig.EnterAndSettle(CoOccupant);
	SubmitAttack(Rig, Hider, ContactAfter(Rig.Params));
	Rig.Tick();
	TestEqual(TEXT("whiff or co-occupant target: the same break"), Rig.StateOf(Hider), EConcealState::Emerging);
	TestEqual(TEXT("the co-occupant stays Concealed"), Rig.StateOf(CoOccupant), EConcealState::Concealed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentEmergeTimingTest, "StealthGame.Concealment.EmergeTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentEmergeTimingTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentAttackBreakTestData;
	FRig Rig(TEXT("ConcealmentEmergeTiming"));
	const FConcealmentParams& P = Rig.Params;
	AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
	Rig.Relations.Bind(Hider, 13, 0);
	Rig.EnterAndSettle(Hider);
	SubmitAttack(Rig, Hider, ContactAfter(P));
	Rig.Tick();
	const double T0 = Rig.Now();
	double CrossAt = -1.0, EndAt = -1.0;
	for (int32 Frame = 0; Frame < 150; ++Frame)
	{
		const bool bInRecord = Rig.Core->FindRecord(Hider)->bInOccupancy;
		const EConcealState Before = Rig.StateOf(Hider);
		Rig.Tick();
		if (bInRecord && !Rig.Core->FindRecord(Hider)->bInOccupancy)
		{
			CrossAt = Rig.Now();
			TestTrue(TEXT("the crossing clears both flags"), !Rig.Flags.OwnerOf(13) && !Rig.Flags.TeamOf(13));
		}
		if (Before == EConcealState::Emerging && Rig.StateOf(Hider) == EConcealState::Outside)
		{
			EndAt = Rig.Now();
		}
	}
	TestEqual(TEXT("Cross at ContactServerTime - StrikeContactSec = t0 + 0.35 s"), CrossAt - T0, static_cast<double>(P.Public.BushStrikeEmergeSec), FrameSec + 1e-6);
	TestEqual(TEXT("Outside when the strike's recovery ends (t0 + AttackAttemptLockoutSec)"), EndAt - T0, P.AttackLockoutSec, FrameSec + 1e-6);
	TestEqual(TEXT("one Emerge log row"), Rig.Count(EConcealmentLogEvent::Emerge), 1);
	TestEqual(TEXT("the re-entry cooldown starts at the emergence's completion"), Rig.Core->FindRecord(Hider)->LastExitCompleteServerTime, EndAt, FrameSec + 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentInsideInputsTest, "StealthGame.Concealment.InsideInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentInsideInputsTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentAttackBreakTestData;
	FRig Rig(TEXT("ConcealmentInsideInputs"));
	AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
	Rig.EnterAndSettle(Hider);
	const int32 CullCalls = Rig.Cull.Calls.Num();
	Rig.Resolver.OnOtherCommand = [](const FQueueItem&, FQueueKey) { return EResolveResult::Voided; };
	for (const ECommandKind Kind : { ECommandKind::PrepareBegin, ECommandKind::InteractBegin })
	{
		FCommandPayload Cmd;
		Cmd.Pawn.Pawn = Hider;
		Cmd.Kind = static_cast<uint8>(Kind);
		Rig.Q.Queue->Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::PawnCommand, Rig.Q.Epoch.Current, FMailboxId::ServerLocal(), Cmd));
		Rig.Tick();
		TestEqual(FString::Printf(TEXT("command %d inside: no break, still Concealed"), static_cast<int32>(Kind)), Rig.StateOf(Hider), EConcealState::Concealed);
	}
	TestEqual(TEXT("nothing un-culled"), Rig.Cull.Calls.Num(), CullCalls);
	// Run inside: no displacement source exists while Concealed (Move is blocked from the transition's start).
	Rig.Pawns.Of(Hider).bRunning = true;
	TestTrue(TEXT("Concealed: Move blocked (no dash-out)"), IsConcealMoveBlocked(Rig.StateOf(Hider)));
	TestEqual(TEXT("Run changes no state"), Rig.StateOf(Hider), EConcealState::Concealed);
	return true;
}

#endif
