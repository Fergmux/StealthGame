#include "Server/Core/ReplySink.h"

FReplySinkId FReplySinkTable::Allocate()
{
	uint16 Index = 0;
	if (FreeIndices.Num() > 0)
	{
		Index = FreeIndices.Pop(EAllowShrinking::No);
	}
	else
	{
		check(Slots.Num() < MAX_uint16);
		Index = static_cast<uint16>(Slots.AddDefaulted());
	}
	FSlot& Slot = Slots[Index];
	// Generation 0 never names a live sink (a default FReplySinkId must not resolve after a wrap).
	if (++Slot.Generation == 0)
	{
		Slot.Generation = 1;
	}
	Slot.bLive = true;
	Slot.Buffer.Reset();

	FReplySinkId Id;
	Id.Index = Index;
	Id.Generation = Slot.Generation;
	return Id;
}

void FReplySinkTable::Release(FReplySinkId Sink)
{
	if (FSlot* Slot = Resolve(Sink))
	{
		Slot->bLive = false;
		Slot->Buffer.Reset();
		FreeIndices.Add(Sink.Index);
	}
}

bool FReplySinkTable::IsLive(FReplySinkId Sink) const
{
	return Resolve(Sink) != nullptr;
}

bool FReplySinkTable::Append(FReplySinkId Sink, FQueueKey SlotKey, const FCommandReply& Reply)
{
	FSlot* Slot = Resolve(Sink);
	if (!Slot)
	{
		// The null sink (AI) and closed connections discard: the sink decides, never a caller check.
		return false;
	}
	Slot->Buffer.Add({ SlotKey, NextOrder++, Reply });
	return true;
}

int32 FReplySinkTable::FlushPass(IReplyTransport& Transport)
{
	int32 Calls = 0;
	TArray<FCommandReply> Batch;
	for (int32 Index = 1; Index < Slots.Num(); ++Index)
	{
		FSlot& Slot = Slots[Index];
		if (!Slot.bLive || Slot.Buffer.Num() == 0)
		{
			continue;
		}
		Slot.Buffer.Sort([](const FEntry& A, const FEntry& B) { return A.Key < B.Key || (A.Key == B.Key && A.Order < B.Order); });
		Batch.Reset(Slot.Buffer.Num());
		for (const FEntry& Entry : Slot.Buffer)
		{
			Batch.Add(Entry.Reply);
		}
		Slot.Buffer.Reset();

		FReplySinkId Id;
		Id.Index = static_cast<uint16>(Index);
		Id.Generation = Slot.Generation;
		Transport.SendCommandReplies(Id, Batch);
		++Calls;
	}
	NextOrder = 0;
	return Calls;
}

int32 FReplySinkTable::GetBufferedCount() const
{
	int32 Count = 0;
	for (const FSlot& Slot : Slots)
	{
		Count += Slot.Buffer.Num();
	}
	return Count;
}

int32 FReplySinkTable::GetBufferedCount(FReplySinkId Sink) const
{
	const FSlot* Slot = Resolve(Sink);
	return Slot ? Slot->Buffer.Num() : 0;
}

FReplySinkTable::FSlot* FReplySinkTable::Resolve(FReplySinkId Sink)
{
	return const_cast<FSlot*>(static_cast<const FReplySinkTable*>(this)->Resolve(Sink));
}

const FReplySinkTable::FSlot* FReplySinkTable::Resolve(FReplySinkId Sink) const
{
	if (IsNull(Sink) || !Slots.IsValidIndex(Sink.Index))
	{
		return nullptr;
	}
	const FSlot& Slot = Slots[Sink.Index];
	return Slot.bLive && Slot.Generation == Sink.Generation ? &Slot : nullptr;
}
