#pragma once

// Shared helper of the PIE tests that need a pawn hidden in a bush (Concealment epic): the production path, never a
// teleport into the leaves (the hide volume blocks an Outside pawn). The pawn is stood at the bush's +X entry, 40 cm out of
// the planter edge and facing the anchor, then submits the one Hide(Enter) with its own origin.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "Components/CapsuleComponent.h"
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
