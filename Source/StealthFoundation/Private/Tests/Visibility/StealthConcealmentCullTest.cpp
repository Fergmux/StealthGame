// Concealment story con-025 (AC1 boundary rule, unit form; AC3 the un-cull in the same step) and IVP stories 024/025: the
// concealment cull's record state machine (ADR-0003 sections 5.1-5.6). The close itself (a channel per non-ally connection)
// needs a net driver and is observed by StealthGame.Concealment.PIE.PerViewerVisibility; here the deadline grid, the
// Visible -> CullPending -> Culled -> Visible chain and the per-viewer relevancy answer are pinned without one.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Server/Visibility/StealthConcealmentCull.h"

namespace StealthConcealmentCullTestLocal
{
	struct FScopedCullWorld
	{
		UWorld* World = nullptr;
		FScopedCullWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ConcealmentCullTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
		~FScopedCullWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthConcealmentCullDeadlineGridTest, "StealthGame.Concealment.CullDeadlineGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStealthConcealmentCullDeadlineGridTest::RunTest(const FString& Parameters)
{
	using namespace StealthConcealmentCullTestLocal;
	// Arrange: the production timing (15 Hz pawn net updates, ConcealCullDelayMs 250) on a grid anchored at Active entry 10 s.
	FScopedCullWorld Scoped;
	UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(Scoped.World);
	if (!TestNotNull(TEXT("the cull exists on an authority game world"), Cull))
	{
		return false;
	}
	Cull->SetTiming(15.0, 250.0);
	Cull->SetGridAnchor(10.0);
	const double Period = 1.0 / 15.0;

	// The first grid point at or after commit + 250 ms, for commits across one period.
	for (int32 Step = 0; Step < 20; ++Step)
	{
		const double Commit = 12.0 + Step * 0.005;
		const double Deadline = Cull->ComputeDeadline(Commit);
		const double Earliest = Commit + 0.25;
		const double GridIndex = (Deadline - 10.0) / Period;
		TestTrue(FString::Printf(TEXT("commit %.3f: the deadline %.4f is on the grid"), Commit, Deadline), FMath::IsNearlyEqual(GridIndex, FMath::RoundToDouble(GridIndex), 1e-6));
		TestTrue(FString::Printf(TEXT("commit %.3f: at or after commit + 250 ms"), Commit), Deadline >= Earliest - 0.001);
		TestTrue(FString::Printf(TEXT("commit %.3f: the first such grid point"), Commit), Deadline - Period < Earliest - 0.001);
	}
	// Every commit in one server frame lands on the same boundary.
	TestEqual(TEXT("two commits in one frame share the boundary"), Cull->ComputeDeadline(12.0101), Cull->ComputeDeadline(12.0101));
	// Boundary value: a commit exactly 250 ms before a grid point closes on that point (inclusive).
	const double OnGrid = 10.0 + 40.0 * Period;
	TestTrue(TEXT("a commit 250 ms before a grid point closes on it"), FMath::IsNearlyEqual(Cull->ComputeDeadline(OnGrid - 0.25), OnGrid, 1e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthConcealmentCullStateChainTest, "StealthGame.Concealment.CullStateChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStealthConcealmentCullStateChainTest::RunTest(const FString& Parameters)
{
	using namespace StealthConcealmentCullTestLocal;
	// Arrange: two pawns, the hider on side 0; one ally viewer and one enemy viewer (controllers carry the team).
	FScopedCullWorld Scoped;
	UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(Scoped.World);
	if (!TestNotNull(TEXT("the cull"), Cull))
	{
		return false;
	}
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* Hider = Scoped.World->SpawnActor<ADefaultPawn>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	APawn* Other = Scoped.World->SpawnActor<ADefaultPawn>(FVector(500.0, 0.0, 0.0), FRotator::ZeroRotator, Spawn);
	APlayerController* AllyViewer = Scoped.World->SpawnActor<APlayerController>(Spawn);
	APlayerController* EnemyViewer = Scoped.World->SpawnActor<APlayerController>(Spawn);
	if (!TestTrue(TEXT("the actors spawned"), Hider && Other && AllyViewer && EnemyViewer))
	{
		return false;
	}
	Cull->ConfigureTeams(
		[Hider](const APawn* Pawn) { return Pawn == Hider ? 0 : -1; },
		[AllyViewer, EnemyViewer](const AActor* Viewer) { return Viewer == AllyViewer ? 0 : (Viewer == EnemyViewer ? 1 : -1); });
	Cull->SetTiming(15.0, 250.0);
	Cull->SetGridAnchor(0.0);

	// Act + assert: the chain.
	TestTrue(TEXT("no record: Visible"), Cull->GetState(Hider) == EConcealCullState::Visible);
	TestTrue(TEXT("no record: relevant to the enemy"), Cull->IsRelevantTo(Hider, EnemyViewer));
	Cull->SetCullState(Hider, true, 1.0);
	const double Deadline = Cull->GetDeadline(Hider);
	TestTrue(TEXT("the commit: CullPending"), Cull->GetState(Hider) == EConcealCullState::CullPending);
	TestTrue(TEXT("CullPending: still relevant to the enemy (the close waits for the boundary)"), Cull->IsRelevantTo(Hider, EnemyViewer));
	Cull->TickCull(Deadline - 0.01);
	TestTrue(TEXT("a frame before the boundary: still CullPending"), Cull->GetState(Hider) == EConcealCullState::CullPending);
	Cull->TickCull(Deadline);
	TestTrue(TEXT("the boundary frame: Culled"), Cull->GetState(Hider) == EConcealCullState::Culled);
	TestFalse(TEXT("Culled: not relevant to the enemy"), Cull->IsRelevantTo(Hider, EnemyViewer));
	TestTrue(TEXT("Culled: relevant to the ally"), Cull->IsRelevantTo(Hider, AllyViewer));
	TestTrue(TEXT("an unrelated pawn is untouched"), Cull->GetState(Other) == EConcealCullState::Visible && Cull->IsRelevantTo(Other, EnemyViewer));
	// Un-cull in the same step (exit execution, a break, a concealed kill's death write): Visible at once.
	Cull->SetCullState(Hider, false, 2.0);
	TestTrue(TEXT("the un-cull: Visible in the same step"), Cull->GetState(Hider) == EConcealCullState::Visible);
	TestTrue(TEXT("the un-cull: relevant to the enemy again (no pending close on a connection here)"), Cull->IsRelevantTo(Hider, EnemyViewer));
	// A cancelled pending cull never closes.
	Cull->SetCullState(Hider, true, 3.0);
	const double SecondDeadline = Cull->GetDeadline(Hider);
	Cull->SetCullState(Hider, false, 3.1);
	Cull->TickCull(SecondDeadline + 0.1);
	TestTrue(TEXT("a cull cancelled before its boundary stays Visible"), Cull->GetState(Hider) == EConcealCullState::Visible);
	TestEqual(TEXT("no channel was closed (no net driver)"), Cull->GetClosedChannelCount(), 0);
	return true;
}

#endif
