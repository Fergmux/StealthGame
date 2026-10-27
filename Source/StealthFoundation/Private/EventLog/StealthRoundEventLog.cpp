#include "Server/EventLog/StealthRoundEventLog.h"

#include "Server/StealthClock.h"
#include "Server/StealthFaultSink.h"

namespace
{
	const FName GRoundLogFaultSource(TEXT("RoundEventLog"));

	/** Round's public phase value for Active (ADR-0002 section 8.10: Lobby..Reset = 1..5). */
	constexpr uint32 PhaseValueActive = 3;

	int64 ToMilliseconds(double Seconds) { return FMath::RoundToInt64(Seconds * 1000.0); }

#define STEALTH_ROUND_LOG_HEADER_NAME(Type, Name, Default) TEXT(#Name),
	const TCHAR* const GRoundLogHeaderFieldNames[] = { STEALTH_ROUND_LOG_HEADER_FIELDS(STEALTH_ROUND_LOG_HEADER_NAME) };
#undef STEALTH_ROUND_LOG_HEADER_NAME

	// --- Disk writer rendering (the store's only text conversion; ADR-0007 section 6) ------------------------------

	void Render(FString& Out, uint64 Value) { Out.Appendf(TEXT("%llu"), Value); }
	void Render(FString& Out, uint32 Value) { Out.Appendf(TEXT("%u"), Value); }
	void Render(FString& Out, int32 Value) { Out.Appendf(TEXT("%d"), Value); }
	void Render(FString& Out, uint16 Value) { Out.Appendf(TEXT("%u"), static_cast<uint32>(Value)); }
	void Render(FString& Out, uint8 Value) { Out.Appendf(TEXT("%u"), static_cast<uint32>(Value)); }
	void Render(FString& Out, bool Value) { Out += Value ? TEXT("1") : TEXT("0"); }
	void Render(FString& Out, const FName& Value) { Out += Value.ToString(); }
	template <typename T>
	void Render(FString& Out, const TArray<T>& Values)
	{
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if (Index > 0)
			{
				Out += TEXT(";");
			}
			Render(Out, Values[Index]);
		}
	}
	void Render(FString& Out, const FRoundLogTeamRosters& Rosters)
	{
		Render(Out, Rosters.Team[0]);
		Out += TEXT("|");
		Render(Out, Rosters.Team[1]);
	}
}

TConstArrayView<const TCHAR*> FRoundLogHeader::GetFieldNames()
{
	return TConstArrayView<const TCHAR*>(GRoundLogHeaderFieldNames, UE_ARRAY_COUNT(GRoundLogHeaderFieldNames));
}

FRoundEventLog::FRoundEventLog(const IRoundLogAccessGate& InGate, const IStealthClock& InClock, IStealthFaultSink& InFaultSink)
	: Gate(InGate)
	, Clock(InClock)
	, FaultSink(InFaultSink)
{
	BeginRound(MaxEntriesDefault);
}

void FRoundEventLog::BeginRound(int32 ConfiguredMaxEntries)
{
	Capacity = ClampMaxEntries(ConfiguredMaxEntries);
	Header = FRoundLogHeader();
	bHeaderWritten = false;
	Slots.Reset();
	Slots.Reserve(Capacity);
	FreeSlots.Reset();
	FreeSlots.Reserve(Capacity);
	OrdinaryFifo.SetNumZeroed(Capacity);
	OrdinaryHead = 0;
	OrdinaryCount = 0;
	LiveCount = 0;
	NextInsertSeq = 1;
	FMemory::Memzero(TruncatedCounts, sizeof(TruncatedCounts));
	ActivePhaseStart.Reset();
	VoidedBudgets.Reset();
	TotalSuppressedVoided = 0;
}

bool FRoundEventLog::WriteHeader(const FRoundLogHeader& InHeader)
{
	if (bHeaderWritten)
	{
		FaultSink.Report(EStealthFault::RoundLogMisuse, GRoundLogFaultSource, static_cast<int64>(EStealthRoundLogMisuse::HeaderWrittenTwice));
		return false;
	}
	Header = InHeader;
	bHeaderWritten = true;
	return true;
}

void FRoundEventLog::Append(const FRoundLogEntry& Entry)
{
	// R19: Voided entries, one per source per second (compared in integer milliseconds); the rest counted, never
	// logged. Only Voided is rate-limited; protected types never are. The budget is spent only when the entry is
	// actually stored.
	FVoidedBudget* Budget = nullptr;
	int64 NowMs = 0;
	if (Entry.Type == ERoundLogType::Voided)
	{
		NowMs = ToMilliseconds(Clock.GetServerTimeSeconds());
		Budget = VoidedBudgets.FindByPredicate([&Entry](const FVoidedBudget& B) { return B.Source == Entry.SourceMailbox.Value; });
		if (Budget != nullptr && Budget->bWritten && NowMs - Budget->LastWrittenMs < ToMilliseconds(VoidedMinSpacingSec))
		{
			++Budget->Suppressed;
			++TotalSuppressedVoided;
			return;
		}
		if (Budget == nullptr)
		{
			Budget = &VoidedBudgets.AddDefaulted_GetRef();
			Budget->Source = Entry.SourceMailbox.Value;
		}
	}

	// The Active-entry time is kept by the store itself: the PhaseChanged row is not protected and may be evicted,
	// but F7's DurationSec needs it (rmf-016).
	if (Entry.Type == ERoundLogType::PhaseChanged && Entry.Data[1] == PhaseValueActive && !ActivePhaseStart.IsSet())
	{
		ActivePhaseStart = Entry.ServerTime;
	}

	const bool bProtected = IsProtectedRoundLogType(Entry.Type);
	if (LiveCount >= Capacity)
	{
		if (OrdinaryCount > 0)
		{
			// Evict the oldest non-protected entry.
			const int32 Evicted = OrdinaryFifo[OrdinaryHead];
			OrdinaryHead = (OrdinaryHead + 1) % Capacity;
			--OrdinaryCount;
			FSlot& Slot = Slots[Evicted];
			++TruncatedCounts[static_cast<int32>(Slot.Entry.Type)];
			Slot.bLive = false;
			FreeSlots.Add(Evicted);
			--LiveCount;
		}
		else
		{
			// Every slot holds a protected entry: nothing may be evicted and the ring never grows.
			++TruncatedCounts[static_cast<int32>(Entry.Type)];
			if (bProtected)
			{
				FaultSink.Report(EStealthFault::RoundLogMisuse, GRoundLogFaultSource, static_cast<int64>(EStealthRoundLogMisuse::RingFullOfProtectedEntries));
			}
			return;
		}
	}

	int32 SlotIndex;
	if (FreeSlots.Num() > 0)
	{
		SlotIndex = FreeSlots.Pop(EAllowShrinking::No);
	}
	else
	{
		SlotIndex = Slots.AddDefaulted();
	}
	FSlot& Slot = Slots[SlotIndex];
	Slot.Entry = Entry;
	Slot.InsertSeq = NextInsertSeq++;
	Slot.bLive = true;
	++LiveCount;
	if (Budget != nullptr)
	{
		Budget->bWritten = true;
		Budget->LastWrittenMs = NowMs;
	}
	if (!bProtected)
	{
		OrdinaryFifo[(OrdinaryHead + OrdinaryCount) % Capacity] = SlotIndex;
		++OrdinaryCount;
	}
}

void FRoundEventLog::CollectLive(TArray<const FSlot*>& Out) const
{
	Out.Reset(LiveCount);
	for (const FSlot& Slot : Slots)
	{
		if (Slot.bLive)
		{
			Out.Add(&Slot);
		}
	}
	Out.Sort([](const FSlot& A, const FSlot& B) { return A.InsertSeq < B.InsertSeq; });
}

ERoundLogReadResult FRoundEventLog::ReadHeader(FRoundLogHeader& OutHeader) const
{
	if (!Gate.IsLogReleasable())
	{
		return ERoundLogReadResult::Refused_NotReleasable;
	}
	if (!bHeaderWritten)
	{
		return ERoundLogReadResult::NoHeader;
	}
	OutHeader = Header;
	return ERoundLogReadResult::Ok;
}

ERoundLogReadResult FRoundEventLog::ReadEntries(TArray<FRoundLogEntry>& OutEntries) const
{
	OutEntries.Reset();
	if (!Gate.IsLogReleasable())
	{
		return ERoundLogReadResult::Refused_NotReleasable;
	}
	ForEachEntryServerInternal([&OutEntries](const FRoundLogEntry& Entry) { OutEntries.Add(Entry); });
	return ERoundLogReadResult::Ok;
}

ERoundLogReadResult FRoundEventLog::ReadTruncatedCounts(TArray<uint32>& OutPerType) const
{
	OutPerType.Reset();
	if (!Gate.IsLogReleasable())
	{
		return ERoundLogReadResult::Refused_NotReleasable;
	}
	OutPerType.Append(TruncatedCounts, GRoundLogTypeCount);
	return ERoundLogReadResult::Ok;
}

void FRoundEventLog::ForEachEntryServerInternal(TFunctionRef<void(const FRoundLogEntry&)> Visit) const
{
	TArray<const FSlot*> Live;
	CollectLive(Live);
	for (const FSlot* Slot : Live)
	{
		Visit(Slot->Entry);
	}
}

FRoundLogTeamTallies FRoundEventLog::TallyTeamSummaryInputsServerInternal() const
{
	FRoundLogTeamTallies Tallies;
	ForEachEntryServerInternal([&Tallies](const FRoundLogEntry& Entry)
	{
		switch (Entry.Type)
		{
		case ERoundLogType::ObjectiveStage:
		case ERoundLogType::FinalCompleted:
			if (Entry.Data[0] < 2)
			{
				++Tallies.StepsCompleted[Entry.Data[0]];
			}
			break;
		case ERoundLogType::StateTransition:
			// D0 old state, D1 new state, D2 team (EParticipantState values: Active 0, Informant 1).
			if (Entry.Data[0] == 0 && Entry.Data[1] == 1 && Entry.Data[2] < 2)
			{
				++Tallies.EliminatedCount[Entry.Data[2]];
			}
			break;
		case ERoundLogType::KillResolved:
			if (Entry.Code == static_cast<uint16>(ERoundKillResult::WrongTarget) && Entry.Data[0] < 2)
			{
				++Tallies.WrongTargetCount[Entry.Data[0]];
			}
			break;
		default:
			break;
		}
	});
	return Tallies;
}

bool FRoundEventLog::WriteToDisk(IRoundLogFileSink& Sink, const FString& FileName) const
{
	if (!Gate.IsLogReleasable())
	{
		return false;  // never mid-round: only on entering Ended or Reset, or at the SessionEnded latch
	}
	FString Text;
	Text.Reserve(1024 + LiveCount * 96);
	Text += TEXT("[Header]\n");
	if (bHeaderWritten)
	{
#define STEALTH_ROUND_LOG_HEADER_RENDER(Type, Name, Default) Text += TEXT(#Name); Text += TEXT("="); Render(Text, Header.Name); Text += TEXT("\n");
		STEALTH_ROUND_LOG_HEADER_FIELDS(STEALTH_ROUND_LOG_HEADER_RENDER)
#undef STEALTH_ROUND_LOG_HEADER_RENDER
	}
	Text += TEXT("[Truncated]\n");
	for (int32 Type = 0; Type < GRoundLogTypeCount; ++Type)
	{
		if (TruncatedCounts[Type] > 0)
		{
			Text.Appendf(TEXT("%d=%u\n"), Type, TruncatedCounts[Type]);
		}
	}
	Text += TEXT("[Entries] tick,seq,time,epoch,type,flags,code,source,p0,p1,d0,d1,d2,d3,d4,d5\n");
	ForEachEntryServerInternal([&Text](const FRoundLogEntry& E)
	{
		Text.Appendf(TEXT("%llu,%llu,%.6f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n"),
			E.Key.ServerTickIndex, E.Key.Seq, E.ServerTime, E.RoundEpoch, static_cast<uint32>(E.Type), static_cast<uint32>(E.Flags),
			static_cast<uint32>(E.Code), E.SourceMailbox.Value, E.Participants[0], E.Participants[1],
			E.Data[0], E.Data[1], E.Data[2], E.Data[3], E.Data[4], E.Data[5]);
	});
	return Sink.WriteFile(FileName, Text);
}

uint64 FRoundEventLog::GetTotalTruncatedCount() const
{
	uint64 Total = 0;
	for (const uint32 Count : TruncatedCounts)
	{
		Total += Count;
	}
	return Total;
}

uint32 FRoundEventLog::GetSuppressedVoidedCount(FMailboxId Source) const
{
	const FVoidedBudget* Budget = VoidedBudgets.FindByPredicate([Source](const FVoidedBudget& B) { return B.Source == Source.Value; });
	return Budget != nullptr ? Budget->Suppressed : 0;
}

ERoundLogReleaseClass FRoundEventLog::GetReleaseClass(ERoundLogType Type)
{
	switch (Type)
	{
	case ERoundLogType::KillResolved:
	case ERoundLogType::Reversal:
	case ERoundLogType::StateTransition:
	case ERoundLogType::StrikeLimit:
	case ERoundLogType::ObjectiveStage:
	case ERoundLogType::FinalCompleted:
	case ERoundLogType::WinLatched:
	case ERoundLogType::PhaseChanged:
	// Penalty's rows (GDD npc-kill-penalty-informants.md PEN-R17: all PostRoundEligible; story nkp-004).
	case ERoundLogType::StrikeCounted:
	case ERoundLogType::RespawnScheduled:
	case ERoundLogType::InformantRespawn:
	case ERoundLogType::RespawnCancelled:
	case ERoundLogType::PawnLost:
		return ERoundLogReleaseClass::PostRoundEligible;
	default:
		// Mailbox order, connection data, voids, debug and the rest: never released.
		return ERoundLogReleaseClass::NeverReleased;
	}
}
