#include "Server/Queue/StealthResolutionQueueCore.h"

#include "Algo/BinarySearch.h"
#include "Algo/StableSort.h"
#include "Server/EventLog/StealthRoundLogWriter.h"
#include "Server/Queue/StealthEventFlushBarrier.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Server/Queue/StealthStagedWriteApplier.h"
#include "Server/Random/StealthSecretStream.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/StealthClock.h"
#include "Server/StealthFaultSink.h"
#include "View/Random/StealthRangedDraw.h"

static_assert(static_cast<int32>(EQueueItemType::DebugItem) + 1 == GQueueItemTypeCount, "GQueueItemTypeCount out of date");
static_assert(static_cast<int32>(ETimerKind::EndedAutoReset) + 1 == GTimerKindCount, "GTimerKindCount out of date");
static_assert(static_cast<int32>(ERoundLogType::CascadeOverflow) + 1 == GRoundLogTypeCount, "GRoundLogTypeCount out of date");

namespace
{
	const FName GQueueFaultSource(TEXT("ResolutionQueue"));

	/** Server time in whole microseconds (the due rule's comparison unit). */
	int64 ToMicroseconds(double Seconds) { return FMath::RoundToInt64(Seconds * 1000000.0); }

	/** Fault Detail for a TimerFired kind: 0x100 + kind, so it never collides with an item type. */
	constexpr int64 TimerKindDetail(ETimerKind Kind) { return 0x100 + static_cast<int64>(Kind); }
}

FResolutionQueueCore::FResolutionQueueCore(const FResolutionQueueDeps& InDeps)
	: Deps(InDeps)
{
	check(Deps.Clock != nullptr && Deps.SubSeeds != nullptr && Deps.Epoch != nullptr && Deps.Log != nullptr && Deps.FaultSink != nullptr);
	Stamped.Reserve(64);
	RecentKeys.SetNum(RecentKeyCount);
	DrainRecords.Reserve(64);
}

// ---------------------------------------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------------------------------------

bool FResolutionQueueCore::RegisterResolver(EQueueItemType Type, IQueueResolver* Resolver)
{
	if (Resolver == nullptr)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::NullRegistration));
		return false;
	}
	IQueueResolver*& Slot_ = TypeResolvers[static_cast<int32>(Type)];
	if (Slot_ != nullptr)
	{
		ReportFault(EStealthFault::QueueDuplicateRegistration, static_cast<int64>(Type));
		return false;
	}
	Slot_ = Resolver;
	return true;
}

bool FResolutionQueueCore::RegisterTimerResolver(ETimerKind Kind, IQueueResolver* Resolver)
{
	if (Resolver == nullptr)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::NullRegistration));
		return false;
	}
	IQueueResolver*& Slot_ = TimerResolvers[static_cast<int32>(Kind)];
	if (Slot_ != nullptr)
	{
		ReportFault(EStealthFault::QueueDuplicateRegistration, TimerKindDetail(Kind));
		return false;
	}
	Slot_ = Resolver;
	return true;
}

void FResolutionQueueCore::UnregisterResolver(EQueueItemType Type)
{
	TypeResolvers[static_cast<int32>(Type)] = nullptr;
}

void FResolutionQueueCore::UnregisterTimerResolver(ETimerKind Kind)
{
	TimerResolvers[static_cast<int32>(Kind)] = nullptr;
}

void FResolutionQueueCore::ClearRegistrations()
{
	for (IQueueResolver*& Resolver : TypeResolvers)
	{
		Resolver = nullptr;
	}
	for (IQueueResolver*& Resolver : TimerResolvers)
	{
		Resolver = nullptr;
	}
	for (IStagedWriteApplier*& Applier : Appliers)
	{
		Applier = nullptr;
	}
	FlushBarrier = nullptr;
	TerminalDecision = nullptr;
}

bool FResolutionQueueCore::RegisterStagedWriteApplier(EStagedWriteType Type, IStagedWriteApplier* Applier)
{
	if (Type == EStagedWriteType::RoundEndStep)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::RoundEndApplierRegistration));
		return false;
	}
	if (Applier == nullptr)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::NullRegistration));
		return false;
	}
	IStagedWriteApplier*& Slot_ = Appliers[static_cast<int32>(Type)];
	if (Slot_ != nullptr)
	{
		ReportFault(EStealthFault::QueueDuplicateRegistration, 0x200 + static_cast<int64>(Type));
		return false;
	}
	Slot_ = Applier;
	return true;
}

void FResolutionQueueCore::UnregisterStagedWriteApplier(EStagedWriteType Type)
{
	Appliers[static_cast<int32>(Type)] = nullptr;
}

IQueueResolver* FResolutionQueueCore::FindResolver(const FQueueItem& Item) const
{
	if (Item.Type == EQueueItemType::TimerFired)
	{
		if (const FTimerPayload* Timer = Item.Payload.TryGet<FTimerPayload>())
		{
			return TimerResolvers[static_cast<int32>(Timer->Kind)];
		}
		return nullptr;
	}
	return TypeResolvers[static_cast<int32>(Item.Type)];
}

int64 FResolutionQueueCore::RegistrationDetail(const FQueueItem& Item)
{
	if (Item.Type == EQueueItemType::TimerFired)
	{
		if (const FTimerPayload* Timer = Item.Payload.TryGet<FTimerPayload>())
		{
			return TimerKindDetail(Timer->Kind);
		}
	}
	return static_cast<int64>(Item.Type);
}

// ---------------------------------------------------------------------------------------------------------
// Intake
// ---------------------------------------------------------------------------------------------------------

FQueueItemHandle FResolutionQueueCore::Submit(FMailboxId Source, const FQueueItem& Item)
{
	return SubmitInternal(Source, Item, false, 0);
}

FQueueItemHandle FResolutionQueueCore::Submit(FMailboxId Source, const FQueueItem& Item, uint32 ConnectionGeneration)
{
	return SubmitInternal(Source, Item, true, ConnectionGeneration);
}

FQueueItemHandle FResolutionQueueCore::SubmitInternal(FMailboxId Source, const FQueueItem& Item, bool bHasGeneration, uint32 ConnectionGeneration)
{
	if (!Source.IsValid())
	{
		// An unset mailbox would sort after the server-local one and break the canonical base order.
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::InvalidMailbox));
		return FQueueItemHandle();
	}
	FPendingItem Pending;
	Pending.Item = Item;
	Pending.Item.Source = Source;
	Pending.HandleId = NextHandleId++;
	Pending.bHasGeneration = bHasGeneration;
	Pending.ConnectionGeneration = ConnectionGeneration;
	// A submit from inside a slot is a deferred outcome on the generic path (R14.3): it records its cause.
	Pending.CauseKey = CurrentKey();

	const int32 Index = Algo::LowerBoundBy(Mailboxes, Source.Value, [](const FMailbox& Mailbox) { return Mailbox.Id.Value; });
	if (!Mailboxes.IsValidIndex(Index) || Mailboxes[Index].Id != Source)
	{
		FMailbox NewMailbox;
		NewMailbox.Id = Source;
		Mailboxes.Insert(MoveTemp(NewMailbox), Index);
	}
	Mailboxes[Index].Items.Add(MoveTemp(Pending));
	return FQueueItemHandle{ Mailboxes[Index].Items.Last().HandleId };
}

FQueueItemHandle FResolutionQueueCore::ScheduleAt(double DueServerTime, const FQueueItem& Item)
{
	FDueEntry Entry;
	Entry.DueTime = DueServerTime;
	Entry.ScheduleOrder = NextScheduleOrder++;
	Entry.Pending.Item = Item;
	Entry.Pending.HandleId = NextHandleId++;
	Entry.Pending.CauseKey = CurrentKey();
	const uint64 HandleId = Entry.Pending.HandleId;
	DueHeap.HeapPush(MoveTemp(Entry), [](const FDueEntry& A, const FDueEntry& B)
	{
		return A.DueTime < B.DueTime || (A.DueTime == B.DueTime && A.ScheduleOrder < B.ScheduleOrder);
	});
	return FQueueItemHandle{ HandleId };
}

int32 FResolutionQueueCore::GetPendingMailboxItemCount() const
{
	int32 Count = 0;
	for (const FMailbox& Mailbox : Mailboxes)
	{
		Count += Mailbox.Items.Num();
	}
	return Count;
}

void FResolutionQueueCore::GetPendingTimerKinds(TArray<ETimerKind>& OutKinds) const
{
	OutKinds.Reset();
	for (const FDueEntry& Entry : DueHeap)
	{
		const FTimerPayload* Timer = Entry.Pending.Item.Type == EQueueItemType::TimerFired ? Entry.Pending.Item.Payload.TryGet<FTimerPayload>() : nullptr;
		if (Timer != nullptr)
		{
			OutKinds.Add(Timer->Kind);
		}
	}
}

TOptional<FQueueKey> FResolutionQueueCore::GetKey(FQueueItemHandle Handle) const
{
	const FRecentKey& Recent = RecentKeys[static_cast<int32>(Handle.Id % RecentKeyCount)];
	if (Handle.Id != 0 && Recent.HandleId == Handle.Id)
	{
		return Recent.Key;
	}
	return {};
}

// ---------------------------------------------------------------------------------------------------------
// Drain
// ---------------------------------------------------------------------------------------------------------

void FResolutionQueueCore::Drain()
{
	if (bDraining)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::ReentrantDrain));
		return;
	}
	bDraining = true;
	DrainTick = Deps.Clock->GetServerTickIndex();
	DrainStartTime = Deps.Clock->GetServerTimeSeconds();
	DrainRecords.Reset();
	LastDrainOverflowCount = 0;
	Stamped.Reset();

	// Group 1: items already due at the drain start, before this frame's intake.
	StampDueItems();
	// Group 2: this frame's intake.
	StampIntake();

	// Run every stamped item in key order; group 3 (items that became due during the drain) is stamped after
	// intake each time the stamped list runs dry.
	int32 Next = 0;
	int32 InlineDuePasses = 0;
	for (;;)
	{
		while (Next < Stamped.Num())
		{
			const FStampedItem Current = Stamped[Next++];  // copy: a slot may append overflow items
			RunSlot(Current);
		}
		if (DueHeap.Num() > 0 && InlineDuePasses >= MaxInlineDuePasses)
		{
			// A resolver that keeps scheduling items due now is a defect: report it; the rest wait for the next drain.
			ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::InlineDueLoop));
			break;
		}
		++InlineDuePasses;
		if (!StampDueItems())
		{
			break;
		}
	}

	Stamped.Reset();
	bDraining = false;

	// 6. The staging pass, in the same tick function immediately after the drain (ADR-0002 section 6).
	RunStagingPass();
}

FQueueKey FResolutionQueueCore::StampNext(const FPendingItem& Pending, const FQueueKey* RootCause)
{
	FStampedItem& Item = Stamped.AddDefaulted_GetRef();
	Item.Key = FQueueKey(DrainTick, NextSeq++);
	Item.Pending = Pending;
	Item.RootCauseKey = RootCause != nullptr ? *RootCause : Item.Key;
	if (Pending.HandleId != 0)
	{
		FRecentKey& Recent = RecentKeys[static_cast<int32>(Pending.HandleId % RecentKeyCount)];
		Recent.HandleId = Pending.HandleId;
		Recent.Key = Item.Key;
	}
	return Item.Key;
}

uint32 FResolutionQueueCore::GetTieParticipantValue(const FQueueItem& Item)
{
	if (const FTimerPayload* Timer = Item.Payload.TryGet<FTimerPayload>())
	{
		if (Timer->Subject.Kind == ESubjectRefKind::Participant && Timer->Subject.Participant.IsValid())
		{
			return Timer->Subject.Participant.Value;
		}
	}
	if (Item.Source.IsValid() && !Item.Source.IsServerLocal())
	{
		return Item.Source.Value;
	}
	return FParticipantId::InvalidValue;
}

uint64 FResolutionQueueCore::SubSeedOrFault(ESeedDomain Domain) const
{
	const uint64 SubSeed = Deps.SubSeeds->GetServerSubSeed(Domain, static_cast<uint32>(DrainTick));
	if (SubSeed == 0)
	{
		// No secret held: the permutation would follow a publicly known stream.
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::NoServerSecret));
	}
	return SubSeed;
}

bool FResolutionQueueCore::StampDueItems()
{
	// R12 due rule in integer microseconds, so a floating-point ulp cannot move the inclusive 1 ms bound.
	const int64 DueLimitUs = ToMicroseconds(DrainStartTime) + ToMicroseconds(DueEpsilonSec);
	TArray<FDueEntry, TInlineAllocator<16>> Due;
	const auto HeapOrder = [](const FDueEntry& A, const FDueEntry& B)
	{
		return A.DueTime < B.DueTime || (A.DueTime == B.DueTime && A.ScheduleOrder < B.ScheduleOrder);
	};
	while (DueHeap.Num() > 0 && ToMicroseconds(DueHeap.HeapTop().DueTime) <= DueLimitUs)
	{
		FDueEntry Entry;
		DueHeap.HeapPop(Entry, HeapOrder, EAllowShrinking::No);
		Due.Add(MoveTemp(Entry));
	}
	if (Due.Num() == 0)
	{
		return false;
	}

	// TieRank: a per-tick server-secret shuffle of the participant ids present (R12), never the id itself.
	TArray<uint32, TInlineAllocator<16>> TieParticipants;
	for (const FDueEntry& Entry : Due)
	{
		const uint32 Value = GetTieParticipantValue(Entry.Pending.Item);
		if (Value != FParticipantId::InvalidValue)
		{
			TieParticipants.AddUnique(Value);
		}
	}
	TieParticipants.Sort();
	if (TieParticipants.Num() > 1)
	{
		FSecretStream Stream(SubSeedOrFault(ESeedDomain::TieRank), TieRankStreamTag);
		StealthRandom::FisherYates(Stream, TArrayView<uint32>(TieParticipants.GetData(), TieParticipants.Num()));
	}
	const auto TieRankOf = [&TieParticipants](const FQueueItem& Item) -> int32
	{
		const uint32 Value = GetTieParticipantValue(Item);
		const int32 Rank = Value == FParticipantId::InvalidValue ? INDEX_NONE : TieParticipants.IndexOfByKey(Value);
		return Rank == INDEX_NONE ? MAX_int32 : Rank;
	};

	Algo::StableSort(Due, [&TieRankOf](const FDueEntry& A, const FDueEntry& B)
	{
		if (A.DueTime != B.DueTime)
		{
			return A.DueTime < B.DueTime;
		}
		// A strict weak order: at equal DueTime, holds first, ordered among themselves by the key of their Begin
		// command (IIF R8); every other item by TieRank; schedule order last.
		const FHoldTimerPayload* HoldA = A.Pending.Item.Type == EQueueItemType::HoldTimer ? A.Pending.Item.Payload.TryGet<FHoldTimerPayload>() : nullptr;
		const FHoldTimerPayload* HoldB = B.Pending.Item.Type == EQueueItemType::HoldTimer ? B.Pending.Item.Payload.TryGet<FHoldTimerPayload>() : nullptr;
		if ((HoldA != nullptr) != (HoldB != nullptr))
		{
			return HoldA != nullptr;
		}
		if (HoldA != nullptr)
		{
			if (HoldA->BeginKey != HoldB->BeginKey)
			{
				return HoldA->BeginKey < HoldB->BeginKey;
			}
			return A.ScheduleOrder < B.ScheduleOrder;
		}
		const int32 RankA = TieRankOf(A.Pending.Item);
		const int32 RankB = TieRankOf(B.Pending.Item);
		if (RankA != RankB)
		{
			return RankA < RankB;
		}
		return A.ScheduleOrder < B.ScheduleOrder;
	});

	for (const FDueEntry& Entry : Due)
	{
		StampNext(Entry.Pending, nullptr);
	}
	return true;
}

void FResolutionQueueCore::StampIntake()
{
	TArray<int32, TInlineAllocator<16>> Order;
	for (int32 Index = 0; Index < Mailboxes.Num(); ++Index)
	{
		if (Mailboxes[Index].Items.Num() > 0)
		{
			Order.Add(Index);  // Mailboxes is kept sorted: this is the canonical base order
		}
	}
	if (Order.Num() > 1)
	{
		FSecretStream Stream(SubSeedOrFault(ESeedDomain::SeqOrder), SeqOrderStreamTag);
		StealthRandom::FisherYates(Stream, TArrayView<int32>(Order.GetData(), Order.Num()));
	}

	for (const int32 MailboxIndex : Order)
	{
		FMailbox& Mailbox = Mailboxes[MailboxIndex];
		const int32 Count = Mailbox.Items.Num();
		const int32 Taken = FMath::Min(Count, MailboxMaxItemsPerDrain);
		for (int32 ItemIndex = 0; ItemIndex < Taken; ++ItemIndex)
		{
			StampNext(Mailbox.Items[ItemIndex], nullptr);
		}
		const int32 Dropped = Count - Taken;
		if (Dropped > 0)
		{
			LastDrainOverflowCount += Dropped;
			TotalOverflowCount += static_cast<uint64>(Dropped);
			FRoundLogEntry Entry;
			Entry.Type = ERoundLogType::MailboxOverflow;
			Entry.Data[0] = static_cast<uint32>(Dropped);
			// Intake precedes every slot of this tick: Seq 0 orders the entry before them.
			AppendEntry(Entry, FQueueKey(DrainTick, 0), Mailbox.Id);
		}
		Mailbox.Items.Reset();
	}
}

bool FResolutionQueueCore::IsPostLatchAllowed(const FQueueItem& Item)
{
	if (Item.Type != EQueueItemType::TimerFired)
	{
		return false;
	}
	const FTimerPayload* Timer = Item.Payload.TryGet<FTimerPayload>();
	return Timer != nullptr
		&& (Timer->Kind == ETimerKind::EndedMinDwell || Timer->Kind == ETimerKind::EndedAutoReset || Timer->Kind == ETimerKind::ConcealmentTransition);
}

void FResolutionQueueCore::EnterSlot(const FStampedItem& Current)
{
	bInSlot = true;
	FollowOnDepth = 0;
	Slot = FSlotContext();
	Slot.Key = Current.Key;
	Slot.RootCauseKey = Current.RootCauseKey;
	Slot.CauseKey = Current.Pending.CauseKey;
	Slot.Source = Current.Pending.Item.Source;
}

void FResolutionQueueCore::EndSlot()
{
	// Slot end: after Resolve and FlushPending() have both returned (ADR-0002 section 5).
	if (FlushBarrier != nullptr)
	{
		FlushBarrier->FlushPending();
		if (FlushBarrier->HasPending())
		{
			ReportFault(EStealthFault::QueueFlushIncomplete, static_cast<int64>(Slot.Key.Seq));
		}
	}
	const int32 RoundEndIndex = Slot.RoundEndIndex;
	bInSlot = false;
	FollowOnDepth = 0;
	if (RoundEndIndex != INDEX_NONE)
	{
		BindRoundEndStep(RoundEndIndex);
	}
}

bool FResolutionQueueCore::StageWrite(const FStagedWrite& Write)
{
	if (!bInSlot)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::StageWriteOutsideSlot));
		return false;
	}
	FStagedWrite& Staged = StagedWrites.Add_GetRef(Write);
	Staged.CauseKey = Slot.RootCauseKey;
	Staged.Expected = Deps.Epoch->GetCurrentEpoch();
	if (Write.Type == EStagedWriteType::RoundEndStep)
	{
		if (Slot.RoundEndIndex != INDEX_NONE)
		{
			// One RoundEndStep per deciding slot (section 8.3): a second one is a defect and is dropped.
			StagedWrites.Pop();
			ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::RoundEndFallbackBinding));
			return false;
		}
		Slot.RoundEndIndex = StagedWrites.Num() - 1;
	}
	return true;
}

void FResolutionQueueCore::BindRoundEndStep(int32 Index)
{
	if (!StagedWrites.IsValidIndex(Index) || StagedWrites[Index].Type != EStagedWriteType::RoundEndStep)
	{
		return;
	}
	FStagedWrite& RoundEnd = StagedWrites[Index];
	const FRoundEndPayload* Payload = RoundEnd.Payload.TryGet<FRoundEndPayload>();
	// (1) A strike decided the round: the latest DeathWrite this slot staged (bound by CauseKey, not by time equality).
	double Latest = -1.0;
	for (const FStagedWrite& Write : StagedWrites)
	{
		if (Write.Type == EStagedWriteType::DeathWrite && Write.CauseKey == RoundEnd.CauseKey)
		{
			Latest = FMath::Max(Latest, Write.ServerTime);
		}
	}
	if (Latest >= 0.0)
	{
		RoundEnd.ServerTime = Latest;
		return;
	}
	// (2) A strike reason without a death write of the slot (only after a CascadeOverflow): the fallback, in every build.
	if (Payload != nullptr && Payload->bStrikeDecided)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::RoundEndFallbackBinding));
		double VictimLatest = -1.0;
		if (Payload->DecidingVictim.Pawn.IsValid())
		{
			for (const FStagedWrite& Write : StagedWrites)
			{
				const FDeathWritePayload* Death = Write.Type == EStagedWriteType::DeathWrite ? Write.Payload.TryGet<FDeathWritePayload>() : nullptr;
				if (Death != nullptr && Death->Victim == Payload->DecidingVictim)
				{
					VictimLatest = FMath::Max(VictimLatest, Write.ServerTime);
				}
			}
		}
		RoundEnd.ServerTime = VictimLatest >= 0.0 ? VictimLatest : DrainStartTime + Payload->FallbackDeferSec;
		return;
	}
	// (3) Objective win, forfeit, debug win: the round-end step is the latch step, consumed inline at slot end.
	const FStagedWrite Inline = RoundEnd;
	StagedWrites.RemoveAt(Index);
	ApplyStaged(Inline);
}

void FResolutionQueueCore::RunStagingPass()
{
	if (bInSlot)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::StageWriteOutsideSlot));
		return;
	}
	if (StagedWrites.Num() == 0)
	{
		return;
	}
	const int64 LimitUs = ToMicroseconds(Deps.Clock->GetServerTimeSeconds()) + ToMicroseconds(DueEpsilonSec);
	TArray<FStagedWrite, TInlineAllocator<8>> Due;
	for (int32 Index = 0; Index < StagedWrites.Num();)
	{
		if (ToMicroseconds(StagedWrites[Index].ServerTime) <= LimitUs)
		{
			Due.Add(StagedWrites[Index]);
			StagedWrites.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}
	Algo::StableSort(Due, [](const FStagedWrite& A, const FStagedWrite& B)
	{
		if (A.ServerTime != B.ServerTime)
		{
			return A.ServerTime < B.ServerTime;
		}
		if (A.CauseKey != B.CauseKey)
		{
			return A.CauseKey < B.CauseKey;
		}
		return static_cast<uint8>(A.Type) < static_cast<uint8>(B.Type);
	});
	for (const FStagedWrite& Write : Due)
	{
		// Dropped on Reset by epoch mismatch (ADR-0002 section 6), re-read per write: an applied write may reset the round.
		if (Write.Expected == Deps.Epoch->GetCurrentEpoch())
		{
			ApplyStaged(Write);
		}
	}
}

void FResolutionQueueCore::ApplyStaged(const FStagedWrite& Write)
{
	if (Write.Type == EStagedWriteType::RoundEndStep)
	{
		const FRoundEndPayload* Payload = Write.Payload.TryGet<FRoundEndPayload>();
		if (TerminalDecision == nullptr || Payload == nullptr)
		{
			ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::StagedWriteNoApplier));
			return;
		}
		TerminalDecision->RunRoundEndStep(*Payload);
	}
	else
	{
		IStagedWriteApplier* Applier = Appliers[static_cast<int32>(Write.Type)];
		if (Applier == nullptr)
		{
			ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::StagedWriteNoApplier));
			return;
		}
		Applier->ApplyStagedWrite(Write);
	}
	// The barrier runs after every applied write that may mutate the Roster (EndRoundAll raises batch events).
	if (FlushBarrier != nullptr)
	{
		FlushBarrier->FlushPending();
	}
}

void FResolutionQueueCore::RunSlot(const FStampedItem& Current)
{
	const FQueueItem& Item = Current.Pending.Item;
	FQueueSlotRecord& Record = DrainRecords.AddDefaulted_GetRef();
	Record.Key = Current.Key;
	Record.Type = Item.Type;
	Record.Source = Item.Source;
	Record.RootCauseKey = Current.RootCauseKey;
	Record.CauseKey = Current.Pending.CauseKey;

	// 1. Epoch (before the connection check: a stale-epoch item from a stale connection reports StaleEpoch).
	if (Item.Expected != Deps.Epoch->GetCurrentEpoch())
	{
		Record.Outcome = EQueueSlotOutcome::Rejected_StaleEpoch;
		LogVoided(Current, EVoidReason::Rejected_StaleEpoch, 0);
		return;
	}

	// 2. Connection generation (an item from a superseded connection).
	if (Current.Pending.bHasGeneration && Deps.Generations != nullptr && Item.Source.IsValid() && !Item.Source.IsServerLocal())
	{
		const uint32 CurrentGeneration = Deps.Generations->GetConnectionGeneration(Item.Source.GetParticipant());
		if (Current.Pending.ConnectionGeneration < CurrentGeneration)
		{
			Record.Outcome = EQueueSlotOutcome::Dropped_StaleConnection;
			++TotalStaleConnectionDrops;
			FRoundLogEntry Entry;
			Entry.Type = ERoundLogType::Dropped_StaleConnection;
			Entry.Participants[0] = Item.Source.Value;
			Entry.Data[0] = static_cast<uint32>(Item.Type);
			Entry.Data[1] = Current.Pending.ConnectionGeneration;
			Entry.Data[2] = CurrentGeneration;
			AppendEntry(Entry, Current.Key, Item.Source);
			return;
		}
	}

	// 3. Latch: the queue is closed except for parked host commands and the post-latch exemptions.
	if (bLatched && !IsPostLatchAllowed(Item))
	{
		if (Item.Type == EQueueItemType::HostCommand)
		{
			Record.Outcome = EQueueSlotOutcome::Parked;
			ParkedHostCommands.Add(Item);
			return;
		}
		Record.Outcome = EQueueSlotOutcome::Rejected_Latched;
		FRoundLogEntry Entry;
		Entry.Type = ERoundLogType::Rejected_Latched;
		Entry.Data[0] = static_cast<uint32>(Item.Type);
		AppendEntry(Entry, Current.Key, Item.Source);
		if (IQueueResolver* Owner = FindResolver(Item))
		{
			EnterSlot(Current);
			Owner->OnRejectedLatched(Item, Current.Key);
			EndSlot();
		}
		return;
	}

#if UE_BUILD_SHIPPING
	if (Item.Type == EQueueItemType::DebugItem)
	{
		Record.Outcome = EQueueSlotOutcome::DebugDroppedInShipping;
		ReportFault(EStealthFault::QueueNoResolver, static_cast<int64>(Item.Type));
		return;
	}
#endif

	IQueueResolver* Resolver = FindResolver(Item);
	if (Resolver == nullptr)
	{
		Record.Outcome = EQueueSlotOutcome::NoResolver;
		ReportFault(EStealthFault::QueueNoResolver, RegistrationDetail(Item));
		return;
	}

	EnterSlot(Current);
	const EResolveResult Result = Resolver->Resolve(Item, Current.Key);
	if (Result == EResolveResult::Voided)
	{
		LogVoided(Current, EVoidReason::Revalidation, Slot.VoidCode);
	}
	EndSlot();
	// Record stays valid: DrainRecords grows only in RunSlot, and a resolver cannot re-enter Drain.
	Record.Outcome = Result == EResolveResult::Voided ? EQueueSlotOutcome::Voided : EQueueSlotOutcome::Executed;
}

bool FResolutionQueueCore::ExecuteFollowOn(const FQueueItem& Work)
{
	if (!bInSlot)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::FollowOnOutsideSlot));
		return false;
	}
	IQueueResolver* Resolver = FindResolver(Work);
	if (Resolver == nullptr)
	{
		ReportFault(EStealthFault::QueueNoResolver, RegistrationDetail(Work));
		return false;
	}

	if (FollowOnDepth >= QueueCascadeDepthMax)
	{
		// A cascade deeper than the bound is a defect: in every build, log it once per slot and move the remaining
		// work out of the slot as items, in raised order, each carrying the originating slot's RootCauseKey.
		++Slot.OverflowCount;
		if (!Slot.bOverflowLogged)
		{
			Slot.bOverflowLogged = true;
			ReportFault(EStealthFault::QueueCascadeOverflow, static_cast<int64>(Slot.Key.Seq));
			FRoundLogEntry Entry;
			Entry.Type = ERoundLogType::CascadeOverflow;
			Entry.Data[0] = 1;
			AppendEntry(Entry, Slot.Key, Slot.Source);
		}
		FPendingItem Pending;
		Pending.Item = Work;
		Pending.CauseKey = Slot.Key;
		StampNext(Pending, &Slot.RootCauseKey);
		return false;
	}

	++FollowOnDepth;
	const EResolveResult Result = Resolver->Resolve(Work, Slot.Key);  // in-slot: the slot's key, no Seq consumed
	--FollowOnDepth;
	if (Result == EResolveResult::Voided)
	{
		FStampedItem AsStamped;
		AsStamped.Key = Slot.Key;
		AsStamped.Pending.Item = Work;
		LogVoided(AsStamped, EVoidReason::Revalidation, 0);
	}
	return true;
}

void FResolutionQueueCore::SetVoidCode(uint32 OwnerCode)
{
	if (!bInSlot)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::VoidCodeOutsideSlot));
		return;
	}
	Slot.VoidCode = OwnerCode;
}

// ---------------------------------------------------------------------------------------------------------
// Latch and lifecycle
// ---------------------------------------------------------------------------------------------------------

bool FResolutionQueueCore::Latch(FQueueKey InTerminalKey)
{
	if (bLatched)
	{
		return false;
	}
	bLatched = true;
	TerminalKey = InTerminalKey;
	return true;
}

TArray<FQueueItem> FResolutionQueueCore::TakeParkedHostCommands()
{
	TArray<FQueueItem> Parked = MoveTemp(ParkedHostCommands);
	ParkedHostCommands.Reset();
	return Parked;
}

void FResolutionQueueCore::ResetForNewRound()
{
	if (bDraining)
	{
		ReportFault(EStealthFault::QueueMisuse, static_cast<int64>(EStealthQueueMisuse::ResetDuringDrain));
		return;
	}
	NextSeq = 1;
	Mailboxes.Reset();
	DueHeap.Reset();
	for (FRecentKey& Recent : RecentKeys)
	{
		Recent = FRecentKey();
	}
	ParkedHostCommands.Reset();
	StagedWrites.Reset();
	DrainRecords.Reset();
	bLatched = false;
	TerminalKey = FQueueKey();
	LastDrainOverflowCount = 0;
	TotalOverflowCount = 0;
	TotalStaleConnectionDrops = 0;
}

// ---------------------------------------------------------------------------------------------------------
// Log and faults
// ---------------------------------------------------------------------------------------------------------

void FResolutionQueueCore::AppendEntry(FRoundLogEntry& Entry, const FQueueKey& Key, FMailboxId Source) const
{
	Entry.Key = Key;
	Entry.ServerTime = Deps.Clock->GetServerTimeSeconds();
	Entry.RoundEpoch = Deps.Epoch->GetCurrentEpoch().RoundEpoch;
	Entry.SourceMailbox = Source;
	Deps.Log->Append(Entry);
}

void FResolutionQueueCore::LogVoided(const FStampedItem& Current, EVoidReason Reason, uint32 OwnerCode) const
{
	FRoundLogEntry Entry;
	Entry.Type = ERoundLogType::Voided;
	Entry.Code = static_cast<uint16>(Reason);
	Entry.Data[0] = static_cast<uint32>(Current.Pending.Item.Type);
	Entry.Data[1] = OwnerCode;
	AppendEntry(Entry, Current.Key, Current.Pending.Item.Source);
}

void FResolutionQueueCore::ReportFault(EStealthFault Fault, int64 Detail) const
{
	Deps.FaultSink->Report(Fault, GQueueFaultSource, Detail);
}
