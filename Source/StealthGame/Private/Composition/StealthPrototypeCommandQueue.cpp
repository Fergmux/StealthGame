#include "StealthPrototypeCommandQueue.h"

#include "Engine/World.h"
#include "StealthCharacter.h"
#include "StealthGame.h"
#include "StealthGameMode.h"

double FStealthWorldCommandClock::GetServerTimeSeconds() const
{
	const UWorld* Current = World.Get();
	return Current ? Current->GetTimeSeconds() : 0.0;
}

FStealthPrototypeCommandQueue::FStealthPrototypeCommandQueue(AStealthGameMode* InGameMode)
	: GameMode(InGameMode)
{
}

void FStealthPrototypeCommandQueue::Submit(FMailboxId Mailbox, const FQueueItem& Item)
{
	const FCommandPayload* Payload = Item.Payload.TryGet<FCommandPayload>();
	AStealthGameMode* Mode = GameMode.Get();
	AStealthCharacter* Character = Payload ? Cast<AStealthCharacter>(Payload->Pawn.Pawn.Get()) : nullptr;
	if (Item.Type != EQueueItemType::PawnCommand || Mode == nullptr || Character == nullptr)
	{
		return;
	}
	++ResolvedCount;
	const ECommandKind Kind = static_cast<ECommandKind>(Payload->Kind);
	UE_LOGF(LogStealth, Verbose, "[cmd] %ls kind %d seq %u", *Character->GetName(), static_cast<int32>(Kind), Payload->ClientSeq.Seq);
	switch (Kind)
	{
	case ECommandKind::InteractBegin:
		Mode->HandleInteractRequest(Character, true);
		break;
	case ECommandKind::InteractEnd:
		Mode->HandleInteractRequest(Character, false);
		break;
	case ECommandKind::PrepareBegin:
		Mode->HandleDefenceRequest(Character);
		break;
	case ECommandKind::PrepareEnd:
		Character->StopDefence();
		break;
	case ECommandKind::Attack:
	{
		AStealthCharacter* Target = Payload->Target.Kind == ESubjectRefKind::Pawn ? Cast<AStealthCharacter>(Payload->Target.Pawn.Pawn.Get()) : nullptr;
		// A null target is a blind strike at the point (the prototype's rule for a pawn hidden in a bush).
		Mode->HandleAssassinationRequest(Character, Target, Payload->WorldPoint);
		break;
	}
	case ECommandKind::Traverse:
	case ECommandKind::HideEnter:
	case ECommandKind::HideExit:
		break;
	}
}
