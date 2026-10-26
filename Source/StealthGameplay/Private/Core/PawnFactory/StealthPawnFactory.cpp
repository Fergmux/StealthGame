#include "Server/Core/StealthPawnFactory.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Server/Roster/StealthRosterStore.h"
#include "View/Core/StealthPawn.h"

namespace StealthPawnFactoryLocal
{
	const FName FaultSource(TEXT("PawnFactory"));

	/** Fault details of factory misuse. */
	constexpr int64 MisuseNoAuthority = 1;
	constexpr int64 MisuseWrongThread = 2;
	constexpr int64 MisuseNoClass = 3;
	constexpr int64 MisuseSpawnFailed = 4;
	constexpr int64 MisuseNoController = 5;
	/** Above this many tokens, tokens of destroyed pawns are pruned at the next spawn. */
	constexpr int32 TokenPruneThreshold = 256;
}

bool UStealthPawnFactory::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld();
}

UStealthPawnFactory* UStealthPawnFactory::Get(const UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<UStealthPawnFactory>() : nullptr;
}

void UStealthPawnFactory::SetHooks(IStealthPawnFactoryHooks* InHooks, UObject* Owner)
{
	Hooks = InHooks;
	HooksOwner = Owner;
}

IStealthPawnFactoryHooks* UStealthPawnFactory::GetHooks() const
{
	return HooksOwner.IsValid() ? Hooks : nullptr;
}

FStealthSpawnResult UStealthPawnFactory::SpawnSharedPawn(const FStealthSpawnRequest& Request)
{
	using namespace StealthPawnFactoryLocal;
	FStealthSpawnResult Result;
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		FaultSink.Report(EStealthFault::InvariantViolation, FaultSource, MisuseNoAuthority);
		return Result;
	}
	if (!IsInGameThread())
	{
		FaultSink.Report(EStealthFault::InvariantViolation, FaultSource, MisuseWrongThread);
		return Result;
	}
	UClass* PawnClass = Request.PawnClass != nullptr ? Request.PawnClass.Get() : DefaultPawnClass.Get();
	if (PawnClass == nullptr)
	{
		FaultSink.Report(EStealthFault::InvariantViolation, FaultSource, MisuseNoClass);
		return Result;
	}

	// 1. Deferred spawn: nothing runs BeginPlay or replicates before the pawn is complete.
	const ESpawnActorCollisionHandlingMethod Collision = Request.ControlKind == EStealthPawnControlKind::Player
		? ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
		: ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	AStealthPawn* Pawn = World->SpawnActorDeferred<AStealthPawn>(PawnClass, Request.Transform, nullptr, nullptr, Collision);
	if (Pawn == nullptr)
	{
		FaultSink.Report(EStealthFault::InvariantViolation, FaultSource, MisuseSpawnFailed);
		return Result;
	}
	if (CreationTokens.Num() > TokenPruneThreshold)
	{
		for (auto It = CreationTokens.CreateIterator(); It; ++It)
		{
			if (It->ResolveObjectPtr() == nullptr)
			{
				It.RemoveCurrent();
			}
		}
	}
	Pawn->SetInstigator(Pawn);
	CreationTokens.Add(FObjectKey(Pawn));
	++SpawnCount;
	if (URosterStore* Roster = URosterStore::Get(World))
	{
		// The per-pawn spawn generation of the Roster's pawn handle (R9 tombstones, I5).
		Roster->NotifyPawnSpawned(Pawn);
	}

	IStealthPawnFactoryHooks* ActiveHooks = GetHooks();
	// 2. Appearance before anything can see the pawn.
	if (ActiveHooks != nullptr)
	{
		ActiveHooks->ApplyAppearance(Pawn, Request.AppearanceId);
	}
	// 3. Bind before visible (a client's first bunch carries the appearance and no binding).
	if (Request.Participant.IsValid() && ActiveHooks != nullptr)
	{
		Result.Bind = ActiveHooks->BindPawn(Request.Participant, Pawn, Request.ExpectedEpoch);
	}
	// 4. Finish spawning.
	Pawn->FinishSpawning(Request.Transform);
	if (!IsValid(Pawn))
	{
		// A crowd spawn that collides is a normal miss (the caller retries elsewhere); a player spawn always spawns.
		return Result;
	}
	Result.Pawn = Pawn;

	// 5. Possess (the factory is the only caller of Possess on a Shared Pawn).
	AController* Controller = Request.ControlKind == EStealthPawnControlKind::Player
		? Request.PlayerController
		: (ActiveHooks != nullptr ? ActiveHooks->CreateAiController(Pawn) : nullptr);
	if (Controller != nullptr)
	{
		Controller->Possess(Pawn);
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			PlayerController->ClientSetRotation(Request.Transform.Rotator(), true);
		}
	}
	else if (Request.ControlKind == EStealthPawnControlKind::Player)
	{
		FaultSink.Report(EStealthFault::InvariantViolation, FaultSource, MisuseNoController);
	}
	return Result;
}

bool UStealthPawnFactory::HasCreationToken(const AActor* Pawn) const
{
	return Pawn != nullptr && CreationTokens.Contains(FObjectKey(Pawn));
}

int32 UStealthPawnFactory::CountUntokenedPawns() const
{
	int32 Untokened = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AStealthPawn> It(World); It; ++It)
		{
			if (!HasCreationToken(*It))
			{
				++Untokened;
			}
		}
	}
	return Untokened;
}
