#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "View/CourtFixtureId.h"

class APawn;
class AStealthPawn;
class IPawnCommandQueue;
class IStealthClock;
class UActionHub;

/**
 * STAND-IN for the IIF stage-2 Interact hold (ADR-0011 sections 4-5, IIF R8; the IIF slot resolver, stories iif-020..iif-055,
 * is not built). Added by Objectives story oc-005 so the production chain runs end to end: it replaces the prototype
 * fallback's InteractBegin / InteractEnd (AStealthGameMode::HandleInteractRequest) on the production path. It follows the
 * interact-target owner contract of ADR-0020 5.1 exactly:
 *   InteractBegin slot   the court is the command's target (FSubjectRef CourtSocket) or, when the client sent none, the nearest
 *                        court in reach (the IIF's prompt target; FindCourtInReach); none in reach: Voided (OutOfReach). The
 *                        hub's Interact predicate decides (Reject: Voided). Executed: D = GetInteractHoldSec() read ONCE,
 *                        OnInteractExecuted inline, OnActionStarted, HoldTimer(Complete) scheduled at now + D (BeginKey = the
 *                        Begin's key, ActionType Interact, a fresh ActionEpoch).
 *   InteractEnd slot     the hold cancels (Released): OnInteractEnded(Cancelled, Key) before OnActionCancelled.
 *   HoldTimer(Complete)  a timer of the pawn's current hold only (ActionEpoch); a pawn no longer legal (killed, struck out,
 *                        round ended) cancels Illegal; otherwise OnInteractEnded(Completed, Key) before OnActionCompleted.
 *   latched Complete     the queue's OnRejectedLatched: the hold ends and OnInteractCompleteLatched logs the attempt.
 *   CancelAll            the round-end staging pass: every hold ends with an invalid key (claim release only).
 * Not covered (IIF stage 2): the release debounce (ReleaseDebounceSec), condition cancels (moving, running), the owner's
 * reply batch and FHoldStatus. A second Begin while holding is Voided (the slot is Interacting). Kind-blind: an NPC decoy and
 * a player run the same path (no caller identity).
 */
class STEALTHGAMEPLAY_API FInteractHoldStandIn final : public IQueueResolver
{
public:
	struct FDeps
	{
		UActionHub* Hub = nullptr;
		IPawnCommandQueue* Queue = nullptr;
		const IStealthClock* Clock = nullptr;
		/** The nearest court fixture within reach of Pawn, or an unset id (the IIF target stand-in). */
		TFunction<FCourtFixtureId(const APawn&)> FindCourtInReach;
		/** True while Pawn's legality class is Active (the IIF's Illegal cancel). Unset: always legal. */
		TFunction<bool(const APawn&)> IsLegalityActive;
		/** Optional: the HoldTimer resolver of other action types (Voided without one). */
		IQueueResolver* NextHoldResolver = nullptr;
	};

	/** One running hold. */
	struct FHold
	{
		FCourtFixtureId Court;
		uint32 ActionEpoch = 0;
		FQueueKey BeginKey;
		double DueServerTime = 0.0;
	};

	explicit FInteractHoldStandIn(const FDeps& InDeps) : Deps(InDeps) {}

	/** PawnCommand InteractBegin / InteractEnd and HoldTimer(ActionType Interact); anything else is Voided. */
	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;
	virtual void OnRejectedLatched(const FQueueItem& Item, FQueueKey Key) override;

	/** Ends every running hold outside a slot (round-end staging, reset): OnInteractEnded(Cancelled, unset key). */
	void CancelAll();

	/** Pawn's running hold, or null. */
	const FHold* FindHold(const APawn* Pawn) const;

	/** Running holds per court (the IIF Interacting slots the claim consistency check compares with). */
	void GetInteractingSlotsByCourt(TMap<FCourtFixtureId, int32>& OutSlots) const;

	int32 GetHoldCount() const;

private:
	EResolveResult ResolveBegin(const FQueueItem& Item, AStealthPawn& Pawn, FQueueKey Key);
	EResolveResult ResolveEnd(AStealthPawn& Pawn, FQueueKey Key);
	EResolveResult ResolveComplete(const FQueueItem& Item, FQueueKey Key);

	FDeps Deps;
	TMap<TWeakObjectPtr<APawn>, FHold> Holds;
	uint32 NextActionEpoch = 1;
};
