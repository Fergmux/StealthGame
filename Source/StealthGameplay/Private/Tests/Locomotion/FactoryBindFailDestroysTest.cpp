// Story SCL-007 (production/epics/shared-character-locomotion/story-007-factory-bind-fail-destroys.md), AC-SCL-03
// FactoryBindFailDestroys: GIVEN BindPawn stubbed to return Rejected_AlreadyBound, WHEN the factory runs, THEN the pawn
// is destroyed before FinishSpawning (an injected binder records the call order; FinishSpawning is observed through
// BeginPlay / IsActorInitialized, which it runs), the code is returned in FStealthSpawnResult.Bind, and no pawn is left
// in the world. The "a connected client never receives the actor" clause runs in the PIE test
// StealthGame.Locomotion.PIE.FactoryBindFailNeverReplicates (StealthTestHarness). Control: an accepted bind finishes.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Server/Core/StealthPawnFactory.h"
#include "View/Core/StealthPawn.h"

namespace FactoryBindFailDestroysTestData
{
	/** Binder stub: returns a fixed code and records whether the pawn had already been finished when asked. */
	class FRecordingHooks final : public IStealthPawnFactoryHooks
	{
	public:
		explicit FRecordingHooks(ERosterResult InResult) : Result(InResult) {}
		TArray<FString> Calls;
		bool bPawnInitialisedAtBind = true;
		virtual void ApplyAppearance(AStealthPawn* Pawn, int32 AppearanceId) override { Calls.Add(TEXT("ApplyAppearance")); }
		virtual ERosterResult BindPawn(FParticipantId Participant, AStealthPawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch) override
		{
			Calls.Add(TEXT("BindPawn"));
			bPawnInitialisedAtBind = Pawn->IsActorInitialized();
			BoundPawn = Pawn;
			BoundRaw = Pawn;
			return Result;
		}
		virtual AController* CreateAiController(AStealthPawn* Pawn) override { Calls.Add(TEXT("CreateAiController")); return nullptr; }
		ERosterResult Result;
		TWeakObjectPtr<AStealthPawn> BoundPawn;
		/** Raw pointer, read only before garbage collection (the test never collects). */
		AStealthPawn* BoundRaw = nullptr;
	};

	struct FScopedTestWorld
	{
		UWorld* World = nullptr;
		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("FactoryBindFailTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
		~FScopedTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	int32 CountPawns(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AStealthPawn> It(World); It; ++It)
		{
			Count += IsValid(*It) ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionFactoryBindFailDestroysTest, "StealthGame.Locomotion.FactoryBindFailDestroys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionFactoryBindFailDestroysTest::RunTest(const FString& Parameters)
{
	using namespace FactoryBindFailDestroysTestData;
	FScopedTestWorld TestWorld;
	UStealthPawnFactory* Factory = UStealthPawnFactory::Get(TestWorld.World);
	if (!TestNotNull(TEXT("the factory subsystem exists in a game world"), Factory))
	{
		return false;
	}
	Factory->SetDefaultPawnClass(AStealthPawn::StaticClass());
	UObject* HooksOwner = TestWorld.World; // the hooks are held weakly through an owner that outlives the calls

	FStealthSpawnRequest Request;
	Request.Transform = FTransform(FVector(0.0, 0.0, 200.0));
	Request.ControlKind = EStealthPawnControlKind::Ai;
	Request.Participant = FParticipantId(7);

	// Rejected bind.
	FRecordingHooks Rejecting(ERosterResult::Rejected_AlreadyBound);
	Factory->SetHooks(&Rejecting, HooksOwner);
	const int32 PawnsBefore = CountPawns(TestWorld.World);
	const FStealthSpawnResult Rejected = Factory->SpawnSharedPawn(Request);
	TestTrue(TEXT("the rejection code is returned"), Rejected.Bind == ERosterResult::Rejected_AlreadyBound);
	TestNull(TEXT("no pawn is returned"), Rejected.Pawn);
	TestFalse(TEXT("BindPawn ran before FinishSpawning (the pawn was not yet initialised)"), Rejecting.bPawnInitialisedAtBind);
	TestTrue(TEXT("the pawn bound is destroyed"), !Rejecting.BoundPawn.IsValid() || !IsValid(Rejecting.BoundPawn.Get()));
	// FinishSpawning never ran on it: it was never initialised and never began play (not merely destroyed afterwards).
	TestTrue(TEXT("the destroyed pawn never finished spawning (not initialised, never began play)"),
		Rejecting.BoundRaw != nullptr && !Rejecting.BoundRaw->IsActorInitialized() && !Rejecting.BoundRaw->HasActorBegunPlay());
	TestEqual(TEXT("call order: appearance, bind, nothing after (no FinishSpawning, no possession)"), FString::Join(Rejecting.Calls, TEXT(",")), FString(TEXT("ApplyAppearance,BindPawn")));
	TestEqual(TEXT("no pawn is left in the world"), CountPawns(TestWorld.World), PawnsBefore);

	// Control: an accepted bind finishes and possesses.
	FRecordingHooks Accepting(ERosterResult::Applied);
	Factory->SetHooks(&Accepting, HooksOwner);
	const FStealthSpawnResult Accepted = Factory->SpawnSharedPawn(Request);
	TestNotNull(TEXT("control: an accepted bind returns the pawn"), Accepted.Pawn);
	TestTrue(TEXT("control: the accepted pawn finished spawning"), Accepted.Pawn != nullptr && Accepted.Pawn->IsActorInitialized());
	TestEqual(TEXT("control: call order appearance, bind, AI controller"), FString::Join(Accepting.Calls, TEXT(",")), FString(TEXT("ApplyAppearance,BindPawn,CreateAiController")));
	TestEqual(TEXT("control: one pawn in the world"), CountPawns(TestWorld.World), PawnsBefore + 1);

	Factory->SetHooks(nullptr, nullptr);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
