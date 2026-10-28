// Story oc-005 (production/epics/objectives-chain/story-005-service-shell-and-composition-root.md), AC2 (ADR-0020 section 1)
// ServiceHasNoTickTimerActor: UObjectivesService driven through Preparing (ValidateSelectedChain), OnLayoutReady, Active
// (with a full tend through the queue) and Ended in a bare world registers no tick function (it is neither an
// FTickableGameObject nor a UTickableWorldSubsystem and no Objectives source overrides Tick or IsTickable), uses no timer
// manager and no latent action (the source scan of Features/Objectives finds no GetWorldTimerManager / GetTimerManager /
// SetTimer / FLatentActionInfo / FLatentActionManager), and spawns nothing (world spawn count 0, the source names no
// SpawnActor). The only Objectives timer is a queue item (the rival release, Round's queue, stories oc-014/015). A seeded
// violation (a Tick override with a timer call) fails the same scan. The include-lint half is ObjectivesIncludeLint (oc-022).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"
#include "ObjectivesSourceScan.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"

namespace ObjectivesServiceHasNoTickTimerActorTestData
{
	const TArray<FString> Forbidden = {
		TEXT("Tick"), TEXT("IsTickable"), TEXT("GetTickableTickType"), TEXT("GetWorldTimerManager"), TEXT("GetTimerManager"), TEXT("SetTimer"),
		TEXT("SetTimerForNextTick"), TEXT("FTimerManager"), TEXT("FLatentActionInfo"), TEXT("FLatentActionManager"), TEXT("SpawnActor"),
		TEXT("FTickableGameObject"), TEXT("UTickableWorldSubsystem"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesServiceHasNoTickTimerActorTest, "StealthGame.Objectives.ServiceHasNoTickTimerActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesServiceHasNoTickTimerActorTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace ObjectivesServiceHasNoTickTimerActorTestData;

	// The class shape.
	static_assert(!std::is_base_of_v<FTickableGameObject, UObjectivesService>, "UObjectivesService must not be tickable");
	TestFalse(TEXT("not a UTickableWorldSubsystem"), UObjectivesService::StaticClass()->IsChildOf(UTickableWorldSubsystem::StaticClass()));

	// Driven through Preparing, Active (a tend to completion) and Ended in a world: nothing is spawned.
	{
		FQueueRig Rig;
		AStealthPawn* Tender = Rig.Pawn(CourtP1Team0, 0, 1, 0);
		int32 Spawned = 0;
		const FDelegateHandle Handle = Rig.World.World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([&Spawned](AActor*) { ++Spawned; }));
		TestTrue(TEXT("Preparing -> layout -> Active"), Rig.ToActive());
		Rig.Begin(Tender, CourtP1Team0);
		Rig.Run(3.5);
		Rig.Service->HandleRoundEnded();
		Rig.World.World->RemoveOnActorSpawnedHandler(Handle);
		TestEqual(TEXT("the tend counted (the service ran inside its slots)"), Rig.Logged(ERoundLogType::ObjectiveStage), 1);
		TestEqual(TEXT("spawn count 0"), Spawned, 0);
	}

	// The source scan of the feature folder (and the stand-in hold, which runs only in queue slots too).
	TArray<StealthObjectivesSourceScan::FSource> Sources = StealthObjectivesSourceScan::LoadSources(TEXT("StealthGameplay/Features/Objectives"));
	TestTrue(TEXT("the Objectives sources were found"), Sources.Num() >= 8);
	const TArray<FString> Findings = StealthObjectivesSourceScan::FindNames(Sources, Forbidden);
	TestEqual(TEXT("no tick, timer manager, latent action or spawn in Objectives"), Findings.Num(), 0);
	for (const FString& Finding : Findings)
	{
		AddError(Finding);
	}

	// Negative control.
	StealthObjectivesSourceScan::FSource Seeded;
	Seeded.Path = TEXT("StealthGameplay/Features/Objectives/Private/Seeded.cpp");
	Seeded.Text = TEXT("void UObjectivesService::Tick(float Delta) { GetWorld()->GetTimerManager().SetTimer(Handle, 1.0f, false); }");
	TestTrue(TEXT("seeded: a Tick override with a timer call is reported"), StealthObjectivesSourceScan::FindNames({ Seeded }, Forbidden).Num() >= 3);
	return true;
}

#endif
