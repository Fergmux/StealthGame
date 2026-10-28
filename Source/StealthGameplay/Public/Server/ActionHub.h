#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/CancelReason.h"
#include "Server/Core/CommandPredicate.h"
#include "Server/Core/InteractTargetOwner.h"
#include "Server/Core/PawnCommandCore.h"
#include "Server/Core/ReplySink.h"
#include "Server/StealthLogFaultSink.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakInterfacePtr.h"
#include "View/CommandReplyTypes.h"

#include "ActionHub.generated.h"

class AStealthPawn;
class IStealthFaultSink;

/** Started and Completed: (pawn, action). Native only: never dynamic, never Blueprint-assignable. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPawnAction, AStealthPawn*, EActionType);

/** Cancelled: (pawn, action, server-only reason). */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPawnActionCancelled, AStealthPawn*, EActionType, ECancelReason);

/**
 * The server-only action hub (ADR-0011 section 6; GDD R11; story iif-003): one per server world. Features subscribe
 * once to its three native events instead of once per pawn, and register one ICommandPredicate per EActionType,
 * weakly, from the composition root.
 *
 * Events fire only inside queue slots, in key order (the callers enforce that in later stories); no listener may relay
 * one to a client except through its own Visibility Policy audience. Nothing here carries caller identity.
 *
 * Registry rules (pinned by story iif-003): re-registering a type replaces the previous predicate; registering a null
 * or dead predicate is refused with a CommandNoPredicate fault; resolving a type whose predicate is missing or whose
 * object was destroyed reports one CommandNoPredicate fault and answers Reject(NotNow), never a crash.
 *
 * Usage (composition root, server):
 *   UActionHub* Hub = UActionHub::Get(World);
 *   Hub->RegisterPredicate(EActionType::Prepare, TWeakInterfacePtr<ICommandPredicate>(AssassinationPredicate));
 *   Hub->OnActionStarted.AddUObject(this, &UMyFeature::HandleStarted);
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API UActionHub : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Server worlds only (authority game worlds; never a pure client). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** The hub of World, or null (client worlds have none). */
	static UActionHub* Get(const UWorld* World);

	/** Registers (or replaces) the predicate for Type. A null or dead predicate is refused with a fault (false). */
	bool RegisterPredicate(EActionType Type, TWeakInterfacePtr<ICommandPredicate> Predicate);

	/** Drops the registration for Type. */
	void UnregisterPredicate(EActionType Type);

	/** True when Type has a live predicate. */
	bool HasPredicate(EActionType Type) const;

	/**
	 * Asks Type's predicate. Missing or stale: one CommandNoPredicate fault and Reject(NotNow). Side-effect-free apart
	 * from that fault (the dry run and the slot call the same function).
	 */
	FPredicateResult ResolvePredicate(EActionType Type, const AStealthPawn& Pawn, const FCommandPayload& Cmd) const;

	/**
	 * Registers (or replaces) the one interact-target owner (ADR-0020 5.1, the ADR-0011 amendment; Objectives story oc-005),
	 * weakly. A null or dead owner is refused with a DeadRegistration fault (false).
	 */
	bool RegisterInteractTargetOwner(TWeakInterfacePtr<IInteractTargetOwner> Owner);

	/** Drops the interact-target owner registration. */
	void UnregisterInteractTargetOwner() { InteractTargetOwner.Reset(); bInteractTargetOwnerRegistered = false; }

	/** True when a live interact-target owner is registered. */
	bool HasInteractTargetOwner() const { return InteractTargetOwner.IsValid(); }

	/**
	 * The live interact-target owner, or null. An owner that was registered and has since been destroyed reports one
	 * DeadRegistration fault per call (ADR-0007 section 4: a dead registration is loud, never a silent dead feature).
	 */
	IInteractTargetOwner* ResolveInteractTargetOwner() const;

	/** Overrides the fault sink (tests); null restores the hub's own log sink. */
	void SetFaultSink(IStealthFaultSink* InFaultSink) { InjectedFaultSink = InFaultSink; }

	/**
	 * The composition root's command wiring: the seams every pawn's FPawnCommandCore binds at BeginPlay on the
	 * server (queue, clock, epoch, legality, fault sink). Set once per world; null until set.
	 */
	void SetCommandWiring(const FPawnCommandCoreDeps& InDeps) { CommandWiring = InDeps; bHasCommandWiring = true; }
	const FPawnCommandCoreDeps* GetCommandWiring() const { return bHasCommandWiring ? &CommandWiring : nullptr; }

	/** The world's reply-sink table (ADR-0011 section 8): controllers allocate a sink per connection; the drain end flushes it. */
	FReplySinkTable& GetReplySinks() { return ReplySinks; }

	/** An action executed (fired inside its queue slot). */
	FOnPawnAction OnActionStarted;

	/** An action ended without completing (fired inside its queue slot). */
	FOnPawnActionCancelled OnActionCancelled;

	/** An action completed (fired inside its queue slot). */
	FOnPawnAction OnActionCompleted;

private:
	IStealthFaultSink& GetFaultSink() const;

	TWeakInterfacePtr<ICommandPredicate> Predicates[GActionTypeCount];
	TWeakInterfacePtr<IInteractTargetOwner> InteractTargetOwner;
	bool bInteractTargetOwnerRegistered = false;

	IStealthFaultSink* InjectedFaultSink = nullptr;
	mutable FStealthLogFaultSink LogFaultSink;

	FPawnCommandCoreDeps CommandWiring;
	bool bHasCommandWiring = false;

	FReplySinkTable ReplySinks;
};
