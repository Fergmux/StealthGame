#include "Server/Core/Round/StealthRoundGrace.h"

void FRoundGraceTable::Open(const FRoundGraceRecord& Record)
{
	Close(Record.Participant);
	Records.Add(Record);
}

const FRoundGraceRecord* FRoundGraceTable::Find(FParticipantId Participant) const
{
	return Records.FindByPredicate([Participant](const FRoundGraceRecord& R) { return R.Participant == Participant; });
}

void FRoundGraceTable::MarkKicked(FParticipantId Participant, bool bKicked)
{
	for (FRoundGraceRecord& Record : Records)
	{
		if (Record.Participant == Participant)
		{
			Record.bKicked = Record.bKicked || bKicked;
		}
	}
}

bool FRoundGraceTable::Close(FParticipantId Participant)
{
	return Records.RemoveAll([Participant](const FRoundGraceRecord& R) { return R.Participant == Participant; }) > 0;
}

FQueueItem FRoundGraceTable::MakeTimerItem(const FRoundGraceRecord& Record)
{
	FTimerPayload Payload;
	Payload.Kind = ETimerKind::GraceExpiry;
	Payload.Subject = FSubjectRef::ForParticipant(Record.Participant);
	Payload.Generation = Record.ExpectedConnectionGeneration;
	Payload.Seq = Record.Participant.Value;
	return FQueueItem::Make(EQueueItemType::TimerFired, Record.ExpectedEpoch, FMailboxId::ServerLocal(), Payload);
}
