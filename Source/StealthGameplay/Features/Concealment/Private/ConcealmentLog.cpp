// Story con-030: the concealment slice of the Round Event Log. See Server/ConcealmentLog.h.

#include "Server/ConcealmentLog.h"

FRoundLogEntry ConcealmentLog::MakeEntry(FQueueKey Key, double ServerTime, EConcealmentLogEvent Event, FParticipantId Participant,
	uint16 BushId, bool bPlayer, uint32 D2, uint32 D3, uint32 D4)
{
	FRoundLogEntry Entry;
	Entry.Key = Key;
	Entry.ServerTime = ServerTime;
	Entry.Type = ERoundLogType::Concealment;
	Entry.Code = static_cast<uint16>(Event);
	Entry.Participants[0] = Participant.Value;
	Entry.Data[0] = BushId;
	Entry.Data[1] = bPlayer ? 1u : 0u;
	Entry.Data[2] = D2;
	Entry.Data[3] = D3;
	Entry.Data[4] = D4;
	return Entry;
}

bool ConcealmentLog::Decode(const FRoundLogEntry& Entry, FConcealmentLogRow& OutRow)
{
	if (Entry.Type != ERoundLogType::Concealment || Entry.Code > static_cast<uint16>(EConcealmentLogEvent::BushStrike))
	{
		return false;
	}
	OutRow = FConcealmentLogRow();
	OutRow.Tick = Entry.Key.ServerTickIndex;
	OutRow.ServerTime = Entry.ServerTime;
	OutRow.Event = static_cast<EConcealmentLogEvent>(Entry.Code);
	OutRow.Participant = FParticipantId(Entry.Participants[0]);
	OutRow.BushId = static_cast<uint16>(Entry.Data[0]);
	OutRow.bPlayer = Entry.Data[1] != 0;
	if (OutRow.Event == EConcealmentLogEvent::BushStrike)
	{
		OutRow.CandidatesBeforeReach = static_cast<int32>(Entry.Data[2]);
		OutRow.CandidatesAfterReach = static_cast<int32>(Entry.Data[3]);
	}
	OutRow.bKilled = OutRow.Event == EConcealmentLogEvent::Emerge && Entry.Data[4] != 0;
	return true;
}

TArray<FConcealmentLogRow> ConcealmentLog::DecodeAll(TConstArrayView<FRoundLogEntry> Entries)
{
	TArray<FConcealmentLogRow> Rows;
	for (const FRoundLogEntry& Entry : Entries)
	{
		FConcealmentLogRow Row;
		if (Decode(Entry, Row))
		{
			Rows.Add(Row);
		}
	}
	return Rows;
}
