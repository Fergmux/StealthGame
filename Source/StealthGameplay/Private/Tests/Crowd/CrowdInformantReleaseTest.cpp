// Story cnb-030 (production/epics/crowd-npc-behaviour/story-030-takeover-eligibility-and-atomic-release.md): takeover
// eligibility supply and the atomic NPC release for informants (GDD CNB-R11, AC-CNB-23; ADR-0016 section 8). A game
// world with the real Director, pawn factory and crowd controllers.
//   AtomicRelease   100 TryReleaseNpc calls on pawns in mixed activities: every released pawn was Eligible at the commit
//                   and none was tending, bracing, hiding, emerging, exiting, reacting or conversing; in the same call
//                   the tree is stopped, the control epoch bumped and the pawn taken out of the ledger (B + 1); no
//                   Move(0) is issued (no pose change); afterwards 0 old-epoch commands reach the pawn (late commands are
//                   injected after 20 of the commits, an async path callback among them).
//   Eligibility     Bound for a restricted driver and for a released pawn, AlleyExit for a body waiting for its exit slot.
//   Reversal        AdoptReleasedPawn(Pawn, {}) after TryReleaseNpc reverses it: B - 1, the sink accepts commands again.
// AC-CNB-23c (the speed on a remote client under 100 ms RTT after possession) needs the multi-process harness: the
// nightly job of story cnb-034.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Server/Core/StealthPawnControlQuery.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/CrowdDirector.h"
#include "Server/CrowdStateTreeComponent.h"
#include "Server/StealthCrowdController.h"
#include "View/Core/StealthPawn.h"

namespace CrowdInformantTestData
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
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CrowdInformantTestWorld"));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdInformantReleaseTest, "StealthGame.Crowd.Informant.AtomicRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdInformantReleaseTest::RunTest(const FString& Parameters)
{
	using namespace CrowdInformantTestData;
	FTestWorld TestWorld;
	UCrowdDirector* Director = UCrowdDirector::Get(TestWorld.World);
	UStealthPawnFactory* Factory = UStealthPawnFactory::Get(TestWorld.World);
	if (!TestNotNull(TEXT("a Director"), Director))
	{
		return false;
	}
	const ECrowdActivity Activities[] = { ECrowdActivity::Routing, ECrowdActivity::Tending, ECrowdActivity::Pausing, ECrowdActivity::Bracing, ECrowdActivity::Hiding,
		ECrowdActivity::Emerging, ECrowdActivity::Exiting, ECrowdActivity::Reacting, ECrowdActivity::Conversing };
	int32 Released = 0, ReleasedIneligible = 0, RefusedEligible = 0, NoEpochBump = 0, NoLedger = 0, TreeRunning = 0, MoveAtRelease = 0, LateReached = 0, Injected = 0;
	for (int32 Call = 0; Call < 100; ++Call)
	{
		AStealthPawn* Pawn = TestWorld.Spawn(FVector(150.0 * (Call % 20), 150.0 * (Call / 20), 0.0));
		AStealthCrowdController* Controller = Pawn != nullptr ? Cast<AStealthCrowdController>(Pawn->GetController()) : nullptr;
		if (!TestNotNull(TEXT("a crowd-driven NPC"), Controller))
		{
			return false;
		}
		const uint32 Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
		const ECrowdActivity Activity = Activities[Call % UE_ARRAY_COUNT(Activities)];
		Director->GetCore()->SetActivity(Ordinal, Activity);
		const bool bEligibleBefore = Director->GetTakeoverEligibility(Pawn) == ETakeoverEligibility::Eligible;
		const uint32 EpochBefore = Factory->GetControlEpoch(Pawn);
		const int32 TakenBefore = Director->GetCore()->GetTakenOver();
		const int64 PassedBefore = Controller->GetSinkCounters().Passed;
		const bool bReleased = Director->TryReleaseNpc(Pawn, FEpochToken(1, 1));
		if (bReleased)
		{
			++Released;
			ReleasedIneligible += bEligibleBefore ? 0 : 1;
			NoEpochBump += Factory->GetControlEpoch(Pawn) == EpochBefore + 1 ? 0 : 1;
			NoLedger += Director->GetCore()->GetTakenOver() == TakenBefore + 1 ? 0 : 1;
			TreeRunning += Controller->GetCrowdStateTree() != nullptr && Controller->GetCrowdStateTree()->IsRunning() ? 1 : 0;
			MoveAtRelease += Controller->GetSinkCounters().Passed != PassedBefore ? 1 : 0;
			if (Call % 5 == 0)
			{
				// Late commands of the previous driver after the commit: a held Move, a Look, a debounced End, a path result.
				++Injected;
				const int64 Passed = Controller->GetSinkCounters().Passed + Controller->GetSinkCounters().DiscretePassed;
				Controller->Move(FVector2D(0.0, 1.0), EGait::Walk);
				Controller->Look(FRotator(0.0, 30.0, 0.0));
				Controller->PrepareEnd();
				FCrowdPathResult Late;
				Late.Ordinal = Ordinal;
				Late.LegId = 1;
				Late.ControlEpoch = EpochBefore;
				Late.bValid = true;
				Late.Corners = { FVector(500.0, 0.0, 0.0) };
				Controller->GetAgent().OnPathResult(Late, *Director);
				LateReached += static_cast<int32>(Controller->GetSinkCounters().Passed + Controller->GetSinkCounters().DiscretePassed - Passed);
			}
		}
		else
		{
			RefusedEligible += bEligibleBefore ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("INFORMANT 100 TryReleaseNpc calls: released %d (ineligible at commit %d), eligible refused %d; per release: no epoch bump %d, no ledger change %d, tree still running %d, a command issued at the release %d; late commands injected after %d commits, reached the pawn %d"),
		Released, ReleasedIneligible, RefusedEligible, NoEpochBump, NoLedger, TreeRunning, MoveAtRelease, Injected, LateReached));
	TestTrue(TEXT("eligible pawns were released"), Released >= 20);
	TestEqual(TEXT("every released pawn was Eligible at the commit"), ReleasedIneligible, 0);
	TestEqual(TEXT("no eligible pawn was refused"), RefusedEligible, 0);
	TestEqual(TEXT("the control epoch is bumped in the same call"), NoEpochBump, 0);
	TestEqual(TEXT("the pawn leaves the ledger (B + 1) in the same call"), NoLedger, 0);
	TestEqual(TEXT("the StateTree is stopped"), TreeRunning, 0);
	TestEqual(TEXT("no command at the release (no pose change: the pawn keeps moving as it was)"), MoveAtRelease, 0);
	TestEqual(TEXT("0 old-epoch commands reach the pawn after the commit"), LateReached, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdInformantEligibilityTest, "StealthGame.Crowd.Informant.EligibilityAndReversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdInformantEligibilityTest::RunTest(const FString& Parameters)
{
	using namespace CrowdInformantTestData;
	FTestWorld TestWorld;
	UCrowdDirector* Director = UCrowdDirector::Get(TestWorld.World);
	AStealthPawn* Npc = TestWorld.Spawn(FVector::ZeroVector);
	AStealthPawn* Exile = TestWorld.Spawn(FVector(300.0, 0.0, 0.0));
	AStealthCrowdController* NpcController = Cast<AStealthCrowdController>(Npc->GetController());
	TestEqual(TEXT("an idle NPC is Eligible"), static_cast<int32>(Director->GetTakeoverEligibility(Npc)), static_cast<int32>(ETakeoverEligibility::Eligible));
	// A released exile body waiting for its exit slot is AlleyExit (its own participant can never re-adopt it).
	Director->ClaimExitSlot(StealthPawnControl::GetSpawnOrdinal(Exile));
	TestEqual(TEXT("an exile body waiting for its slot is AlleyExit"), static_cast<int32>(Director->GetTakeoverEligibility(Exile)), static_cast<int32>(ETakeoverEligibility::AlleyExit));
	// Released: Bound until the reversal.
	TestTrue(TEXT("the NPC is released"), Director->TryReleaseNpc(Npc, FEpochToken(1, 1)));
	TestEqual(TEXT("a released pawn reports Bound"), static_cast<int32>(Director->GetTakeoverEligibility(Npc)), static_cast<int32>(ETakeoverEligibility::Bound));
	TestFalse(TEXT("a released pawn cannot be released twice"), Director->TryReleaseNpc(Npc, FEpochToken(1, 1)));
	const int32 Taken = Director->GetCore()->GetTakenOver();
	const int32 Passed = NpcController->GetSinkCounters().Passed;
	NpcController->Move(FVector2D(0.0, 1.0), EGait::Walk);
	TestEqual(TEXT("the released pawn's sink drops commands"), NpcController->GetSinkCounters().Passed, Passed);
	// The reversal: AdoptReleasedPawn(Pawn, {}).
	Director->AdoptReleasedPawn(Npc, FCrowdAdoptParams());
	TestEqual(TEXT("the reversal decrements B"), Director->GetCore()->GetTakenOver(), Taken - 1);
	TestEqual(TEXT("the reversed pawn is Eligible again"), static_cast<int32>(Director->GetTakeoverEligibility(Npc)), static_cast<int32>(ETakeoverEligibility::Eligible));
	const int32 PassedAfterReversal = NpcController->GetSinkCounters().Passed;
	NpcController->Move(FVector2D(0.0, 1.0), EGait::Walk);
	TestEqual(TEXT("the sink is unlatched at the pawn's current epoch: commands reach the pawn again"), NpcController->GetSinkCounters().Passed, PassedAfterReversal + 1);
	TestFalse(TEXT("the controller is no longer released"), NpcController->IsReleasedForTakeover());
	// A restricted driver is Bound.
	AStealthPawn* Bound = TestWorld.Spawn(FVector(600.0, 0.0, 0.0));
	Bound->GetController()->UnPossess();
	Director->DriveBoundPawn(Bound);
	AStealthCrowdController* Restricted = TestWorld.World->SpawnActor<AStealthCrowdController>();
	Restricted->Possess(Bound);
	TestEqual(TEXT("a participant-bound pawn is Bound"), static_cast<int32>(Director->GetTakeoverEligibility(Bound)), static_cast<int32>(ETakeoverEligibility::Bound));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
