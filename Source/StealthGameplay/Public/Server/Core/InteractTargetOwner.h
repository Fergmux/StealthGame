#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "View/CourtFixtureId.h"
#include "View/StealthQueueTypes.h"

#include "InteractTargetOwner.generated.h"

class AStealthPawn;

/** How an Interact hold ended (ADR-0020 5.1): the HoldTimer(Complete) slot, or any cancel (release, condition, removal). */
enum class EInteractEnd : uint8
{
	Completed,
	Cancelled,
};

/**
 * The interact-target owner (ADR-0020 section 5.1, requested as an amendment to ADR-0011; declared in Core by Objectives story
 * oc-006 because the IIF stage-2 slot resolver is not built yet). Consumer-owned like ICommandPredicate: Objectives implements
 * it and the composition root registers it weakly with UActionHub::RegisterInteractTargetOwner. Exactly one owner is
 * registered for Interact; a second interact-target kind would need the amendment to route per kind.
 *
 * Call contract (the IIF side; today the composition root's Interact hold STAND-IN):
 *   GetInteractHoldSec   read ONCE at execution to schedule HoldTimer(Complete); one value for every target (no court
 *                        parameter), so D can never depend on a court's role (AC-OC-08).
 *   OnInteractExecuted   inline in the Begin's slot after the command executed, for every pawn kind, no caller identity.
 *   OnInteractEnded      inline in the slot that ends the hold (HoldTimer(Complete) -> Completed; release, ConditionCancel,
 *                        Superseded, Illegal, PawnRemoved -> Cancelled), BEFORE OnActionCompleted / OnActionCancelled fan out,
 *                        with CurrentKey() as Key. Outside a slot (round-end staging, pawn teardown) Key is unset and End is
 *                        Cancelled: the owner only releases its claim. A pending debounced End keeps the claim until the
 *                        DebounceExpiry slot ends the hold.
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UInteractTargetOwner : public UInterface
{
	GENERATED_BODY()
};

class STEALTHGAMEPLAY_API IInteractTargetOwner
{
	GENERATED_BODY()

public:
	/** The hold duration D in seconds, the same for every target. */
	virtual double GetInteractHoldSec() const = 0;

	/** The Begin executed at Court (inline in its slot). */
	virtual void OnInteractExecuted(const AStealthPawn& Pawn, FCourtFixtureId Court, FQueueKey Key) = 0;

	/** The hold ended (inline in the ending slot; Key unset outside a slot). */
	virtual void OnInteractEnded(const AStealthPawn& Pawn, FCourtFixtureId Court, EInteractEnd End, FQueueKey Key) = 0;

	/**
	 * The HoldTimer(Complete) of a hold at Court was rejected by the latch (ADR-0002 8.1; the queue's OnRejectedLatched). The
	 * owner may log the attempt (Round R19 TendCompleted(NoEffect, Latched)) and releases its claim; nothing else changes.
	 */
	virtual void OnInteractCompleteLatched(const AStealthPawn& Pawn, FCourtFixtureId Court, FQueueKey Key) = 0;
};
