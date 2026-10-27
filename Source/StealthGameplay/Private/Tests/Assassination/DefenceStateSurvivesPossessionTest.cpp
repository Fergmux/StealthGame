// Story asn-003 (production/epics/assassination-defence-reversal/story-003-defence-state-component-and-cooldown.md),
// AC2 / AC-ASN-18: the defence stamp survives every possession change (GDD ASN-R18).
// GIVEN a stamp at t = 100 on a pawn WHEN (separately) the pawn is rejoined (its controller leaves at 105 and a new player
// controller possesses the same pawn at 110), released to the crowd (an AI controller takes it at 120), and re-pawned by
// PawnRecovery at 130 (a new pawn adopts the lost pawn's state) THEN a Prepare at t = 140 replies Cooldown in all three
// cases; AND a PawnRecovery with an unknown stamp stamps t = 130 (Prepare at 189.9 replies Cooldown, at 190.0 executes):
// a recovery is never a free brace. Reset clears the stamp (the hook story asn-019 asserts end to end).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "AssassinationTestWorld.h"
#include "GameFramework/PlayerController.h"
#include "Server/AssassinationPreparePredicate.h"
#include "Server/DefenceStateComponent.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace AssassinationPossessionTestData
{
	const FAssassinationTuning_Public Tuning;

	bool IsCooldownAt(const AStealthPawn& Pawn, double T)
	{
		const UDefenceStateComponent* Component = UDefenceStateComponent::Find(Pawn);
		return UAssassinationPreparePredicate::Evaluate(Component != nullptr ? &Component->GetCore() : nullptr, T, Tuning.DefenceCooldownSec)
			== FPredicateResult::Reject(ECommandRejectCode::Cooldown);
	}

	AStealthPawn* StampedPawn(const StealthAssassinationTest::FScopedWorld& TestWorld, double X)
	{
		AStealthPawn* Pawn = TestWorld.SpawnPawn(X);
		if (Pawn != nullptr)
		{
			UDefenceStateComponent::FindOrAdd(*Pawn)->GetCore().BeginPrepare(100.0, Tuning.DefenceCooldownSec, FQueueKey(6000, 1));
		}
		return Pawn;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationDefenceStateSurvivesPossessionTest, "StealthGame.Assassination.DefenceStateSurvivesPossession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationDefenceStateSurvivesPossessionTest::RunTest(const FString& Parameters)
{
	using namespace AssassinationPossessionTestData;
	StealthAssassinationTest::FScopedWorld TestWorld(TEXT("AssassinationPossessionTestWorld"));
	UWorld* World = TestWorld.World;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// ---- Rejoin into the same pawn ----
	{
		AStealthPawn* Pawn = StampedPawn(TestWorld, 0.0);
		APlayerController* First = World->SpawnActor<APlayerController>(Params);
		APlayerController* Rejoined = World->SpawnActor<APlayerController>(Params);
		if (!TestTrue(TEXT("rejoin fixture spawned"), Pawn != nullptr && First != nullptr && Rejoined != nullptr))
		{
			return false;
		}
		const UDefenceStateComponent* Before = UDefenceStateComponent::Find(*Pawn);
		First->Possess(Pawn);
		First->UnPossess();     // disconnect at 105
		Rejoined->Possess(Pawn); // rejoin at 110
		TestTrue(TEXT("rejoin: the pawn keeps the same component"), UDefenceStateComponent::Find(*Pawn) == Before);
		TestTrue(TEXT("rejoin: Prepare at 140 replies Cooldown"), IsCooldownAt(*Pawn, 140.0));
		Rejoined->UnPossess();
	}

	// ---- ReleaseToCrowd: the crowd's AI controller takes the pawn ----
	{
		AStealthPawn* Pawn = StampedPawn(TestWorld, 300.0);
		APlayerController* Player = World->SpawnActor<APlayerController>(Params);
		AAIController* Crowd = World->SpawnActor<AAIController>(Params);
		if (!TestTrue(TEXT("release fixture spawned"), Pawn != nullptr && Player != nullptr && Crowd != nullptr))
		{
			return false;
		}
		Player->Possess(Pawn);
		Player->UnPossess();
		Crowd->Possess(Pawn); // released to the crowd at 120
		TestTrue(TEXT("release: the crowd now drives the pawn"), Pawn->GetController() == Crowd);
		TestTrue(TEXT("release: Prepare at 140 replies Cooldown (NPC braces honour the same cooldown)"), IsCooldownAt(*Pawn, 140.0));
		Crowd->UnPossess();
	}

	// ---- PawnRecovery: a new pawn adopts the lost pawn's stamp ----
	{
		AStealthPawn* Lost = StampedPawn(TestWorld, 600.0);
		AStealthPawn* Recovered = TestWorld.SpawnPawn(900.0);
		if (!TestTrue(TEXT("recovery fixture spawned"), Lost != nullptr && Recovered != nullptr))
		{
			return false;
		}
		UDefenceStateComponent::FindOrAdd(*Recovered)->GetCore().AdoptForRecovery(&UDefenceStateComponent::Find(*Lost)->GetCore(), 130.0);
		TestTrue(TEXT("recovery: the stamp is copied (100)"), UDefenceStateComponent::Find(*Recovered)->GetCore().GetLastPrepareActivation() == TOptional<double>(100.0));
		TestTrue(TEXT("recovery: Prepare at 140 replies Cooldown"), IsCooldownAt(*Recovered, 140.0));
	}

	// ---- PawnRecovery with an unknown stamp: stamps now (130) ----
	{
		AStealthPawn* Recovered = TestWorld.SpawnPawn(1200.0);
		if (!TestNotNull(TEXT("unknown-stamp fixture spawned"), Recovered))
		{
			return false;
		}
		FDefenceStateCore& Core = UDefenceStateComponent::FindOrAdd(*Recovered)->GetCore();
		Core.AdoptForRecovery(nullptr, 130.0);
		TestTrue(TEXT("unknown stamp: recovery stamps 130"), Core.GetLastPrepareActivation() == TOptional<double>(130.0));
		TestTrue(TEXT("unknown stamp: Prepare at 189.9 replies Cooldown"), IsCooldownAt(*Recovered, 189.9));
		TestFalse(TEXT("unknown stamp: Prepare at 190.0 executes"), IsCooldownAt(*Recovered, 190.0));

		const FDefenceStateCore NeverBraced;
		FDefenceStateCore FromNeverBraced;
		FromNeverBraced.AdoptForRecovery(&NeverBraced, 130.0);
		TestTrue(TEXT("a lost pawn that never braced counts as unknown: stamps 130"), FromNeverBraced.GetLastPrepareActivation() == TOptional<double>(130.0));

		Core.Reset();
		TestFalse(TEXT("Reset clears the stamp"), Core.GetLastPrepareActivation().IsSet());
		TestFalse(TEXT("after Reset Prepare is allowed"), IsCooldownAt(*Recovered, 131.0));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
