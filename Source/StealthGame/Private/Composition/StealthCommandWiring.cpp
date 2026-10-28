// Assassination stories asn-012 / asn-013 play integration (production/epics/assassination-defence-reversal/): the
// command path into Round's queue and the stand-ins the slot needs until Concealment and IIF stage 2 exist.
// See StealthCommandWiring.h.

#include "StealthCommandWiring.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/Core/Round/StealthRoundRosterGateway.h"
#include "Server/StealthCompositionRoot.h"
#include "StealthBush.h"
#include "StealthCharacter.h"
#include "StealthGame.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "StealthPlayerState.h"

double FStealthWorldCommandClock::GetServerTimeSeconds() const
{
	const UWorld* Current = World.Get();
	return Current ? Current->GetTimeSeconds() : 0.0;
}

// ---- FStealthRoundCommandQueue -------------------------------------------------------------------------------------

FStealthRoundCommandQueue::FStealthRoundCommandQueue(URoundMatchStore* InRound, UStealthMatchAdapters* InGenerations)
	: Round(InRound)
	, Generations(InGenerations)
{
}

void FStealthRoundCommandQueue::Submit(FMailboxId Mailbox, const FQueueItem& Item)
{
	URoundMatchStore* Store = Round.Get();
	FResolutionQueueCore* Queue = Store != nullptr ? Store->GetRoundQueue() : nullptr;
	if (Queue == nullptr || !Mailbox.IsValid())
	{
		++DroppedCount;
		UE_LOGF(LogStealth, Verbose, "[cmd] dropped: no round queue (mailbox %u)", Mailbox.Value);
		return;
	}
	++SubmittedCount;
	const FParticipantId Participant = Mailbox.GetParticipant();
	const UStealthMatchAdapters* Adapters = Generations.Get();
	if (Participant.IsValid() && Adapters != nullptr)
	{
		Queue->Submit(Mailbox, Item, Adapters->GetConnectionGeneration(Participant));
		return;
	}
	Queue->Submit(Mailbox, Item);
}

void FStealthRoundCommandQueue::ScheduleAt(double DueServerTime, const FQueueItem& Item)
{
	URoundMatchStore* Store = Round.Get();
	if (FResolutionQueueCore* Queue = Store != nullptr ? Store->GetRoundQueue() : nullptr)
	{
		Queue->ScheduleAt(DueServerTime, Item);
	}
}

FEpochToken FStealthRoundEpochSource::GetCurrentEpoch() const
{
	const URoundRosterStoreGateway* Gateway = Roster.Get();
	return Gateway != nullptr ? Gateway->GetEpochToken() : FEpochToken();
}

// ---- FStealthPrototypeBushSpotQuery (stand-in) ---------------------------------------------------------------------

void FStealthPrototypeBushSpotQuery::GetBushes(TArray<const AStealthBush*>& Out) const
{
	Out.Reset();
	if (UWorld* Target = World.Get())
	{
		for (TActorIterator<AStealthBush> It(Target); It; ++It)
		{
			if (IsValid(*It))
			{
				Out.Add(*It);
			}
		}
	}
	Out.Sort([](const AStealthBush& A, const AStealthBush& B) { return A.GetFName().LexicalLess(B.GetFName()); });
}

bool FStealthPrototypeBushSpotQuery::GetFootprint(uint16 BushId, FBox2D& OutFootprint) const
{
	TArray<const AStealthBush*> Bushes;
	GetBushes(Bushes);
	const int32 Index = static_cast<int32>(BushId) - 1;
	if (!Bushes.IsValidIndex(Index))
	{
		return false;
	}
	OutFootprint = Bushes[Index]->GetFootprint2D();
	return OutFootprint.bIsValid;
}

bool FStealthPrototypeBushSpotQuery::FindSpotZone(const FVector& WorldPoint, uint16& OutBushId) const
{
	TArray<const AStealthBush*> Bushes;
	GetBushes(Bushes);
	const FVector2D Point(WorldPoint);
	for (int32 Index = 0; Index < Bushes.Num(); ++Index)
	{
		const FBox2D Zone = Bushes[Index]->GetFootprint2D().ExpandBy(ToleranceCm);
		if (Zone.bIsValid && Zone.IsInside(Point))
		{
			OutBushId = static_cast<uint16>(Index + 1);
			return true;
		}
	}
	return false;
}

void FStealthPrototypeBushSpotQuery::GetSpotOccupants(uint16 BushId, const APawn* Attacker, TArray<APawn*>& OutOccupants) const
{
	OutOccupants.Reset();
	FBox2D Footprint(ForceInit);
	UWorld* Target = World.Get();
	if (Target == nullptr || !GetFootprint(BushId, Footprint))
	{
		return;
	}
	// A capsule standing in the volume overlaps it; its centre lies within the footprint grown by the capsule radius.
	const FBox2D Occupied = Footprint.ExpandBy(AStealthPawn::SampleCapsuleRadiusCm);
	for (TActorIterator<AStealthCharacter> It(Target); It; ++It)
	{
		AStealthCharacter* Character = *It;
		if (Character != Attacker && IsConcealed(Character) && Occupied.IsInside(FVector2D(Character->GetActorLocation())))
		{
			OutOccupants.Add(Character);
		}
	}
}

bool FStealthPrototypeBushSpotQuery::IsConcealed(const APawn* Pawn) const
{
	const AStealthCharacter* Character = Cast<AStealthCharacter>(Pawn);
	return Character != nullptr && !Character->IsDying() && Character->IsConcealed();
}

// ---- FStealthPrototypeCommandFallback (stand-in until IIF stage 2) -------------------------------------------------

EResolveResult FStealthPrototypeCommandFallback::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	const FCommandPayload* Payload = Item.Type == EQueueItemType::PawnCommand ? Item.Payload.TryGet<FCommandPayload>() : nullptr;
	AStealthGameMode* Mode = GameMode.Get();
	AStealthCharacter* Character = Payload != nullptr ? Cast<AStealthCharacter>(Payload->Pawn.Pawn.Get()) : nullptr;
	if (Payload == nullptr || Mode == nullptr || Character == nullptr)
	{
		return EResolveResult::Voided;
	}
	switch (static_cast<ECommandKind>(Payload->Kind))
	{
	case ECommandKind::InteractBegin:
		Mode->HandleInteractRequest(Character, true);
		return EResolveResult::Executed;
	case ECommandKind::InteractEnd:
		Mode->HandleInteractRequest(Character, false);
		return EResolveResult::Executed;
	case ECommandKind::Traverse:
	case ECommandKind::HideEnter:
	case ECommandKind::HideExit:
		return EResolveResult::Executed; // no-ops, as under the D53 prototype queue
	default:
		return EResolveResult::Voided; // Attack and Prepare never reach the fallback (Assassination resolves them)
	}
}

// ---- FStealthPrototypeBodyRemovalObserver (stand-in until Concealment) --------------------------------------------

void FStealthPrototypeBodyRemovalObserver::OnBodyRemovalCommitted(APawn* Body)
{
	UE_LOGF(LogStealth, Verbose, "[Encounter] body removal committed for %ls", Body != nullptr ? *Body->GetName() : TEXT("?"));
}

// ---- FStealthControllerReplyTransport --------------------------------------------------------------------------------

void FStealthControllerReplyTransport::SendCommandReplies(FReplySinkId Sink, const TArray<FCommandReply>& Replies)
{
	UWorld* Target = World.Get();
	if (Target == nullptr)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = Target->GetPlayerControllerIterator(); It; ++It)
	{
		AStealthPlayerController* Controller = Cast<AStealthPlayerController>(It->Get());
		const FReplySinkId Held = Controller != nullptr ? Controller->GetReplySink() : FReplySinkId();
		if (Controller != nullptr && Held.Index == Sink.Index && Held.Generation == Sink.Generation)
		{
			Controller->ClientCommandReplies(Replies);
			return;
		}
	}
}
