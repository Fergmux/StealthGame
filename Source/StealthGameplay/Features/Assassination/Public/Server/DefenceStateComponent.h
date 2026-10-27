#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Server/DefenceStateCore.h"

#include "DefenceStateComponent.generated.h"

/**
 * The server-only defence state of one pawn (ADR-0018 section 4.1; GDD ASN-R18; story asn-003): a thin facade over
 * FDefenceStateCore. Never replicated, never ticks, one class for every pawn regardless of kind (NPC or player, ADR-0001
 * carrier model, Pillar-1 kind-blindness). The owner-only cooldown end reaches the owning client through
 * UDefenceOwnerComponent on the PlayerController (story asn-016), never through this component.
 *
 * Creation: Find returns null for a pawn that never braced or struck, which every reader treats as T_act none and no
 * lockout. The first defence write calls FindOrAdd on the server, so the component's presence depends only on a
 * Prepare/Attack execution, never on pawn kind. (A default subobject on AStealthPawn would make Core include a Feature
 * header, against ADR-0007's include direction; decision logged 2026-10-27.)
 *
 * Lifecycle (ASN-R18): it lives on the pawn, so a rejoin into the same pawn and ReleaseToCrowd keep it; a PawnRecovery
 * re-pawn calls GetCore().AdoptForRecovery(lost pawn's core or null, Now); Reset clears it.
 */
UCLASS(NotBlueprintable, NotBlueprintType, ClassGroup = (Stealth))
class STEALTHGAMEPLAY_API UDefenceStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDefenceStateComponent();

	/** The component on Actor, or null when the pawn never had a defence write. */
	static UDefenceStateComponent* Find(const AActor& Actor);

	/** The component on Actor, created and registered (server only; refused with null on a client). */
	static UDefenceStateComponent* FindOrAdd(AActor& Actor);

	/** The state (read). */
	const FDefenceStateCore& GetCore() const { return Core; }

	/** The state (write: only the Assassination resolvers and the lifecycle hooks). */
	FDefenceStateCore& GetCore() { return Core; }

private:
	FDefenceStateCore Core;
};
