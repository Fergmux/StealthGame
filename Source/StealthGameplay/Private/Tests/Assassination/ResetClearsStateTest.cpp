// Story asn-019 (production/epics/assassination-defence-reversal/story-019-server-body-sequence-and-reset.md), AC3 /
// AC-ASN-41 ResetClearsState, with the QA rewrite: a fixture built through the rig's factory functions with two killed
// NPC bodies (their death writes staged, not yet applied: pending contact writes), a reversal's pending CounterPose, a
// running defence cooldown (owner value published) and two attacker lockouts. When Reset runs:
//   - every stamp of all five pawns is none (T_act unset, LockoutEnd -infinity, the window cleared) and every owner
//     cooldown is none;
//   - every strike role is Ready (the bodies, the strikers, the bracer, the reversed attacker and the counterer);
//   - no body remains (both torn down through the hand-over, once each; the body records cleared);
//   - no dropped write fires afterwards: the stale DeathWrite / CounterPose writes reaching the applier apply nothing, and
//     every reset-round TimerFired body item (and the stale CapEnd) resolves Voided with zero hand-over / observer calls,
//     even past the latest due time plus 1 s;
//   - Reset twice and Reset with zero bodies change nothing more.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationResetClearsStateTest, "StealthGame.Assassination.ResetClearsState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationResetClearsStateTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	FRig R(TEXT("ResetClearsStateWorld"), nullptr, /*bWithPawnState*/ true, /*bWithBodySequence*/ true);
	APawn* A = R.Spawn(FVector::ZeroVector, 0);
	APawn* B = R.Spawn(FVector(0.0, 2000.0, 0.0), 0);
	APawn* V1 = R.Spawn(FVector(150.0, 0.0, 0.0));
	APawn* V2 = R.Spawn(FVector(150.0, 2000.0, 0.0));
	APawn* Bracer = R.Spawn(FVector(0.0, -3000.0, 0.0), 1);
	APawn* Reverser = R.Spawn(FVector(150.0, 4000.0, 0.0), 1);
	APawn* Reversed = R.Spawn(FVector(0.0, 4000.0, 0.0), 0);

	R.BraceFrom(Bracer, 4, 1);           // a running cooldown (and an open window)
	R.BraceFrom(Reverser, 5, 2);
	R.AttackRef(A, V1);                  // body 1 (pending death write)
	R.AttackRef(B, V2);                  // body 2
	R.AttackRef(Reversed, Reverser);     // a reversal: the attacker's body, the defender's pending CounterPose
	TestEqual(TEXT("fixture: three bodies scheduled"), R.Core->GetBodies().Num(), 3);
	TestTrue(TEXT("fixture: a running cooldown, published to the owner"), R.State(Bracer)->GetLastPrepareActivation().IsSet() && R.OwnerCooldown.Get(Bracer) > 0.0);
	TestTrue(TEXT("fixture: two lockouts"), R.State(A)->IsLockedOut(R.Clock.GetServerTimeSeconds()) && R.State(B)->IsLockedOut(R.Clock.GetServerTimeSeconds()));
	TestTrue(TEXT("fixture: the bodies are Dead, the strikers Striking"), R.Core->IsDead(V1) && R.Core->IsDead(V2) && R.Core->GetRole(A) == EStrikeRole::Striking);
	const TArray<FStagedWrite> Staged = R.Queue.Staged;
	const TArray<FFakeQueue::FScheduled> Scheduled = R.Queue.Scheduled;
	TestTrue(TEXT("fixture: three death writes and a CounterPose pending"), Staged.Num() == 4);
	double LatestDue = 0.0;
	for (const FFakeQueue::FScheduled& Entry : Scheduled)
	{
		LatestDue = FMath::Max(LatestDue, Entry.Due);
	}

	R.Core->Reset();

	APawn* const All[] = { A, B, V1, V2, Bracer, Reverser, Reversed };
	for (APawn* Pawn : All)
	{
		const FDefenceStateCore* State = R.State(Pawn);
		TestTrue(FString::Printf(TEXT("%s: every stamp none"), *Pawn->GetName()), State == nullptr
			|| (!State->GetLastPrepareActivation().IsSet() && State->GetLockoutEnd() == -TNumericLimits<double>::Max() && !State->GetWindow().BeginKey.IsSet()));
		TestTrue(FString::Printf(TEXT("%s: role Ready"), *Pawn->GetName()), R.Core->GetRole(Pawn) == EStrikeRole::Ready);
	}
	TestTrue(TEXT("every owner cooldown is none"), R.OwnerCooldown.ResetCount == 1 && R.OwnerCooldown.Get(Bracer) < 0.0 && R.Defence.ResetCount == 1);
	TestTrue(TEXT("no body remains: each torn down once"), R.Core->GetBodies().IsEmpty() && R.HandOver.CountTornDown(V1) == 1 && R.HandOver.CountTornDown(V2) == 1
		&& R.HandOver.CountTornDown(Reversed) == 1 && R.HandOver.TornDown.Num() == 3);
	TestTrue(TEXT("the body generation moved on"), R.Core->GetBodyGeneration() == 2);

	// No dropped write fires afterwards: past the latest due time + 1 s, every stale write and item does nothing.
	R.Clock.AdvanceSeconds(LatestDue + 1.0 - R.Clock.GetServerTimeSeconds());
	for (const FStagedWrite& Write : Staged)
	{
		R.Core->ApplyStagedWrite(Write);
	}
	TestTrue(TEXT("the reset round's death and counter writes apply nothing"), R.Core->GetAppliedWrites().IsEmpty() && R.PawnState.Deaths.IsEmpty());
	int32 BodyItems = 0;
	int32 BodyItemsExecuted = 0;
	int32 CapEndsExecuted = 0;
	for (const FFakeQueue::FScheduled& Entry : Scheduled)
	{
		const EResolveResult Result = R.Core->Resolve(Entry.Item, R.NextKey());
		if (Entry.Item.Type == EQueueItemType::TimerFired)
		{
			++BodyItems;
			BodyItemsExecuted += Result == EResolveResult::Executed ? 1 : 0;
		}
		else if (const FHoldTimerPayload* Hold = Entry.Item.Payload.TryGet<FHoldTimerPayload>(); Hold != nullptr && Hold->ActionType == static_cast<uint8>(EActionType::Prepare))
		{
			CapEndsExecuted += Result == EResolveResult::Executed ? 1 : 0;
		}
	}
	TestTrue(TEXT("the fixture scheduled six body items (release and removal of three bodies)"), BodyItems == 6);
	TestEqual(TEXT("zero TimerFired body items execute after Reset"), BodyItemsExecuted, 0);
	TestEqual(TEXT("the stale CapEnds of the reset round execute nothing"), CapEndsExecuted, 0);
	TestEqual(TEXT("zero hand-over releases and zero observer calls after Reset"), R.HandOver.Released.Num() + R.RemovalObserver.Committed.Num(), 0);
	TestEqual(TEXT("no fault"), R.Faults.GetRecords().Num(), 0);

	// Reset twice, and with zero bodies: nothing more is torn down, nothing fails.
	R.Core->Reset();
	TestEqual(TEXT("Reset twice tears nothing more down"), R.HandOver.TornDown.Num(), 3);
	FRig Empty(TEXT("ResetClearsStateEmptyWorld"), nullptr, true, true);
	Empty.Core->Reset();
	TestTrue(TEXT("Reset with zero bodies"), Empty.HandOver.TornDown.IsEmpty() && Empty.Core->GetBodies().IsEmpty());
	return true;
}

#endif
