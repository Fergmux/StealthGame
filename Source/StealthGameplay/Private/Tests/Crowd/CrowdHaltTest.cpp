// Story cnb-031 (production/epics/crowd-npc-behaviour/story-031-halt-and-handover-epoch.md): Halt at the round-end step
// and the handover epoch discipline (GDD CNB-R12, R10.5, AC-CNB-22, AC-CNB-38; ADR-0016 section 7).
//   EndedHold        (AC-CNB-22, headless crowd at 1/60 s) 50 NPCs moving, running, tending, braced, hiding and
//                    traversing when Round's round-end step calls Halt: every agent issues Move(0) in that step, every
//                    pawn reaches rest within RunStopSec plus the traversal remainder with no deceleration above the
//                    baseline, and afterwards no emergence, absorption, reaction, tell or discrete command happens.
//   HandoverEpoch    (AC-CNB-38a, a game world with the real factory and controllers) 100 takeovers (control epoch bumped
//                    in the drain) with late Moves, Looks, debounced Ends and path callbacks of the old driver: 0 reach
//                    the pawn; 100 hand-backs (ReleaseControl) detach only at a Director frame start and never while
//                    the pawn is in a traversal.
//   HaltSynchronous  (AC-CNB-38b) 50 Halts with commands pending: Halt latches every sink inside the call, before any
//                    further crowd tick, and after it only Move(0) and Look reach a pawn.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Server/Core/StealthPawnControlQuery.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/CrowdDirector.h"
#include "Server/StealthCrowdController.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace CrowdHaltTestData
{
	class FHooks final : public IStealthPawnFactoryHooks
	{
	public:
		virtual void ApplyAppearance(AStealthPawn* Pawn, int32 AppearanceId) override {}
		virtual ERosterResult BindPawn(FParticipantId Participant, AStealthPawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch) override { return ERosterResult::NoOp_NotBound; }
		virtual AController* CreateAiController(AStealthPawn* Pawn) override { return Pawn->GetWorld()->SpawnActor<AStealthCrowdController>(); }
	};

	struct FTestWorld
	{
		UWorld* World = nullptr;
		FHooks Hooks;
		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CrowdHaltTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			UStealthPawnFactory::Get(World)->SetHooks(&Hooks, World);
			UStealthPawnFactory::Get(World)->SetDefaultPawnClass(AStealthPawn::StaticClass());
		}
		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		AStealthPawn* Spawn(const FVector& Location)
		{
			FStealthSpawnRequest Request;
			Request.Transform = FTransform(Location + FVector(0.0, 0.0, AStealthPawn::SampleCapsuleHalfHeightCm + 2.0));
			Request.ControlKind = EStealthPawnControlKind::Ai;
			return UStealthPawnFactory::Get(World)->SpawnSharedPawn(Request).Pawn;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdEndedHoldTest, "StealthGame.Crowd.Halt.EndedHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdEndedHoldTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	using namespace StealthCrowdHerringTest;
	FConfig Config;
	Config.HalfSizeCm = 3000.0;
	Config.EnabledTellMask = FConfig::AllTells;
	// Busy herrings, so the Halt finds pawns running, tending, braced, hiding and traversing.
	Config.Tuning.CrowdRunIntervalSec = 60.0f;
	Config.Tuning.CrowdVaultIntervalSec = 90.0f;
	Config.Tuning.CrowdDecoyTendIntervalSec = 120.0f;
	Config.Tuning.CrowdBraceIntervalSec = 120.0f;
	FWorld World(Config);
	World.AddReferenceHerringSites();
	for (int32 Index = 0; Index < 13; ++Index)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * Index / 13.0;
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
		World.AddSocket(Out * 2700.0, Out * 2850.0);
	}
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(150.0);
	TMap<FCrowdAgent::EHerringMode, int32> ModesAtHalt;
	int32 Running = 0, Traversing = 0;
	for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
	{
		const FNpc& Npc = World.GetNpc(Index);
		ModesAtHalt.FindOrAdd(Npc.Agent.GetHerringMode())++;
		Running += Npc.Body.Gait == EGait::Run && Npc.Body.Motion.Speed > 300.0f ? 1 : 0;
		Traversing += Npc.Body.bTraversing ? 1 : 0;
	}
	const int32 EventsBefore = World.GetSocketEvents().Num();
	const int32 TellsBefore = Recorder.Performed.Num();
	const int32 CommandsBefore = World.GetCommandLog().Num();
	const double HaltAt = World.GetNow();
	// Round's round-end step: Halt in one call.
	World.Halt();
	int32 NonZeroMove = 0;
	for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
	{
		NonZeroMove += !World.GetNpc(Index).Body.bGone && World.GetNpc(Index).Body.MoveIntent.Size() > 0.0 ? 1 : 0;
	}
	const FLocomotionTuning_Public& Loco = World.GetLocomotionTuning();
	const double MaxDecel = StealthLocomotion::RunDeceleration(Loco);
	TMap<uint32, float> Last;
	TMap<uint32, double> RestAt;
	int32 OverDecel = 0;
	while (World.GetNow() < HaltAt + 10.0)
	{
		World.EmitStrike(World.GetNpc(0).Body.Location, 0); // strikes keep coming: no reaction may start
		World.Step();
		for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
		{
			const FNpc& Npc = World.GetNpc(Index);
			if (Npc.Body.bGone || Npc.Body.bTraversing)
			{
				continue;
			}
			if (const float* Prev = Last.Find(Npc.Body.Ordinal); Prev != nullptr)
			{
				OverDecel += (*Prev - Npc.Body.Motion.Speed) > MaxDecel * Config.StepSec * 1.02 + 1.0 ? 1 : 0;
			}
			Last.Add(Npc.Body.Ordinal, Npc.Body.Motion.Speed);
			if (Npc.Body.Motion.Speed <= 1.0f && !RestAt.Contains(Npc.Body.Ordinal))
			{
				RestAt.Add(Npc.Body.Ordinal, World.GetNow() - HaltAt);
			}
		}
	}
	double Slowest = 0.0;
	int32 NeverRested = 0;
	for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
	{
		const FNpc& Npc = World.GetNpc(Index);
		if (Npc.Body.bGone)
		{
			continue;
		}
		const double* At = RestAt.Find(Npc.Body.Ordinal);
		NeverRested += At == nullptr ? 1 : 0;
		Slowest = At != nullptr ? FMath::Max(Slowest, *At) : Slowest;
	}
	int32 Reactions = 0;
	for (const FCommandRecord& Record : World.GetCommandLog())
	{
		Reactions += Record.Time > HaltAt + 1e-9 ? 1 : 0;
	}
	// The bound: RunStopSec plus the longest traversal remainder (the sim's climb is 1.4 s) plus a step.
	const double Bound = Loco.RunStopSec + 1.4 + Config.StepSec;
	AddInfo(FString::Printf(TEXT("ENDEDHOLD Halt at %.1f s with %d running, %d traversing, %d tending, %d braced, %d hidden, %d exiting: non-zero Move after the Halt call %d, slowest to rest %.2f s (bound %.2f), never at rest %d, decelerations over the baseline %d; after the Halt: socket events %d, tells %d, commands %d"),
		HaltAt, Running, Traversing, ModesAtHalt.FindRef(FCrowdAgent::EHerringMode::DecoyHold) + ModesAtHalt.FindRef(FCrowdAgent::EHerringMode::DecoyBeginCheck),
		ModesAtHalt.FindRef(FCrowdAgent::EHerringMode::BraceHold), ModesAtHalt.FindRef(FCrowdAgent::EHerringMode::BushHidden),
		ModesAtHalt.FindRef(FCrowdAgent::EHerringMode::ExitWalk) + ModesAtHalt.FindRef(FCrowdAgent::EHerringMode::ExitWait), NonZeroMove, Slowest, Bound, NeverRested, OverDecel,
		World.GetSocketEvents().Num() - EventsBefore, Recorder.Performed.Num() - TellsBefore, World.GetCommandLog().Num() - CommandsBefore));
	TestEqual(TEXT("every AI-driven pawn's Move is cleared in the round-end step"), NonZeroMove, 0);
	TestEqual(TEXT("every pawn reaches rest"), NeverRested, 0);
	TestTrue(TEXT("within RunStopSec plus the traversal remainder"), Slowest <= Bound);
	TestEqual(TEXT("no deceleration above the baseline"), OverDecel, 0);
	TestEqual(TEXT("no emergence or absorption afterwards"), World.GetSocketEvents().Num() - EventsBefore, 0);
	TestEqual(TEXT("no tell afterwards"), Recorder.Performed.Num() - TellsBefore, 0);
	TestEqual(TEXT("no discrete command afterwards (no reaction, no herring)"), Reactions, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdHandoverEpochTest, "StealthGame.Crowd.Halt.HandoverEpoch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdHandoverEpochTest::RunTest(const FString& Parameters)
{
	using namespace CrowdHaltTestData;
	FTestWorld TestWorld;
	UCrowdDirector* Director = UCrowdDirector::Get(TestWorld.World);
	UStealthPawnFactory* Factory = UStealthPawnFactory::Get(TestWorld.World);
	int32 Reached = 0, Takeovers = 0;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		AStealthPawn* Pawn = TestWorld.Spawn(FVector(150.0 * (Index % 10), 150.0 * (Index / 10), 0.0));
		AStealthCrowdController* Old = Cast<AStealthCrowdController>(Pawn->GetController());
		const uint32 OldEpoch = Factory->GetControlEpoch(Pawn);
		// The takeover's commit in Round's drain bumps the control epoch (ADR-0010); the old driver's work is in flight.
		Factory->BumpControlEpoch(Pawn);
		++Takeovers;
		const int64 Before = Old->GetSinkCounters().Passed + Old->GetSinkCounters().DiscretePassed;
		Old->Move(FVector2D(0.3, 0.9), EGait::Walk);
		Old->Look(FRotator(0.0, 10.0, 0.0));
		Old->PrepareEnd();
		Old->InteractEnd();
		FCrowdPathResult Late;
		Late.Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
		Late.LegId = 1;
		Late.ControlEpoch = OldEpoch;
		Late.bValid = true;
		Late.Corners = { FVector(800.0, 0.0, 0.0) };
		Old->GetAgent().OnPathResult(Late, *Director);
		Reached += static_cast<int32>(Old->GetSinkCounters().Passed + Old->GetSinkCounters().DiscretePassed - Before);
	}
	// Hand-backs: ReleaseControl detaches at the next Director frame start, and never during a traversal.
	int32 DuringTraversal = 0, NotDeferred = 0, Detached = 0;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		AStealthPawn* Pawn = TestWorld.Spawn(FVector(150.0 * (Index % 10), 2000.0 + 150.0 * (Index / 10), 0.0));
		AStealthCrowdController* Controller = Cast<AStealthCrowdController>(Pawn->GetController());
		UStealthMovementComponent* Movement = Pawn->GetStealthMovement();
		const bool bTraversing = Index % 2 == 0;
		if (bTraversing)
		{
			Movement->SetMovementMode(UStealthMovementComponent::TraversalMovementMode);
		}
		Director->ReleaseControl(Pawn);
		NotDeferred += Controller->GetAgent().GetActivity() == ECrowdActivity::Halted || Director->GetPendingReleaseCount() == 0 ? 1 : 0;
		Director->TickDirector(1.0f / 60.0f);
		const bool bLatchedNow = Controller->GetAgent().GetActivity() == ECrowdActivity::Halted;
		DuringTraversal += bTraversing && bLatchedNow ? 1 : 0;
		if (bTraversing)
		{
			Movement->SetMovementMode(MOVE_Walking);
			Director->TickDirector(1.0f / 60.0f);
		}
		Detached += Controller->GetAgent().GetActivity() == ECrowdActivity::Halted ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("HANDOVER %d takeovers with late commands: reached the pawn %d; 100 hand-backs: applied before a frame start %d, during a traversal %d, detached %d"),
		Takeovers, Reached, NotDeferred, DuringTraversal, Detached));
	TestEqual(TEXT("0 old-epoch commands reach any pawn"), Reached, 0);
	TestEqual(TEXT("a hand-back waits for the Director's frame start"), NotDeferred, 0);
	TestEqual(TEXT("never during a traversal"), DuringTraversal, 0);
	TestEqual(TEXT("every hand-back detaches"), Detached, 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdHaltSynchronousTest, "StealthGame.Crowd.Halt.HaltSynchronous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdHaltSynchronousTest::RunTest(const FString& Parameters)
{
	using namespace CrowdHaltTestData;
	int32 NotLatched = 0, MotionAfter = 0, Halts = 0;
	for (int32 Round = 0; Round < 50; ++Round)
	{
		FTestWorld TestWorld;
		UCrowdDirector* Director = UCrowdDirector::Get(TestWorld.World);
		TArray<AStealthCrowdController*> Controllers;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Controllers.Add(Cast<AStealthCrowdController>(TestWorld.Spawn(FVector(200.0 * Index, 200.0 * Round, 0.0))->GetController()));
		}
		// Commands pending in the frame: Halt inside the round-end step, then the same frame's later calls.
		Director->Halt();
		++Halts;
		for (AStealthCrowdController* Controller : Controllers)
		{
			NotLatched += Controller->GetAgent().GetActivity() == ECrowdActivity::Halted ? 0 : 1;
			const int64 Before = Controller->GetSinkCounters().Passed + Controller->GetSinkCounters().DiscretePassed;
			Controller->Move(FVector2D(0.0, 1.0), EGait::Run);
			Controller->PrepareBegin();
			Controller->Traverse(FTraversalHint());
			MotionAfter += static_cast<int32>(Controller->GetSinkCounters().Passed + Controller->GetSinkCounters().DiscretePassed - Before);
			// Move(0) and Look stay allowed after Halt.
			const int64 Allowed = Controller->GetSinkCounters().Passed;
			Controller->Move(FVector2D::ZeroVector, EGait::Walk);
			Controller->Look(FRotator(0.0, 5.0, 0.0));
			MotionAfter += Controller->GetSinkCounters().Passed - Allowed == 2 ? 0 : 1;
		}
	}
	AddInfo(FString::Printf(TEXT("HALTSYNC %d Halts: sinks not latched inside the call %d, non-Move(0) commands passed after Halt (or Move(0)/Look refused) %d"), Halts, NotLatched, MotionAfter));
	TestEqual(TEXT("Halt latches every sink inside the call"), NotLatched, 0);
	TestEqual(TEXT("after Halt only Move(0) and Look reach a pawn"), MotionAfter, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
