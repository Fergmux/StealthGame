#include "StealthPenaltyQueueAdapter.h"

#include "Server/Queue/StealthResolutionQueueCore.h"
#include "StealthPenaltyCore.h"

// Stories nkp-009 (schedule, cancel, the resolver registration) and nkp-012 (retries reuse the same payload shape).
// ADR-0007 thin adapter: no rule here; the core decides.

FPenaltyRespawnQueueAdapter::FPenaltyRespawnQueueAdapter(FResolutionQueueCore* InQueue, TFunction<uint64()> InMatchId)
	: Queue(InQueue)
	, MatchId(MoveTemp(InMatchId))
{
}

uint64 FPenaltyRespawnQueueAdapter::ScheduleRespawnItem(double DueServerTime, const FPenaltyRespawnItem& Item)
{
	if (Queue == nullptr || !Item.Participant.IsValid())
	{
		return 0;
	}
	FTimerPayload Payload;
	Payload.Kind = ETimerKind::InformantRespawn;
	Payload.Subject = FSubjectRef::ForParticipant(Item.Participant);
	Payload.Generation = 0;
	Payload.Seq = Item.RequestSeq;
	Payload.Phase = 0;
	const FEpochToken Expected(MatchId ? MatchId() : 0, Item.RoundEpoch);
	return Queue->ScheduleAt(DueServerTime, FQueueItem::Make(EQueueItemType::TimerFired, Expected, FMailboxId::ServerLocal(), Payload)).Id;
}

void FPenaltyRespawnQueueAdapter::CancelRespawnItem(uint64 Handle)
{
	if (Queue != nullptr && Handle != 0)
	{
		Queue->CancelTimer(FQueueItemHandle{ Handle });
	}
}

EResolveResult FPenaltyRespawnQueueAdapter::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	const FTimerPayload* Payload = Item.Payload.TryGet<FTimerPayload>();
	if (Core == nullptr || Payload == nullptr || Payload->Kind != ETimerKind::InformantRespawn || Payload->Subject.Kind != ESubjectRefKind::Participant)
	{
		return EResolveResult::Voided;
	}
	return Core->ResolveRespawnItem(Payload->Subject.Participant, Payload->Seq, Item.Expected.RoundEpoch) ? EResolveResult::Executed : EResolveResult::Voided;
}
