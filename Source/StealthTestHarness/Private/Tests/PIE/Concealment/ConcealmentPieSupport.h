#pragma once

// Shared helper of the PIE tests that need a pawn hidden in a bush (Concealment epic): the production path, never a
// teleport into the leaves (the hide volume blocks an Outside pawn). The pawn is stood at the bush's +X entry, 40 cm out of
// the planter edge and facing the anchor, then submits the one Hide(Enter) with its own origin.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Server/ConcealmentService.h"
#include "View/ConcealmentComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "View/BushRuntimeSubsystem.h"

namespace StealthConcealmentPie
{
	/** The bush whose footprint holds Point on the server (the runtime of the stand-in Manifest), or null. */
	inline const FBushRuntime* BushAt(const FVector& Point)
	{
		const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(StealthAssassinationPie::ServerWorld());
		return Bushes != nullptr ? Bushes->FindBushAt(Point) : nullptr;
	}

	/** The copy of ServerActor on World through the shared NetGUID (legacy replication): ServerActor on the server; null when World holds none (culled, not yet opened, destroyed). */
	inline AActor* CopyByGuid(UWorld* World, const AActor* ServerActor)
	{
		if (World == nullptr || ServerActor == nullptr)
		{
			return nullptr;
		}
		if (ServerActor->GetWorld() == World)
		{
			return const_cast<AActor*>(ServerActor);
		}
		const UNetDriver* ServerDriver = ServerActor->GetWorld() != nullptr ? ServerActor->GetWorld()->GetNetDriver() : nullptr;
		const UNetDriver* ClientDriver = World->GetNetDriver();
		if (ServerDriver == nullptr || ClientDriver == nullptr || !ServerDriver->GetNetGuidCache().IsValid() || !ClientDriver->GetNetGuidCache().IsValid())
		{
			return nullptr;
		}
		const FNetworkGUID Guid = ServerDriver->GetNetGuidCache()->GetNetGUID(ServerActor);
		if (!Guid.IsValid())
		{
			return nullptr;
		}
		AActor* Copy = Cast<AActor>(ClientDriver->GetNetGuidCache()->GetObjectFromNetGUID(Guid, false));
		return IsValid(Copy) && !Copy->IsActorBeingDestroyed() ? Copy : nullptr;
	}

	/** True when this machine draws Pawn's body: the mesh is visible (the latch, the S10 warm-up and the death removal all act on it). */
	inline bool IsBodyDrawn(const AStealthPawn* Copy)
	{
		const USkeletalMeshComponent* Mesh = Copy != nullptr ? Copy->GetMesh() : nullptr;
		return Mesh != nullptr && Mesh->IsVisible() && !Copy->IsHidden();
	}

	/** Lifts the re-entry cooldown on the server core for the session (back-to-back cycles); returns the previous parameters. */
	inline TOptional<FConcealmentParams> LiftReentryCooldown()
	{
		UConcealmentService* Service = UConcealmentService::Get(StealthAssassinationPie::ServerWorld());
		FConcealmentCore* Core = Service != nullptr ? Service->GetCore() : nullptr;
		if (Core == nullptr)
		{
			return {};
		}
		const FConcealmentParams Previous = Core->GetParams();
		FConcealmentParams Lifted = Previous;
		Lifted.Server.BushReenterCooldownSec = 0.0f;
		Core->SetParamsForTest(Lifted);
		return Previous;
	}

	/** The server state of Pawn (Outside without a service). */
	inline EConcealState StateOf(const APawn* Pawn)
	{
		const UConcealmentService* Service = UConcealmentService::Get(StealthAssassinationPie::ServerWorld());
		return Service != nullptr ? Service->GetState(Pawn) : EConcealState::Outside;
	}

		/** Teleports a player's server pawn and its owner copy to At (ground point), facing Facing (the owner owns its rotation). */
	inline void PlacePlayerAt(AStealthPlayerController* ServerController, const FVector& At, const FRotator& Facing)
	{
		APawn* Server = ServerController != nullptr ? ServerController->GetPawn() : nullptr;
		const ACharacter* Character = Cast<ACharacter>(Server);
		if (Character == nullptr)
		{
			return;
		}
		const FVector Location(At.X, At.Y, At.Z + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0);
		Character->GetCharacterMovement()->StopMovementImmediately();
		Server->TeleportTo(Location, Facing, false, true);
		ServerController->SetControlRotation(Facing);
		if (AStealthPlayerController* Local = StealthAssassinationPie::OwnerMachineController(ServerController); Local != nullptr && Local != ServerController)
		{
			if (APawn* Copy = Local->GetPawn())
			{
				Copy->TeleportTo(Location, Facing, false, true);
			}
			Local->SetControlRotation(Facing);
		}
	}

	/** The ground centroid of the server's bushes (zero without any). */
	inline FVector BushCentroid()
	{
		const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(StealthAssassinationPie::ServerWorld());
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FBushRuntime& Bush : Bushes != nullptr ? Bushes->GetBushes() : TArray<FBushRuntime>())
		{
			Sum += Bush.Anchor;
			++Count;
		}
		return Count > 0 ? Sum / Count : FVector::ZeroVector;
	}

	/**
	 * Spawns an unbound Shared Pawn (an NPC identity) on the server at Location. With no controller the movement component
	 * would not simulate (the concealment root-motion move would never run); the crowd's NPCs are possessed, so the test
	 * pawn simulates without one (bRunPhysicsWithNoController) instead.
	 */
	inline AStealthPawn* SpawnHider(const FVector& Location, const FRotator& Rotation)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStealthPawn* Pawn = StealthAssassinationPie::ServerWorld()->SpawnActor<AStealthPawn>(AStealthPawn::StaticClass(), Location, Rotation, Params);
		if (Pawn != nullptr)
		{
			Pawn->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		}
		return Pawn;
	}

		/** Stands an unbound pawn at the +X entry of Bush (Y offset), facing the anchor. */
	inline void StandAtEntry(AStealthPawn* Pawn, const FBushRuntime& Bush, double YOffsetCm)
	{
		const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Pawn->GetCharacterMovement()->StopMovementImmediately();
		Pawn->TeleportTo(Bush.Anchor + FVector(140.0, YOffsetCm, HalfHeight + 2.0), FRotator(0.0, 180.0, 0.0), false, true);
	}

		/** Stands Pawn at the +X entry of the bush holding BushPoint and submits Hide(Enter). False without such a bush. */
	inline bool HideIn(AStealthPawn* Pawn, const FVector& BushPoint, uint32& Seq)
	{
		const FBushRuntime* Bush = BushAt(BushPoint);
		if (Pawn == nullptr || Bush == nullptr)
		{
			return false;
		}
		const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Entry = Bush->Anchor + FVector(140.0, 0.0, HalfHeight + 2.0);
		Pawn->GetCharacterMovement()->StopMovementImmediately();
		Pawn->TeleportTo(Entry, FRotator(0.0, 180.0, 0.0), false, true);
		const uint16 BushId = Bush->Id;
		StealthAssassinationPie::AsOwner(Pawn, Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
		return true;
	}
}

#endif
