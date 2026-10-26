// Story SCL-006 (production/epics/shared-character-locomotion/story-006-single-spawn-site.md), AC-SCL-02 / R2:
// with pawn creation traced (UWorld actor-spawned handler), a round start creates 2 x TeamSize + NpcCount Shared Pawns
// all through UStealthPawnFactory::SpawnSharedPawn, an NPC kill's replacement creates exactly 1, an informant kill's
// respawn creates 0 (a takeover, stubbed here; the real path is story SCL-028), every spawned pawn has Instigator ==
// itself and the factory creation token, and the composition root never calls RestartPlayer / SpawnDefaultPawnFor /
// SpawnDefaultPawnAtTransform ([STATIC] scan of Source/StealthGame). Negative control: a pawn spawned outside the
// factory is detected as untokened. TeamSize and NpcCount are the tuning defaults (FRoundTuning_Public,
// FCrowdTuning_Server). The live round-start evidence is the D53 integration screenshot.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Core/Round/StealthRoundRosterGateway.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/StealthTuningSubstructs.h"
#include "View/Core/StealthPawn.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace SingleSpawnSiteTestData
{
	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SingleSpawnSiteTestWorld"));
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

	/** Stub facades: appearance and bind are recorded; AI pawns get an AAIController (the crowd controller stand-in). */
	class FStubHooks : public IStealthPawnFactoryHooks
	{
	public:
		explicit FStubHooks(UWorld* InWorld) : World(InWorld) {}
		int32 Appearances = 0;
		int32 Binds = 0;
		virtual void ApplyAppearance(AStealthPawn* Pawn, int32 AppearanceId) override { ++Appearances; }
		virtual ERosterResult BindPawn(FParticipantId Participant, AStealthPawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch) override
		{
			++Binds;
			return ERosterResult::Applied;
		}
		virtual AController* CreateAiController(AStealthPawn* Pawn) override
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<AAIController>(Params);
		}
		UWorld* World;
	};

	/** The stubbed informant respawn: takes over an existing crowd pawn (creates nothing). */
	AStealthPawn* StubTakeOverCrowdPawn(UWorld* World, APawn* Exclude)
	{
		for (TActorIterator<AStealthPawn> It(World); It; ++It)
		{
			if (*It != Exclude)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Removes // comments so a mention in a comment is not a call. */
	FString CodeOnly(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		FString Out;
		for (const FString& Line : Lines)
		{
			const int32 Comment = Line.Find(TEXT("//"));
			Out += (Comment == INDEX_NONE ? Line : Line.Left(Comment)) + TEXT("\n");
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionSingleSpawnSiteTest, "StealthGame.Locomotion.SingleSpawnSite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionSingleSpawnSiteTest::RunTest(const FString& Parameters)
{
	using namespace SingleSpawnSiteTestData;

	const int32 TeamSize = FRoundTuning_Public().TeamSize;
	const int32 NpcCount = FCrowdTuning_Server().NpcCount;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.World;
	UStealthPawnFactory* Factory = UStealthPawnFactory::Get(World);
	if (!TestNotNull(TEXT("the factory exists in a game world"), Factory))
	{
		return false;
	}
	FStubHooks Hooks(World);
	UObject* HooksOwner = NewObject<URoundRosterStoreGateway>(GetTransientPackage());
	HooksOwner->AddToRoot();
	Factory->SetHooks(&Hooks, HooksOwner);
	Factory->SetDefaultPawnClass(AStealthPawn::StaticClass());

	// Trace every actor spawned in the world.
	TArray<TWeakObjectPtr<AStealthPawn>> Traced;
	const FDelegateHandle TraceHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([&Traced](AActor* Actor)
	{
		if (AStealthPawn* Pawn = Cast<AStealthPawn>(Actor))
		{
			Traced.Add(Pawn);
		}
	}));

	// --- Round start: 2 x TeamSize players + NpcCount NPCs -------------------------------------------------------
	const int32 Players = 2 * TeamSize;
	TArray<APlayerController*> Controllers;
	for (int32 Index = 0; Index < Players; ++Index)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Controllers.Add(World->SpawnActor<APlayerController>(Params));
	}
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Players + NpcCount; ++Index)
	{
		FStealthSpawnRequest Request;
		Request.Transform = FTransform(FVector(300.0 * (Index % 10), 300.0 * (Index / 10), 100.0));
		Request.AppearanceId = Index % 4;
		const bool bPlayer = Index < Players;
		Request.ControlKind = bPlayer ? EStealthPawnControlKind::Player : EStealthPawnControlKind::Ai;
		Request.Participant = bPlayer ? FParticipantId(static_cast<uint32>(Index)) : FParticipantId();
		Request.PlayerController = bPlayer ? Controllers[Index] : nullptr;
		Spawned += Factory->SpawnSharedPawn(Request).Pawn != nullptr ? 1 : 0;
	}
	TestEqual(TEXT("round start: every requested pawn was created"), Spawned, Players + NpcCount);
	TestEqual(TEXT("round start: the trace saw 2 x TeamSize + NpcCount Shared Pawns"), Traced.Num(), Players + NpcCount);
	TestEqual(TEXT("round start: one appearance per pawn, before visibility"), Hooks.Appearances, Players + NpcCount);
	TestEqual(TEXT("round start: one bind per participant"), Hooks.Binds, Players);
	for (int32 Index = 0; Index < Players; ++Index)
	{
		TestTrue(FString::Printf(TEXT("player %d possessed by its controller"), Index), Controllers[Index]->GetPawn() != nullptr);
	}

	// --- NPC kill and its replacement: exactly 1 --------------------------------------------------------------------
	const int32 BeforeReplacement = Traced.Num();
	AStealthPawn* Victim = Traced.Last().Get();
	Victim->Destroy();
	FStealthSpawnRequest Replacement;
	Replacement.Transform = FTransform(FVector(-500.0, -500.0, 100.0));
	Replacement.ControlKind = EStealthPawnControlKind::Ai;
	TestNotNull(TEXT("the replacement was created"), Factory->SpawnSharedPawn(Replacement).Pawn);
	TestEqual(TEXT("an NPC replacement creates exactly 1 pawn"), Traced.Num() - BeforeReplacement, 1);

	// --- Informant kill and its respawn: 0 (stubbed takeover) ---------------------------------------------------------
	const int32 BeforeRespawn = Traced.Num();
	APawn* InformantBody = Controllers[0]->GetPawn();
	AStealthPawn* Taken = StubTakeOverCrowdPawn(World, InformantBody);
	TestNotNull(TEXT("the takeover found a crowd pawn"), Taken);
	TestEqual(TEXT("an informant respawn creates 0 pawns"), Traced.Num() - BeforeRespawn, 0);

	// --- Every traced pawn: factory token and Instigator == itself -------------------------------------------------------
	int32 Checked = 0;
	for (const TWeakObjectPtr<AStealthPawn>& Weak : Traced)
	{
		if (const AStealthPawn* Pawn = Weak.Get())
		{
			++Checked;
			TestTrue(FString::Printf(TEXT("%s carries the factory creation token"), *Pawn->GetName()), Factory->HasCreationToken(Pawn));
			TestTrue(FString::Printf(TEXT("%s has Instigator == itself"), *Pawn->GetName()), Pawn->GetInstigator() == Pawn);
		}
	}
	TestTrue(TEXT("pawns were checked"), Checked > 0);
	TestEqual(TEXT("debug guard: no untokened pawn in the world"), Factory->CountUntokenedPawns(), 0);

	// Negative control: a pawn spawned outside the factory is caught by the guard.
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStealthPawn* Rogue = World->SpawnActor<AStealthPawn>(FVector(9000.0, 0.0, 100.0), FRotator::ZeroRotator, Params);
		TestFalse(TEXT("control: a non-factory pawn has no token"), Factory->HasCreationToken(Rogue));
		TestEqual(TEXT("control: the guard counts it"), Factory->CountUntokenedPawns(), 1);
		Rogue->Destroy();
	}
	World->RemoveOnActorSpawnedHandler(TraceHandle);

	// --- [STATIC] the composition root never uses the engine's default pawn paths ------------------------------------------
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGame")), TEXT("*.cpp"), true, false);
	TestTrue(TEXT("composition root sources found"), Files.Num() > 0);
	const TCHAR* const ForbiddenCalls[] = { TEXT("RestartPlayer("), TEXT("SpawnDefaultPawnFor("), TEXT("SpawnDefaultPawnAtTransform("), TEXT("RestartPlayerAtPlayerStart(") };
	for (const FString& File : Files)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			continue;
		}
		const FString Code = CodeOnly(Text);
		for (const TCHAR* Call : ForbiddenCalls)
		{
			TestFalse(FString::Printf(TEXT("%s does not call %s"), *FPaths::GetCleanFilename(File), Call), Code.Contains(Call));
		}
	}

	HooksOwner->RemoveFromRoot();
	return true;
}

#endif
