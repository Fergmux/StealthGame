// Objectives Chain story oc-005: the STAND-IN Interact hold until IIF stage 2. See Server/Core/InteractHoldStandIn.h.

#include "Server/Core/InteractHoldStandIn.h"

#include "Server/ActionHub.h"
#include "Server/Core/PawnCommandCore.h"
#include "Server/StealthClock.h"
#include "View/Core/StealthPawn.h"
#include "View/NetPawnCommand.h"

EResolveResult FInteractHoldStandIn::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	if (Item.Type == EQueueItemType::HoldTimer)
	{
		const FHoldTimerPayload* Hold = Item.Payload.TryGet<FHoldTimerPayload>();
		if (Hold != nullptr && Hold->ActionType == static_cast<uint8>(EActionType::Interact))
		{
			return ResolveComplete(Item, Key);
		}
		return Deps.NextHoldResolver != nullptr ? Deps.NextHoldResolver->Resolve(Item, Key) : EResolveResult::Voided;
	}
	const FCommandPayload* Cmd = Item.Type == EQueueItemType::PawnCommand ? Item.Payload.TryGet<FCommandPayload>() : nullptr;
	AStealthPawn* Pawn = Cmd != nullptr ? Cast<AStealthPawn>(Cmd->Pawn.Pawn.Get()) : nullptr;
	if (Pawn == nullptr)
	{
		return EResolveResult::Voided;
	}
	switch (static_cast<ECommandKind>(Cmd->Kind))
	{
	case ECommandKind::InteractBegin:
		return ResolveBegin(Item, *Pawn, Key);
	case ECommandKind::InteractEnd:
		return ResolveEnd(*Pawn, Key);
	default:
		return EResolveResult::Voided;
	}
}

EResolveResult FInteractHoldStandIn::ResolveBegin(const FQueueItem& Item, AStealthPawn& Pawn, FQueueKey Key)
{
	if (Deps.Hub == nullptr || Deps.Queue == nullptr || Deps.Clock == nullptr || Holds.Contains(&Pawn))
	{
		return EResolveResult::Voided; // a wiring gap, or the slot is already Interacting
	}
	FCommandPayload Cmd = Item.Payload.Get<FCommandPayload>();
	FCourtFixtureId Court(Cmd.Target.Kind == ESubjectRefKind::CourtSocket ? Cmd.Target.CourtSocketId : 0);
	if (!Court.IsSet() && Deps.FindCourtInReach)
	{
		Court = Deps.FindCourtInReach(Pawn);
	}
	if (!Court.IsSet())
	{
		return EResolveResult::Voided; // OutOfReach: no court fixture in reach
	}
	Cmd.Target = FSubjectRef::ForCourtSocket(Court.Value);
	if (Deps.Hub->ResolvePredicate(EActionType::Interact, Pawn, Cmd).Verdict == EPredicateVerdict::Reject)
	{
		return EResolveResult::Voided;
	}
	IInteractTargetOwner* Owner = Deps.Hub->ResolveInteractTargetOwner();
	if (Owner == nullptr)
	{
		return EResolveResult::Voided;
	}
	// D is read once, at execution: a later tuning publish never moves this hold's timer.
	const double Due = Deps.Clock->GetServerTimeSeconds() + Owner->GetInteractHoldSec();
	FHold& Hold = Holds.Add(&Pawn);
	Hold.Court = Court;
	Hold.ActionEpoch = NextActionEpoch++;
	Hold.BeginKey = Key;
	Hold.DueServerTime = Due;
	Owner->OnInteractExecuted(Pawn, Court, Key);
	Deps.Hub->OnActionStarted.Broadcast(&Pawn, EActionType::Interact);

	FHoldTimerPayload Timer;
	Timer.Pawn.Pawn = &Pawn;
	Timer.Kind = EHoldTimerKind::Complete;
	Timer.ActionType = static_cast<uint8>(EActionType::Interact);
	Timer.ActionEpoch = Hold.ActionEpoch;
	Timer.BeginKey = Key;
	Deps.Queue->ScheduleAt(Due, FQueueItem::Make(EQueueItemType::HoldTimer, Item.Expected, FMailboxId::ServerLocal(), Timer));
	return EResolveResult::Executed;
}

EResolveResult FInteractHoldStandIn::ResolveEnd(AStealthPawn& Pawn, FQueueKey Key)
{
	FHold Hold;
	if (!Holds.RemoveAndCopyValue(&Pawn, Hold))
	{
		return EResolveResult::Voided; // no hold to end
	}
	if (IInteractTargetOwner* Owner = Deps.Hub != nullptr ? Deps.Hub->ResolveInteractTargetOwner() : nullptr)
	{
		Owner->OnInteractEnded(Pawn, Hold.Court, EInteractEnd::Cancelled, Key);
	}
	if (Deps.Hub != nullptr)
	{
		Deps.Hub->OnActionCancelled.Broadcast(&Pawn, EActionType::Interact, ECancelReason::Released);
	}
	return EResolveResult::Executed;
}

EResolveResult FInteractHoldStandIn::ResolveComplete(const FQueueItem& Item, FQueueKey Key)
{
	const FHoldTimerPayload& Timer = Item.Payload.Get<FHoldTimerPayload>();
	AStealthPawn* Pawn = Cast<AStealthPawn>(Timer.Pawn.Pawn.Get());
	const FHold* Found = Pawn != nullptr ? Holds.Find(Pawn) : nullptr;
	if (Found == nullptr || Found->ActionEpoch != Timer.ActionEpoch || Timer.Kind != EHoldTimerKind::Complete)
	{
		return EResolveResult::Voided; // a timer of an ended or older hold
	}
	const FHold Hold = *Found;
	Holds.Remove(Pawn);
	const bool bLegal = !Deps.IsLegalityActive || Deps.IsLegalityActive(*Pawn);
	IInteractTargetOwner* Owner = Deps.Hub != nullptr ? Deps.Hub->ResolveInteractTargetOwner() : nullptr;
	if (Owner != nullptr)
	{
		Owner->OnInteractEnded(*Pawn, Hold.Court, bLegal ? EInteractEnd::Completed : EInteractEnd::Cancelled, Key);
	}
	if (Deps.Hub != nullptr)
	{
		if (bLegal)
		{
			Deps.Hub->OnActionCompleted.Broadcast(Pawn, EActionType::Interact);
		}
		else
		{
			Deps.Hub->OnActionCancelled.Broadcast(Pawn, EActionType::Interact, ECancelReason::Illegal);
		}
	}
	return EResolveResult::Executed;
}

void FInteractHoldStandIn::OnRejectedLatched(const FQueueItem& Item, FQueueKey Key)
{
	const FHoldTimerPayload* Timer = Item.Type == EQueueItemType::HoldTimer ? Item.Payload.TryGet<FHoldTimerPayload>() : nullptr;
	if (Timer == nullptr || Timer->ActionType != static_cast<uint8>(EActionType::Interact))
	{
		if (Deps.NextHoldResolver != nullptr && Item.Type == EQueueItemType::HoldTimer)
		{
			Deps.NextHoldResolver->OnRejectedLatched(Item, Key);
		}
		return;
	}
	AStealthPawn* Pawn = Cast<AStealthPawn>(Timer->Pawn.Pawn.Get());
	const FHold* Found = Pawn != nullptr ? Holds.Find(Pawn) : nullptr;
	if (Found == nullptr || Found->ActionEpoch != Timer->ActionEpoch)
	{
		return;
	}
	const FCourtFixtureId Court = Found->Court;
	Holds.Remove(Pawn);
	if (IInteractTargetOwner* Owner = Deps.Hub != nullptr ? Deps.Hub->ResolveInteractTargetOwner() : nullptr)
	{
		Owner->OnInteractCompleteLatched(*Pawn, Court, Key);
	}
}

void FInteractHoldStandIn::CancelAll()
{
	TMap<TWeakObjectPtr<APawn>, FHold> Ending = MoveTemp(Holds);
	Holds.Reset();
	IInteractTargetOwner* Owner = Deps.Hub != nullptr ? Deps.Hub->ResolveInteractTargetOwner() : nullptr;
	for (const TPair<TWeakObjectPtr<APawn>, FHold>& Pair : Ending)
	{
		const AStealthPawn* Pawn = Cast<AStealthPawn>(Pair.Key.Get());
		if (Owner != nullptr && Pawn != nullptr)
		{
			Owner->OnInteractEnded(*Pawn, Pair.Value.Court, EInteractEnd::Cancelled, FQueueKey()); // outside a slot: claim release only
		}
	}
}

const FInteractHoldStandIn::FHold* FInteractHoldStandIn::FindHold(const APawn* Pawn) const
{
	return Pawn != nullptr ? Holds.Find(const_cast<APawn*>(Pawn)) : nullptr;
}

void FInteractHoldStandIn::GetInteractingSlotsByCourt(TMap<FCourtFixtureId, int32>& OutSlots) const
{
	OutSlots.Reset();
	for (const TPair<TWeakObjectPtr<APawn>, FHold>& Pair : Holds)
	{
		if (Pair.Key.IsValid())
		{
			++OutSlots.FindOrAdd(Pair.Value.Court);
		}
	}
}

int32 FInteractHoldStandIn::GetHoldCount() const
{
	return Holds.Num();
}
