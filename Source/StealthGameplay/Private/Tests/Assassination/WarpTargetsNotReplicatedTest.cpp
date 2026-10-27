// Story asn-014 AC1, reflection half (ADR-0018 WarpTargetsNotReplicated, section 7.2, E5): StealthGame.Assassination.
// WarpTargetsNotReplicated. Generic audit: every AStealthPawn class loaded in the process (C++ and Blueprint) is spawned in a
// bare world and every UMotionWarpingComponent on it is checked: the Shared Pawn's UStealthMotionWarpingComponent has the
// WarpTargets lifetime row COND_Never; any other motion-warping component (the sample Blueprint's, on the D53 prototype
// adapter) is not replicated at all after PostInitializeComponents. A pawn spawned later has the same rows, and the
// component class set is the same for every instance of a class. Control: the engine parent's WarpTargets row is
// replicated, and a plain UMotionWarpingComponent added before FinishSpawning (as the sample Blueprint's is) is turned off.
// The wire half (no motion-warping record reaches a client after a strike) is StealthGame.Assassination.PIE.OutcomeBlindWire.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Engine/Blueprint.h"
#include "MotionWarpingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Tests/Assassination/AssassinationTestWorld.h"
#include "UObject/UObjectIterator.h"
#include "View/Core/StealthMotionWarpingComponent.h"

namespace WarpTargetsNotReplicatedTestData
{
	ELifetimeCondition LifetimeConditionOf(const UClass* Class, FName Property)
	{
		// ClassReps is built lazily (normally by the first net driver that replicates the class).
		const_cast<UClass*>(Class)->SetUpRuntimeReplicationData();
		TArray<FLifetimeProperty> Props;
		Class->GetDefaultObject()->GetLifetimeReplicatedProps(Props);
		for (const FLifetimeProperty& Row : Props)
		{
			if (Class->ClassReps.IsValidIndex(Row.RepIndex) && Class->ClassReps[Row.RepIndex].Property->GetFName() == Property)
			{
				return Row.Condition;
			}
		}
		return COND_Max;
	}

	/** True when no motion-warping component on Pawn can send WarpTargets; OutClasses lists the component classes. */
	bool AuditPawn(const AActor* Pawn, TArray<const UClass*>& OutClasses, FString& OutWhy)
	{
		OutClasses.Reset();
		TInlineComponentArray<UMotionWarpingComponent*> Warping(Pawn);
		bool bOk = true;
		for (const UMotionWarpingComponent* Component : Warping)
		{
			OutClasses.Add(Component->GetClass());
			if (Component->IsA<UStealthMotionWarpingComponent>())
			{
				if (LifetimeConditionOf(Component->GetClass(), TEXT("WarpTargets")) != COND_Never)
				{
					bOk = false;
					OutWhy += FString::Printf(TEXT(" %s: WarpTargets row not COND_Never;"), *Component->GetName());
				}
			}
			else if (Component->GetIsReplicated())
			{
				bOk = false;
				OutWhy += FString::Printf(TEXT(" %s (%s): replicated;"), *Component->GetName(), *Component->GetClass()->GetName());
			}
		}
		OutClasses.Sort([](const UClass& A, const UClass& B) { return A.GetFName().LexicalLess(B.GetFName()); });
		return bOk;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationWarpTargetsNotReplicatedTest, "StealthGame.Assassination.WarpTargetsNotReplicated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationWarpTargetsNotReplicatedTest::RunTest(const FString& Parameters)
{
	using namespace WarpTargetsNotReplicatedTestData;
	StealthAssassinationTest::FScopedWorld World(TEXT("WarpTargetsAuditWorld"));

	// Control: the engine parent replicates WarpTargets (the audit has something to catch).
	const ELifetimeCondition Parent = LifetimeConditionOf(UMotionWarpingComponent::StaticClass(), TEXT("WarpTargets"));
	TestTrue(TEXT("control: UMotionWarpingComponent.WarpTargets is replicated in the engine parent"), Parent != COND_Never && Parent != COND_Max);
	TestTrue(TEXT("UStealthMotionWarpingComponent.WarpTargets is COND_Never"), LifetimeConditionOf(UStealthMotionWarpingComponent::StaticClass(), TEXT("WarpTargets")) == COND_Never);

	// Every concrete C++ AStealthPawn class loaded (Blueprint classes are covered by the PIE audit on live pawns).
	int32 Audited = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(AStealthPawn::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
			|| Class->ClassGeneratedBy != nullptr || Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* First = World.World->SpawnActor<AActor>(Class, FVector(Audited * 500.0, 0.0, 200.0), FRotator::ZeroRotator, Params);
		AActor* Later = World.World->SpawnActor<AActor>(Class, FVector(Audited * 500.0, 300.0, 200.0), FRotator::ZeroRotator, Params);
		if (!TestTrue(FString::Printf(TEXT("%s spawns"), *Class->GetName()), First != nullptr && Later != nullptr))
		{
			continue;
		}
		TArray<const UClass*> FirstClasses, LaterClasses;
		FString Why;
		TestTrue(FString::Printf(TEXT("%s: no motion-warping component can send WarpTargets%s"), *Class->GetName(), *Why), AuditPawn(First, FirstClasses, Why));
		TestTrue(FString::Printf(TEXT("%s: a pawn spawned later has the same rows"), *Class->GetName()), AuditPawn(Later, LaterClasses, Why) && FirstClasses == LaterClasses);
		AddInfo(FString::Printf(TEXT("WARPAUDIT %s: %d motion-warping component(s)"), *Class->GetName(), FirstClasses.Num()));
		++Audited;
	}
	TestTrue(TEXT("at least AStealthPawn was audited"), Audited >= 1);
	const AStealthPawn* Shared = World.SpawnPawn(9000.0);
	TestTrue(TEXT("the Shared Pawn carries UStealthMotionWarpingComponent"), Shared != nullptr && Shared->GetMotionWarping() != nullptr
		&& Shared->GetWarpingForStrike() == Shared->GetMotionWarping());

	// The D53 adapter case: a plain UMotionWarpingComponent present before PostInitializeComponents (the sample Blueprint's
	// SCS component) is not replicated once the pawn has finished spawning.
	{
		const FTransform At(FVector(9500.0, 0.0, 200.0));
		AStealthPawn* Pawn = World.World->SpawnActorDeferred<AStealthPawn>(AStealthPawn::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		UMotionWarpingComponent* Sample = Pawn != nullptr ? NewObject<UMotionWarpingComponent>(Pawn, TEXT("SampleMotionWarping")) : nullptr;
		if (TestNotNull(TEXT("deferred pawn with a sample-style component"), Sample))
		{
			Sample->SetIsReplicated(true);
			Pawn->AddInstanceComponent(Sample);
			Pawn->FinishSpawning(At);
			TArray<const UClass*> Classes;
			FString Why;
			TestTrue(TEXT("the sample-style component is no longer replicated after PostInitializeComponents"), !Sample->GetIsReplicated() && AuditPawn(Pawn, Classes, Why));
			TestTrue(TEXT("the strike lunge still uses the Shared component when both exist"), Pawn->GetWarpingForStrike() == Pawn->GetMotionWarping());
		}
	}
	return true;
}

#endif
