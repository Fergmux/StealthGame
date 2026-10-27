#include "Server/StealthPenaltyEventRows.h"

#include "Server/EventLog/StealthRoundLogWriter.h"
#include "Server/StealthClock.h"
#include "Server/StealthFaultSink.h"

// Story nkp-004 (ADR-0019 section 7): Penalty's Event Log rows and their single write path. Nothing in this file
// formats, logs or traces a row or a field (StealthGame.Penalty.NoLoggingLint).

namespace StealthPenaltyEventLogLocal
{
	const FName FaultSource(TEXT("PenaltyEventLog"));

	void PutDouble(FRoundLogEntry& Entry, int32 Index, double Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Entry.Data[Index] = static_cast<uint32>(Bits & 0xFFFFFFFFull);
		Entry.Data[Index + 1] = static_cast<uint32>(Bits >> 32);
	}

	double GetDouble(const FRoundLogEntry& Entry, int32 Index)
	{
		const uint64 Bits = static_cast<uint64>(Entry.Data[Index]) | (static_cast<uint64>(Entry.Data[Index + 1]) << 32);
		double Value = 0.0;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	template <typename RowType>
	void Begin(FRoundLogEntry& Out, FParticipantId Participant)
	{
		Out.Type = RowType::Type;
		Out.Participants[0] = Participant.Value;
	}
}

namespace PenaltyEventRows
{
	using namespace StealthPenaltyEventLogLocal;

	void Encode(const FPenaltyStrikeCountedRow& Row, FRoundLogEntry& Out)
	{
		Begin<FPenaltyStrikeCountedRow>(Out, Row.Killer);
		Out.Data[0] = Row.StrikeAfter;
		Out.Data[1] = Row.TallyAfter;
		Out.Data[2] = Row.bGraced ? 1u : 0u;
	}

	void Encode(const FPenaltyRespawnScheduledRow& Row, FRoundLogEntry& Out)
	{
		Begin<FPenaltyRespawnScheduledRow>(Out, Row.Participant);
		Out.Code = static_cast<uint16>(Row.Cause);
		PutDouble(Out, 0, Row.LostServerTime);
		PutDouble(Out, 2, Row.DueServerTime);
	}

	void Encode(const FPenaltyInformantRespawnRow& Row, FRoundLogEntry& Out)
	{
		Begin<FPenaltyInformantRespawnRow>(Out, Row.Participant);
		PutDouble(Out, 0, Row.PickLatencySec);
		Out.Data[2] = Row.WideningLevel;
	}

	void Encode(const FPenaltyRespawnCancelledRow& Row, FRoundLogEntry& Out)
	{
		Begin<FPenaltyRespawnCancelledRow>(Out, Row.Participant);
		Out.Code = static_cast<uint16>(Row.Reason);
	}

	void Encode(const FPenaltyPawnLostRow& Row, FRoundLogEntry& Out)
	{
		Begin<FPenaltyPawnLostRow>(Out, Row.Participant);
		Out.Code = static_cast<uint16>(Row.Cause);
	}

	bool Decode(const FRoundLogEntry& Entry, FPenaltyStrikeCountedRow& Out)
	{
		if (Entry.Type != FPenaltyStrikeCountedRow::Type)
		{
			return false;
		}
		Out.Killer = FParticipantId(Entry.Participants[0]);
		Out.StrikeAfter = static_cast<uint8>(Entry.Data[0]);
		Out.TallyAfter = static_cast<uint8>(Entry.Data[1]);
		Out.bGraced = Entry.Data[2] != 0;
		return true;
	}

	bool Decode(const FRoundLogEntry& Entry, FPenaltyRespawnScheduledRow& Out)
	{
		if (Entry.Type != FPenaltyRespawnScheduledRow::Type)
		{
			return false;
		}
		Out.Participant = FParticipantId(Entry.Participants[0]);
		Out.Cause = static_cast<ERespawnCause>(Entry.Code);
		Out.LostServerTime = GetDouble(Entry, 0);
		Out.DueServerTime = GetDouble(Entry, 2);
		return true;
	}

	bool Decode(const FRoundLogEntry& Entry, FPenaltyInformantRespawnRow& Out)
	{
		if (Entry.Type != FPenaltyInformantRespawnRow::Type)
		{
			return false;
		}
		Out.Participant = FParticipantId(Entry.Participants[0]);
		Out.PickLatencySec = GetDouble(Entry, 0);
		Out.WideningLevel = static_cast<uint8>(Entry.Data[2]);
		return true;
	}

	bool Decode(const FRoundLogEntry& Entry, FPenaltyRespawnCancelledRow& Out)
	{
		if (Entry.Type != FPenaltyRespawnCancelledRow::Type)
		{
			return false;
		}
		Out.Participant = FParticipantId(Entry.Participants[0]);
		Out.Reason = static_cast<ERespawnCancelReason>(Entry.Code);
		return true;
	}

	bool Decode(const FRoundLogEntry& Entry, FPenaltyPawnLostRow& Out)
	{
		if (Entry.Type != FPenaltyPawnLostRow::Type)
		{
			return false;
		}
		Out.Participant = FParticipantId(Entry.Participants[0]);
		Out.Cause = static_cast<EPenaltyPawnLostCause>(Entry.Code);
		return true;
	}
}

bool FPenaltyEventLog::Append(FRoundLogEntry& Entry, uint32 RoundEpoch)
{
	using namespace StealthPenaltyEventLogLocal;
	if (Slot == nullptr || !Slot->IsInSlot())
	{
		if (Faults != nullptr)
		{
			Faults->Report(EStealthFault::QueueMisuse, FaultSource, PenaltyFaultDetail::RowOutsideSlot);
		}
		return false;
	}
	if (Writer == nullptr)
	{
		if (Faults != nullptr)
		{
			Faults->Report(EStealthFault::DeadRegistration, FaultSource, PenaltyFaultDetail::NoLogWriter);
		}
		return false;
	}
	Entry.Key = Slot->CurrentKey();
	Entry.ServerTime = Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0;
	Entry.RoundEpoch = RoundEpoch;
	Entry.SourceMailbox = FMailboxId::ServerLocal();
	Writer->Append(Entry);
	return true;
}
