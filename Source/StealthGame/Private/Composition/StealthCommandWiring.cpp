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

// ---- FStealthPrototypeCommandFallback (stand-in until IIF stage 2) -------------------------------------------------

EResolveResult FStealthPrototypeCommandFallback::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	const FCommandPayload* Payload = Item.Type == EQueueItemType::PawnCommand ? Item.Payload.TryGet<FCommandPayload>() : nullptr;
	if (Payload != nullptr && (static_cast<ECommandKind>(Payload->Kind) == ECommandKind::InteractBegin || static_cast<ECommandKind>(Payload->Kind) == ECommandKind::InteractEnd))
	{
		return Interact != nullptr ? Interact->Resolve(Item, Key) : EResolveResult::Voided; // every pawn kind, the same path
	}
	if (Payload != nullptr && (static_cast<ECommandKind>(Payload->Kind) == ECommandKind::HideEnter || static_cast<ECommandKind>(Payload->Kind) == ECommandKind::HideExit))
	{
		return Hide != nullptr ? Hide->Resolve(Item, Key) : EResolveResult::Voided; // Concealment's slot (con-004/005)
	}
	AStealthGameMode* Mode = GameMode.Get();
	AStealthCharacter* Character = Payload != nullptr ? Cast<AStealthCharacter>(Payload->Pawn.Pawn.Get()) : nullptr;
	if (Payload == nullptr || Mode == nullptr || Character == nullptr)
	{
		return EResolveResult::Voided;
	}
	switch (static_cast<ECommandKind>(Payload->Kind))
	{
	case ECommandKind::Traverse:
		return EResolveResult::Executed; // a no-op, as under the D53 prototype queue
	default:
		return EResolveResult::Voided; // Attack and Prepare never reach the fallback (Assassination resolves them)
	}
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
