#pragma once

// Shared fixture of the StealthGame.Assassination.* tests (story asn-003): a bare game world with BeginPlay run, torn
// down by the destructor, and a pawn spawner. Test-only; included by tests under Private/Tests/Assassination.

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "View/Core/StealthPawn.h"

namespace StealthAssassinationTest
{
	/** One server-authoritative game world for a test. */
	struct FScopedWorld
	{
		UWorld* World = nullptr;

		explicit FScopedWorld(const TCHAR* Name)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, Name);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}

		~FScopedWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		/** A pawn at X (always spawned). */
		AStealthPawn* SpawnPawn(double X) const
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<AStealthPawn>(FVector(X, 0.0, 200.0), FRotator::ZeroRotator, Params);
		}
	};

	/** The server tick length the boundary tests allow (ADR-0002: one server frame, 60 Hz here). */
	inline constexpr double ServerTickSec = 1.0 / 60.0;
}
