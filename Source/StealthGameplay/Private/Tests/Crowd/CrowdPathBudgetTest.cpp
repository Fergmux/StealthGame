// Story cnb-007 (production/epics/crowd-npc-behaviour/story-007-path-source-and-start-budget.md): the path source and
// the path-start budget (ADR-0016 section 4.1; GDD CNB-R3, R15, F5).
//   StartBudget     50 simultaneous requests: at most CrowdPathStartsPerFrameLimit (2) starts per frame, FIFO, none
//                   dropped (every request started exactly once); a newer leg of one pawn replaces its pending one.
//   AcceptanceGate  a result is accepted only when the leg id AND the control epoch match; while waiting the follower
//                   holds its Move (at most CrowdPathInvalidHoldSec, then Move(0)).
//   PrebakedEdges   a leg along a CrowdRoute edge uses the pre-baked path with zero async queries (fixture source).
//   NoQueryBeforeLayout  the Director starts no query while the navmesh is not ready (no layout in a bare world), and
//                   records no navmesh rebuild during a round.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Server/CrowdDirector.h"
#include "Server/CrowdFollower.h"
#include "Server/CrowdPathBudget.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace CrowdPathBudgetTestData
{
	/** A fixture route graph: one CrowdRoute edge (0,0) -> (3000,0) with two pre-baked corners; counts async starts. */
	class FFixturePathSource final : public ICrowdPathSource
	{
	public:
		int32 AsyncStarts = 0;
		virtual bool TryPrebakedEdge(const FVector& From, const FVector& To, TArray<FVector>& OutCorners) override
		{
			if (From.Equals(FVector::ZeroVector, 1.0) && To.Equals(FVector(3000.0, 0.0, 0.0), 1.0))
			{
				OutCorners = { FVector(1000.0, 200.0, 0.0), FVector(2000.0, 200.0, 0.0), To };
				return true;
			}
			return false;
		}
		virtual void StartQuery(const FCrowdPathRequest& Request) override { ++AsyncStarts; }
	};

	/** The path layer as the Director drives it: pre-baked edge first, else a budgeted async query. */
	bool RequestThrough(ICrowdPathSource& Source, FCrowdPathBudget& Budget, const FCrowdPathRequest& Request, TArray<FVector>& OutCorners)
	{
		if (Source.TryPrebakedEdge(Request.From, Request.To, OutCorners))
		{
			return true;
		}
		Budget.Enqueue(Request);
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPathStartBudgetTest, "StealthGame.Crowd.Path.StartBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdPathStartBudgetTest::RunTest(const FString& Parameters)
{
	const int32 Limit = FCrowdTuning_Server().CrowdPathStartsPerFrameLimit;
	FCrowdPathBudget Budget;
	for (uint32 Ordinal = 0; Ordinal < 50; ++Ordinal)
	{
		FCrowdPathRequest Request;
		Request.Ordinal = Ordinal;
		Request.LegId = 1;
		Budget.Enqueue(Request);
	}
	TArray<uint32> StartedOrder;
	int32 MaxPerFrame = 0;
	int32 Frames = 0;
	while (Budget.NumPending() > 0 && Frames < 1000)
	{
		++Frames;
		// A hitch frame (every 7th) still honours the cap.
		const int32 Started = Budget.StartThisFrame(Limit, [&StartedOrder](const FCrowdPathRequest& Request) { StartedOrder.Add(Request.Ordinal); });
		MaxPerFrame = FMath::Max(MaxPerFrame, Started);
	}
	TestEqual(TEXT("the cap is CrowdPathStartsPerFrameLimit (2)"), Limit, 2);
	TestTrue(TEXT("at most 2 starts per frame"), MaxPerFrame <= Limit);
	TestEqual(TEXT("all 50 requests started (deferred, never dropped)"), StartedOrder.Num(), 50);
	TestEqual(TEXT("in 25 frames"), Frames, 25);
	bool bFifo = true;
	for (int32 Index = 0; Index < StartedOrder.Num(); ++Index)
	{
		bFifo &= StartedOrder[Index] == static_cast<uint32>(Index);
	}
	TestTrue(TEXT("FIFO order"), bFifo);
	TestEqual(TEXT("started count = enqueued count"), Budget.GetStartedCount(), Budget.GetEnqueuedCount());

	// A newer leg of the same pawn replaces its pending request in place (its old leg is gone).
	FCrowdPathBudget Replace;
	FCrowdPathRequest First;
	First.Ordinal = 4;
	First.LegId = 1;
	Replace.Enqueue(First);
	FCrowdPathRequest Second = First;
	Second.LegId = 2;
	Replace.Enqueue(Second);
	uint32 SeenLeg = 0;
	Replace.StartThisFrame(2, [&SeenLeg](const FCrowdPathRequest& Request) { SeenLeg = Request.LegId; });
	TestEqual(TEXT("one pending request per pawn: the newest leg"), SeenLeg, 2u);
	TestEqual(TEXT("the replacement is counted"), Replace.GetReplacedCount(), int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPathAcceptanceGateTest, "StealthGame.Crowd.Path.AcceptanceGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdPathAcceptanceGateTest::RunTest(const FString& Parameters)
{
	FCrowdPathResult Result;
	Result.LegId = 7;
	Result.ControlEpoch = 3;
	TestTrue(TEXT("matching leg and epoch: accepted"), FCrowdPathBudget::Accept(Result, 7, 3));
	TestFalse(TEXT("an old leg: discarded"), FCrowdPathBudget::Accept(Result, 8, 3));
	TestFalse(TEXT("a previous driver's epoch: discarded"), FCrowdPathBudget::Accept(Result, 7, 4));

	// While waiting for a result the follower holds its current Move, then Move(0) after CrowdPathInvalidHoldSec.
	const FCrowdTuning_Server Tuning;
	const FLocomotionTuning_Public Locomotion;
	FCrowdDraws Draws(0x7);
	FCrowdFollower Follower;
	Follower.Initialize(1, 0.0, Tuning, Draws);
	const FVector Goal(2000.0, 0.0, 0.0);
	Follower.StartLeg(1, FVector::ZeroVector, MakeArrayView(&Goal, 1), 0.0, Tuning, Draws);
	FCrowdPerceivedPawn Self;
	Self.Key = 1;
	Self.VelocityCmPerSec = FVector(140.0, 0.0, 0.0);
	const FCrowdFollowerOutput Moving = Follower.Tick(0.1, 1.0f / 60.0f, Self, {}, Tuning, Locomotion, Draws);
	TestTrue(TEXT("walking on the leg"), Moving.Magnitude > 0.0f);
	Follower.BeginWaitForPath(0.2);
	const FCrowdFollowerOutput Held = Follower.Tick(0.2 + 0.4, 1.0f / 60.0f, Self, {}, Tuning, Locomotion, Draws);
	TestTrue(TEXT("within the hold the last Move is kept"), Held.Magnitude == Moving.Magnitude && Held.WorldDirection.Equals(Moving.WorldDirection, 1e-6));
	const FCrowdFollowerOutput Released = Follower.Tick(0.2 + Tuning.CrowdPathInvalidHoldSec + 0.05, 1.0f / 60.0f, Self, {}, Tuning, Locomotion, Draws);
	TestEqual(TEXT("after CrowdPathInvalidHoldSec: Move(0)"), Released.Magnitude, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPathPrebakedEdgesTest, "StealthGame.Crowd.Path.PrebakedEdges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdPathPrebakedEdgesTest::RunTest(const FString& Parameters)
{
	using namespace CrowdPathBudgetTestData;
	FFixturePathSource Source;
	FCrowdPathBudget Budget;
	FCrowdPathRequest Edge;
	Edge.From = FVector::ZeroVector;
	Edge.To = FVector(3000.0, 0.0, 0.0);
	TArray<FVector> Corners;
	TestTrue(TEXT("an edge leg is answered at once"), RequestThrough(Source, Budget, Edge, Corners));
	TestEqual(TEXT("with the pre-baked corners"), Corners.Num(), 3);
	Budget.StartThisFrame(2, [&Source](const FCrowdPathRequest& Request) { Source.StartQuery(Request); });
	TestEqual(TEXT("0 FindPathAsync calls for an edge leg"), Source.AsyncStarts, 0);
	FCrowdPathRequest Free;
	Free.From = FVector(10.0, 10.0, 0.0);
	Free.To = FVector(800.0, 900.0, 0.0);
	TestFalse(TEXT("an off-edge leg is queued"), RequestThrough(Source, Budget, Free, Corners));
	Budget.StartThisFrame(2, [&Source](const FCrowdPathRequest& Request) { Source.StartQuery(Request); });
	TestEqual(TEXT("one async query for the off-edge leg"), Source.AsyncStarts, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPathNoQueryBeforeLayoutTest, "StealthGame.Crowd.Path.NoQueryBeforeLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdPathNoQueryBeforeLayoutTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CrowdPathLayoutWorld"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	UCrowdDirector* Director = UCrowdDirector::Get(World);
	if (TestNotNull(TEXT("Director"), Director))
	{
		FCrowdPathRequest Request;
		Request.Ordinal = 1;
		Request.LegId = 1;
		TArray<FVector> Corners;
		TestFalse(TEXT("an async request is queued"), Director->RequestPath(Request, Corners));
		for (int32 Frame = 0; Frame < 10; ++Frame)
		{
			Director->TickDirector(1.0f / 60.0f);
		}
		TestEqual(TEXT("no query was started without a navmesh (no OnLayoutReady)"), Director->GetPathBudget().GetStartedCount(), int64(0));
		TestEqual(TEXT("the request waits, never dropped"), Director->GetPathBudget().NumPending(), 1);
		TestEqual(TEXT("no navmesh rebuild recorded"), Director->GetNavRebuildsDuringRound(), 0);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
