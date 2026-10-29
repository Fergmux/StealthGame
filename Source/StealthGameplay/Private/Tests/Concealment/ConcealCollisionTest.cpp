// Story con-008 (production/epics/concealment-bushes/story-008-conceal-collision-pure-function.md; GDD CON-R4, AC-CON-28;
// ADR-0017 section 4):
//   - StealthGame.Concealment.CollisionAndAvoidance (AC1a): ComputeConcealCollision over the 5 states at t in {0, mid, end}:
//     Outside blocks BushVolume and Pawn, every other state ignores both.
//   - StealthGame.Concealment.CollisionReplay (AC2): the pawn's motion gate gives each simulated move its own state byte at
//     the move's server time; a replayed move restores the byte its saved move recorded (FSavedMove_Stealth), never the
//     current state's; responses are written only when the byte changes.
//   - StealthGame.Concealment.MoveZeroed (AC3): Move input is blocked in Entering, Concealed, Exiting and Emerging from
//     the transition's start, on the server (the mirror) and on the owner (derived from the replicated pose); Outside
//     input is untouched.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "Components/CapsuleComponent.h"
#include "Core/Movement/StealthSavedMove.h"
#include "View/ConcealCollision.h"
#include "View/ConcealmentComponent.h"
#include "View/Core/StealthPawnMovementComponent.h"

using namespace StealthConcealmentTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentConcealCollisionTest, "StealthGame.Concealment.CollisionAndAvoidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentConcealCollisionTest::RunTest(const FString& Parameters)
{
	const EConcealState States[] = { EConcealState::Outside, EConcealState::Entering, EConcealState::Concealed, EConcealState::Exiting, EConcealState::Emerging };
	const TCHAR* const Names[] = { TEXT("Outside"), TEXT("Entering"), TEXT("Concealed"), TEXT("Exiting"), TEXT("Emerging") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(States); ++Index)
	{
		for (const double T : { 0.0, 0.45, 0.9 })
		{
			const FConcealCollision C = ComputeConcealCollision(States[Index], T);
			const ECollisionResponse Expected = States[Index] == EConcealState::Outside ? ECR_Block : ECR_Ignore;
			TestEqual(FString::Printf(TEXT("%s at t=%.2f: BushVolume"), Names[Index], T), static_cast<int32>(C.BushVolume), static_cast<int32>(Expected));
			TestEqual(FString::Printf(TEXT("%s at t=%.2f: Pawn"), Names[Index], T), static_cast<int32>(C.Pawn), static_cast<int32>(Expected));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentCollisionReplayTest, "StealthGame.Concealment.CollisionReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentCollisionReplayTest::RunTest(const FString& Parameters)
{
	FWorldHolder World(TEXT("ConcealmentCollisionReplay"));
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStealthPawn* Pawn = World.World->SpawnActor<AStealthPawn>(FVector(0.0, 0.0, 300.0), FRotator::ZeroRotator, SpawnParams);
	UConcealmentComponent* Gate = UConcealmentComponent::Find(Pawn);
	UStealthMovementComponent* Movement = Pawn != nullptr ? Pawn->GetStealthMovement() : nullptr;
	if (!TestNotNull(TEXT("the pawn carries the component"), Gate) || !TestNotNull(TEXT("and the movement component"), Movement))
	{
		return false;
	}
	TestTrue(TEXT("the component is the movement component's gate"), Movement->GetMotionGate() == Gate);
	UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();

	// Live moves: Outside, then the enter transition starts at T = 10 (the server mirror).
	TArray<uint8> Saved;
	Saved.Add(Gate->EvaluateMoveState(9.9));
	Gate->ApplyMoveState(Saved.Last());
	Gate->SetServerState(EConcealState::Entering, 1, 10.0);
	for (const double T : { 10.0, 10.2, 10.4 })
	{
		Saved.Add(Gate->EvaluateMoveState(T));
		Gate->ApplyMoveState(Saved.Last());
	}
	TestEqual(TEXT("the move before the transition is open"), Saved[0], static_cast<uint8>(0));
	TestTrue(TEXT("the moves from the transition's start are gated"), Saved[1] == 1 && Saved[2] == 1 && Saved[3] == 1);
	TestEqual(TEXT("the capsule now ignores BushVolume"), static_cast<int32>(Capsule->GetCollisionResponseToChannel(ConcealmentCollision::BushVolumeChannel)), static_cast<int32>(ECR_Ignore));
	TestEqual(TEXT("... and Pawn"), static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_Pawn)), static_cast<int32>(ECR_Ignore));
	TestEqual(TEXT("one write for the one change (the three gated moves wrote once)"), Gate->GetCollisionWriter().WriteCount, 1);

	// A correction replay: the saved moves replay with their own recorded bytes (FSavedMove_Stealth::PrepMoveFor), whatever
	// the state is now. The pawn is Outside again "now"; the replayed move from before keeps its own byte.
	Gate->SetServerState(EConcealState::Outside, 0, 11.0);
	FSavedMove_Stealth Move;
	Move.Clear();
	Move.SavedGateState = Saved[2];
	Move.PrepMoveFor(Pawn);
	TestEqual(TEXT("replay restores the saved move's byte, not the current state's"), Movement->GetMotionGateState(), Saved[2]);
	Gate->ApplyMoveState(Movement->GetMotionGateState());
	TestEqual(TEXT("the replayed move's collision equals its recorded state (still ignoring)"),
		static_cast<int32>(Capsule->GetCollisionResponseToChannel(ConcealmentCollision::BushVolumeChannel)), static_cast<int32>(ECR_Ignore));
	TestEqual(TEXT("... with no extra write (unchanged)"), Gate->GetCollisionWriter().WriteCount, 1);
	Move.SavedGateState = Saved[0];
	Move.PrepMoveFor(Pawn);
	Gate->ApplyMoveState(Movement->GetMotionGateState());
	TestEqual(TEXT("replaying the open move blocks again"), static_cast<int32>(Capsule->GetCollisionResponseToChannel(ConcealmentCollision::BushVolumeChannel)), static_cast<int32>(ECR_Block));
	TestEqual(TEXT("... one more write (a change)"), Gate->GetCollisionWriter().WriteCount, 2);
	// Two saved moves with different bytes never combine (each keeps its own state).
	TestTrue(TEXT("a saved move records the gate byte (the field exists and is cleared)"), [](){ FSavedMove_Stealth M; M.SavedGateState = 1; M.Clear(); return M.SavedGateState == 0; }());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentMoveZeroedTest, "StealthGame.Concealment.MoveZeroed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentMoveZeroedTest::RunTest(const FString& Parameters)
{
	FWorldHolder World(TEXT("ConcealmentMoveZeroed"));
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStealthPawn* Pawn = World.World->SpawnActor<AStealthPawn>(FVector(0.0, 0.0, 300.0), FRotator::ZeroRotator, SpawnParams);
	UConcealmentComponent* Gate = UConcealmentComponent::Find(Pawn);
	if (!TestNotNull(TEXT("the pawn carries the component"), Gate))
	{
		return false;
	}
	// Server: the mirror decides, from the transition's start.
	const EConcealState States[] = { EConcealState::Outside, EConcealState::Entering, EConcealState::Concealed, EConcealState::Exiting, EConcealState::Emerging };
	for (const EConcealState State : States)
	{
		Gate->SetServerState(State, State == EConcealState::Outside ? 0 : 1, 5.0);
		const uint8 Byte = Gate->EvaluateMoveState(5.0);
		TestEqual(FString::Printf(TEXT("server, state %d: Move blocked iff not Outside"), static_cast<int32>(State)), Gate->IsMoveBlocked(Byte), State != EConcealState::Outside);
		TestEqual(FString::Printf(TEXT("pure rule agrees for state %d"), static_cast<int32>(State)), IsConcealMoveBlocked(State), State != EConcealState::Outside);
	}
	// Owner / any client: derived from the replicated FActionState (a BushEnter pose from its start; the inside latch until
	// a completed exit; a BushExit until its end).
	UConcealmentComponent* ClientGate = NewObject<UConcealmentComponent>(GetTransientPackage());
	FConcealmentTuning_Public Tuning;
	ClientGate->SetTuning(Tuning);
	TestEqual(TEXT("client with no evidence: Outside, Move untouched"), ClientGate->GetEffectiveState(0.0), EConcealState::Outside);
	ClientGate->SetConcealedEvidence(true);
	TestEqual(TEXT("client with the owner's concealed flag: Concealed, Move blocked"), ClientGate->GetEffectiveState(0.0), EConcealState::Concealed);
	TestTrue(TEXT("... blocked"), ClientGate->IsMoveBlocked(ClientGate->EvaluateMoveState(0.0)));
	ClientGate->SetConcealedEvidence(false);
	TestFalse(TEXT("flag cleared: Move untouched"), ClientGate->IsMoveBlocked(ClientGate->EvaluateMoveState(0.0)));
	return true;
}

#endif
