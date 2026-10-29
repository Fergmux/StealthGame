// Story con-003 (production/epics/concealment-bushes/story-003-conceal-state-machine-and-occupancy-record.md; GDD CON-R2,
// CON-R3, States and Transitions, AC-CON-30; ADR-0017 sections 1, 3, 7):
//   - StealthGame.Concealment.StateMachine: the legal moves through the real resolvers (enter, commit, exit, emerge, kill),
//     every transition taking a new generation; illegal requests change nothing; UConcealmentComponent never replicates and
//     holds no replicated property (the allowlist has zero rows for it); every pawn carries exactly one.
//   - StealthGame.Concealment.OccupancyRecord: the record holds exactly the Concealed pawns and the Exiting ones before
//     their crossing, with strictly increasing CommitSeq; 0, 1, 4 and 20 occupants are all accepted (no capacity).
//   - StealthGame.Concealment.TakeoverEligibility: IsInBushOrTransition is false only for Outside (Emerging included, D33).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "UObject/UnrealType.h"
#include "View/ConcealmentComponent.h"

using namespace StealthConcealmentTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentStateMachineTest, "StealthGame.Concealment.StateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentStateMachineTest::RunTest(const FString& Parameters)
{
	// Arrange: one pawn 30 cm from bush 1's open +X side.
	FRig Rig(TEXT("ConcealmentStateMachine"));
	AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);

	// Exit while Outside is refused (NotNow) and changes nothing.
	Rig.SubmitHide(Pawn, ECommandKind::HideExit, 0);
	Rig.Tick();
	TestEqual(TEXT("Outside: Exit is refused"), Rig.StateOf(Pawn), EConcealState::Outside);
	TestEqual(TEXT("... with NotNow"), Rig.RigQueue.CountRejected(ECommandRejectCode::NotNow), 1);
	TestTrue(TEXT("... and no record was created"), Rig.Core->FindRecord(Pawn) == nullptr || Rig.Core->FindRecord(Pawn)->TransitionGeneration == 0);

	// Outside -> Entering (generation 1) -> Concealed at the commit (the enter transition's generation).
	Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
	Rig.Tick();
	TestEqual(TEXT("HideEnter execution: Entering"), Rig.StateOf(Pawn), EConcealState::Entering);
	const uint32 EnterGen = Rig.Core->FindRecord(Pawn)->TransitionGeneration;
	TestEqual(TEXT("the enter transition takes a new generation"), EnterGen, 1u);
	Rig.Run(0.5);
	TestEqual(TEXT("Commit: Concealed"), Rig.StateOf(Pawn), EConcealState::Concealed);
	TestEqual(TEXT("the mirror on the pawn follows (UConcealmentComponent server mirror)"), Rig.Pawns.Of(Pawn).Mirror, EConcealState::Concealed);
	Rig.Run(0.5);

	// A second Enter while Concealed is refused (NotNow); an Exit is executed: Exiting, new generation.
	Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
	Rig.Tick();
	TestEqual(TEXT("Concealed: a second Enter is refused"), Rig.StateOf(Pawn), EConcealState::Concealed);
	Rig.SubmitHide(Pawn, ECommandKind::HideExit, 0);
	Rig.Tick();
	TestEqual(TEXT("Concealed -> Exiting"), Rig.StateOf(Pawn), EConcealState::Exiting);
	TestTrue(TEXT("the exit takes a new generation"), Rig.Core->FindRecord(Pawn)->TransitionGeneration > EnterGen);
	Rig.SubmitHide(Pawn, ECommandKind::HideExit, 0);
	Rig.Tick();
	TestEqual(TEXT("Exiting: a second Exit is Busy"), Rig.RigQueue.CountRejected(ECommandRejectCode::Busy), 1);
	Rig.Run(1.0);
	TestEqual(TEXT("Exiting -> Outside at End"), Rig.StateOf(Pawn), EConcealState::Outside);

	// Concealed -> Emerging (an Attack executed from inside), then Outside.
	Rig.Run(6.1); // the re-entry cooldown
	Rig.Pawns.Of(Pawn).Location = FVector(130.0, 0.0, 86.0);
	Rig.EnterAndSettle(Pawn);
	TestEqual(TEXT("re-entered: Concealed"), Rig.StateOf(Pawn), EConcealState::Concealed);
	const FQueueItem Cause = Rig.Q.MakeTimer(ETimerKind::ConcealmentTransition, 0);
	Rig.Core->OnAttackExecuted(*Pawn, Rig.Now() + 0.75, FQueueKey(), Cause);
	TestEqual(TEXT("Concealed -> Emerging"), Rig.StateOf(Pawn), EConcealState::Emerging);
	Rig.Run(2.0);
	TestEqual(TEXT("Emerging -> Outside"), Rig.StateOf(Pawn), EConcealState::Outside);

	// Entering -> Outside: killed before the commit (no record, the Commit item drops by generation).
	Rig.Run(6.1);
	Rig.Pawns.Of(Pawn).Location = FVector(130.0, 0.0, 86.0);
	Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
	Rig.Tick();
	Rig.Core->OnPawnKilled(*Pawn, FQueueKey());
	TestEqual(TEXT("Entering -> Outside (killed before the commit)"), Rig.StateOf(Pawn), EConcealState::Outside);
	Rig.Run(1.0);
	TestEqual(TEXT("the dropped Commit never conceals the visible victim"), Rig.StateOf(Pawn), EConcealState::Outside);

	// No illegal move was ever applied or faulted (every refusal above happened at the predicate).
	TestEqual(TEXT("no ConcealTransition fault"), Rig.Q.Faults.GetRecords().FilterByPredicate([](const FStealthFaultRecord& R) { return R.Fault == EStealthFault::ConcealTransition; }).Num(), 0);

	// The component: one per pawn, never replicated, no replicated property (zero allowlist rows).
	TArray<UConcealmentComponent*> Components;
	Pawn->GetComponents(Components);
	TestEqual(TEXT("exactly one UConcealmentComponent on the pawn"), Components.Num(), 1);
	TestTrue(TEXT("it is a default subobject named Concealment"), Components.Num() == 1 && Components[0]->GetFName() == UConcealmentComponent::ComponentName);
	TestFalse(TEXT("it does not replicate"), Components.Num() == 1 && Components[0]->GetIsReplicated());
	int32 NetProperties = 0;
	for (TFieldIterator<FProperty> It(UConcealmentComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		NetProperties += It->HasAnyPropertyFlags(CPF_Net) ? 1 : 0;
	}
	TestEqual(TEXT("no replicated property on UConcealmentComponent"), NetProperties, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentOccupancyRecordTest, "StealthGame.Concealment.OccupancyRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentOccupancyRecordTest::RunTest(const FString& Parameters)
{
	for (const int32 Count : { 0, 1, 4, 20 })
	{
		FRig Rig(*FString::Printf(TEXT("ConcealmentOccupancy%d"), Count));
		TArray<AStealthPawn*> Pawns;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			// Around the +X and +Y sides, inside the reach.
			Pawns.Add(Index % 2 == 0 ? Rig.SpawnAtReach(20.0, -80.0 + 8.0 * Index) : Rig.Spawn(FVector(-80.0 + 8.0 * Index, 120.0, 86.0)));
		}
		for (AStealthPawn* Pawn : Pawns)
		{
			Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
		}
		Rig.Run(1.0);
		TArray<APawn*> Record;
		Rig.Core->GetOccupancy(1, Record);
		TestEqual(FString::Printf(TEXT("%d entrants: all accepted (no capacity limit)"), Count), Record.Num(), Count);
		uint32 LastSeq = 0;
		bool bIncreasing = true;
		for (const APawn* Pawn : Record)
		{
			const uint32 Seq = Rig.Core->FindRecord(Pawn)->CommitSeq;
			bIncreasing &= Seq > LastSeq;
			LastSeq = Seq;
		}
		TestTrue(FString::Printf(TEXT("%d entrants: CommitSeq strictly increasing in record order"), Count), bIncreasing);
		if (Count == 0)
		{
			continue;
		}
		// The first occupant exits: in the record until its crossing, out after it.
		APawn* First = Record[0];
		Rig.SubmitHide(First, ECommandKind::HideExit, 0);
		Rig.Tick();
		Rig.Core->GetOccupancy(1, Record);
		TestTrue(FString::Printf(TEXT("%d: an Exiting pawn before its crossing stays in the record"), Count), Record.Contains(First));
		Rig.Run(Rig.Params.Public.BushExitCrossSec);
		Rig.Core->GetOccupancy(1, Record);
		TestFalse(FString::Printf(TEXT("%d: it leaves the record at the crossing"), Count), Record.Contains(First));
		TestEqual(FString::Printf(TEXT("%d: the others stay"), Count), Record.Num(), Count - 1);
		// Reset clears every record.
		Rig.Core->ResetAll();
		Rig.Core->GetOccupancy(1, Record);
		TestEqual(FString::Printf(TEXT("%d: reset empties the record"), Count), Record.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentTakeoverEligibilityTest, "StealthGame.Concealment.TakeoverEligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentTakeoverEligibilityTest::RunTest(const FString& Parameters)
{
	FRig Rig(TEXT("ConcealmentTakeover"));
	AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
	TestFalse(TEXT("Outside: not in a bush or transition"), Rig.Core->IsInBushOrTransition(Pawn));
	Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
	Rig.Tick();
	TestTrue(TEXT("Entering: true"), Rig.StateOf(Pawn) == EConcealState::Entering && Rig.Core->IsInBushOrTransition(Pawn));
	Rig.Run(1.0);
	TestTrue(TEXT("Concealed: true"), Rig.StateOf(Pawn) == EConcealState::Concealed && Rig.Core->IsInBushOrTransition(Pawn));
	Rig.SubmitHide(Pawn, ECommandKind::HideExit, 0);
	Rig.Tick();
	TestTrue(TEXT("Exiting: true"), Rig.StateOf(Pawn) == EConcealState::Exiting && Rig.Core->IsInBushOrTransition(Pawn));
	Rig.Run(1.0);
	TestFalse(TEXT("Outside again: false"), Rig.Core->IsInBushOrTransition(Pawn));
	Rig.Run(6.1);
	Rig.Pawns.Of(Pawn).Location = FVector(130.0, 0.0, 86.0);
	Rig.EnterAndSettle(Pawn);
	Rig.Core->OnAttackExecuted(*Pawn, Rig.Now() + 0.75, FQueueKey(), Rig.Q.MakeTimer(ETimerKind::ConcealmentTransition, 0));
	TestTrue(TEXT("Emerging: true (D33: an emerging pawn still counts as concealed for the ping refusal)"), Rig.StateOf(Pawn) == EConcealState::Emerging && Rig.Core->IsInBushOrTransition(Pawn));
	Rig.Run(0.4);
	TestTrue(TEXT("Emerging after its crossing: still true until fully out"), Rig.StateOf(Pawn) == EConcealState::Emerging && Rig.Core->IsInBushOrTransition(Pawn));
	Rig.Run(2.0);
	TestFalse(TEXT("fully out: false"), Rig.Core->IsInBushOrTransition(Pawn));
	return true;
}

#endif
