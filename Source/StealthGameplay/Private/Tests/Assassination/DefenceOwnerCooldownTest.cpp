// Story asn-016 (production/epics/assassination-defence-reversal/story-016-defence-owner-cooldown-component.md), AC2
// (ASN-R18 / ADR-0018 section 1.5) and the reflection half of AC1 (AC-ASN-36; the traffic half is
// StealthGame.Assassination.PIE.DefenceStateServerOnly), with the QA rewrite and an injected clock:
//   - a Prepare(Begin) executing at t = 100 with DefenceCooldownSec 60 publishes the owner's CooldownEndServerTime 160.0,
//     once; re-presses inside the window (t = 102) and the cooldown (t = 110, and one tick before 160) are voided Cooldown
//     and write nothing; at the cooldown end (inclusive, F3) a press executes again and publishes 220.0;
//   - UDefenceOwnerComponent on a controller: SyncFromPawn copies the pawn's stamp end (160.0), a pawn that never braced
//     gives none (-1); a PawnRecovery re-pawn copies the lost pawn's end (160.0) or, the stamp unknown, stamps now: at
//     t = 130 the owner shows 190.0;
//   - reflection: the pawn-side UDefenceStateComponent declares no replicated property and is not replicated; the owner
//     component's only replicated property is CooldownEndServerTime, registered COND_OwnerOnly.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Server/DefenceStateComponent.h"
#include "Tests/Assassination/EncounterTestRig.h"
#include "UObject/UnrealType.h"
#include "View/DefenceOwnerComponent.h"

namespace DefenceOwnerCooldownTestData
{
	constexpr double Tick = 1.0 / 60.0;

	/** The names of the CPF_Net properties Class itself declares (not its super's). */
	TArray<FString> OwnReplicatedProperties(const UClass* Class)
	{
		TArray<FString> Out;
		for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Net))
			{
				Out.Add(It->GetName());
			}
		}
		return Out;
	}

	int32 LifetimeCount(const UObject* Object, ELifetimeCondition Condition)
	{
		TArray<FLifetimeProperty> Props;
		Object->GetLifetimeReplicatedProps(Props);
		return Props.FilterByPredicate([Condition](const FLifetimeProperty& P) { return P.Condition == Condition; }).Num();
	}

	UDefenceOwnerComponent* AddOwnerComponent(AActor& Controller)
	{
		UDefenceOwnerComponent* Owner = NewObject<UDefenceOwnerComponent>(&Controller, TEXT("DefenceOwner"));
		Controller.AddInstanceComponent(Owner);
		Owner->RegisterComponent();
		return Owner;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationDefenceOwnerCooldownTest, "StealthGame.Assassination.DefenceOwnerCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationDefenceOwnerCooldownTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace DefenceOwnerCooldownTestData;

	// The slot publication (the resolver over its seams; the clock starts at t = 100).
	{
		FRig R(TEXT("DefenceOwnerCooldownSlotWorld"));
		APawn* P = R.Spawn(FVector::ZeroVector, 0);
		TestTrue(TEXT("the rig's cooldown is the 60 s default"), FMath::IsNearlyEqual(R.Params.Public.DefenceCooldownSec, 60.0f));
		R.BraceFrom(P, 2, 1);
		TestTrue(TEXT("Prepare(Begin) at t = 100 publishes CooldownEndServerTime 160.0, once"), R.OwnerCooldown.Writes.Num() == 1 && R.OwnerCooldown.Get(P) == 160.0);
		R.Advance(2.0);
		TestTrue(TEXT("a re-press inside the window (t = 102) is voided Cooldown"), R.BraceFrom(P, 2, 2) == EResolveResult::Voided && R.LastReply().Code == ECommandRejectCode::Cooldown);
		R.Advance(8.0);
		TestTrue(TEXT("a re-press in the cooldown (t = 110) is voided Cooldown"), R.BraceFrom(P, 2, 3) == EResolveResult::Voided);
		R.Clock.AdvanceSeconds(50.0 - Tick);
		TestTrue(TEXT("a re-press one tick before the cooldown end is voided Cooldown"), R.BraceFrom(P, 2, 4) == EResolveResult::Voided);
		TestTrue(TEXT("no re-press wrote the owner value (still 160.0)"), R.OwnerCooldown.Writes.Num() == 1 && R.OwnerCooldown.Get(P) == 160.0);
		R.Clock.AdvanceSeconds(Tick + 1e-6);
		TestTrue(TEXT("at the cooldown end a press executes again and publishes 220.0"), R.BraceFrom(P, 2, 5) == EResolveResult::Executed
			&& R.OwnerCooldown.Writes.Num() == 2 && FMath::IsNearlyEqual(R.OwnerCooldown.Get(P), 220.0, 1e-5));
	}

	// The owner component on a controller: sync on re-pawn and PawnRecovery.
	{
		StealthAssassinationTest::FScopedWorld World(TEXT("DefenceOwnerCooldownWorld"));
		APlayerController* Controller = World.World->SpawnActor<APlayerController>();
		UDefenceOwnerComponent* Owner = AddOwnerComponent(*Controller);
		TestTrue(TEXT("Find answers the controller's component"), UDefenceOwnerComponent::Find(Controller) == Owner);
		TestEqual(TEXT("none by default"), Owner->GetCooldownEndServerTime(), -1.0);

		AStealthPawn* Braced = World.SpawnPawn(0.0);
		UDefenceStateComponent::FindOrAdd(*Braced)->GetCore().BeginPrepare(100.0, 60.0, FQueueKey(1, 1));
		Owner->SyncFromPawn(Braced, 60.0);
		TestEqual(TEXT("re-pawn onto a braced pawn copies its end 160.0"), Owner->GetCooldownEndServerTime(), 160.0);
		TestEqual(TEXT("the owner's remaining cooldown at t = 130 is 30 s"), Owner->GetCooldownRemaining(130.0), 30.0);

		AStealthPawn* Fresh = World.SpawnPawn(500.0);
		Owner->SyncFromPawn(Fresh, 60.0);
		TestEqual(TEXT("a pawn that never braced: none"), Owner->GetCooldownEndServerTime(), -1.0);

		AStealthPawn* Recovered = World.SpawnPawn(1000.0);
		UDefenceOwnerComponent::AdoptForRecovery(Controller, Recovered, Braced, 140.0, 60.0);
		TestEqual(TEXT("PawnRecovery with the lost stamp known copies its end 160.0"), Owner->GetCooldownEndServerTime(), 160.0);

		AStealthPawn* Unknown = World.SpawnPawn(1500.0);
		UDefenceOwnerComponent::AdoptForRecovery(Controller, Unknown, nullptr, 130.0, 60.0);
		TestEqual(TEXT("PawnRecovery with the stamp unknown at t = 130 stamps now: 190.0"), Owner->GetCooldownEndServerTime(), 190.0);
		const int32 Writes = Owner->GetWriteCount();
		Owner->SyncFromPawn(Unknown, 60.0);
		TestEqual(TEXT("an unchanged value writes nothing"), Owner->GetWriteCount(), Writes);
	}

	// Reflection (AC1's static half): the defence state never replicates; the owner datum is one owner-only double.
	{
		TestTrue(TEXT("UDefenceStateComponent declares no replicated property"), OwnReplicatedProperties(UDefenceStateComponent::StaticClass()).IsEmpty());
		TestFalse(TEXT("UDefenceStateComponent is not replicated"), GetDefault<UDefenceStateComponent>()->GetIsReplicated());
		TestEqual(TEXT("UDefenceStateComponent registers no lifetime property beyond its base"),
			LifetimeCount(GetDefault<UDefenceStateComponent>(), COND_OwnerOnly) + LifetimeCount(GetDefault<UDefenceStateComponent>(), COND_None),
			LifetimeCount(GetDefault<UActorComponent>(), COND_OwnerOnly) + LifetimeCount(GetDefault<UActorComponent>(), COND_None));
		const TArray<FString> OwnerProps = OwnReplicatedProperties(UDefenceOwnerComponent::StaticClass());
		TestTrue(FString::Printf(TEXT("UDefenceOwnerComponent replicates only CooldownEndServerTime (got %s)"), *FString::Join(OwnerProps, TEXT(", "))),
			OwnerProps.Num() == 1 && OwnerProps[0] == TEXT("CooldownEndServerTime"));
		TestEqual(TEXT("registered COND_OwnerOnly (and nothing else of its own is owner-only)"),
			LifetimeCount(GetDefault<UDefenceOwnerComponent>(), COND_OwnerOnly) - LifetimeCount(GetDefault<UActorComponent>(), COND_OwnerOnly), 1);
		TestTrue(TEXT("UDefenceOwnerComponent is replicated by default"), GetDefault<UDefenceOwnerComponent>()->GetIsReplicated());
	}
	return true;
}

#endif
