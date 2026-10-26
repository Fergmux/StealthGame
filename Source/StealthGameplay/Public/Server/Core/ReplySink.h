#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthQueuePayloads.h"
#include "View/CommandReplyTypes.h"

/**
 * The reply step of the pawn command interface (ADR-0011 section 8; story iif-004).
 *
 * Every owner reply (a rejection, an FHoldStatus, later Assassination's owner results) is appended inside the slot
 * that produced it to its sink's per-pass buffer, and flushed once at the end of that pass as ONE batched
 * ClientCommandReplies per connection, entries ordered by the queue key of the producing slot (ties keep append
 * order, so within one slot the IIF's reply precedes the feature's result). Arrival time therefore never encodes
 * where in the drain a failure occurred, and at most one reply RPC per connection per drain is sent.
 *
 * The null sink (FReplySinkId index 0: AI and server-local sources) discards on append: that is the only difference
 * between sources, decided by the sink, never by a caller check. A connection whose buffer is empty at the flush
 * gets no call. A released sink (connection closed mid-drain) discards its buffer without a send.
 *
 * Usage (server):
 *   const FReplySinkId Sink = Table.Allocate();              // when a connection's controller is set up
 *   Table.Append(Sink, SlotKey, FCommandReply::MakeRejected(Seq, ECommandRejectCode::Busy));   // inside a slot
 *   Table.FlushPass(Transport);                               // at the end of the drain's resolution step
 *   Table.Release(Sink);                                      // when the connection goes away
 */

/** Sends one connection's batch (the controller's ClientCommandReplies in production; a spy in tests). */
class IReplyTransport
{
public:
	virtual ~IReplyTransport() = default;

	/** Called at most once per live sink per FlushPass, with that pass's entries in key order. Never inside a slot. */
	virtual void SendCommandReplies(FReplySinkId Sink, const TArray<FCommandReply>& Replies) = 0;
};

/** The server's reply-sink table: opaque generation-guarded ids and one per-pass buffer per sink. */
class STEALTHGAMEPLAY_API FReplySinkTable
{
public:
	/** The null sink (AI, server-local): appends to it are discarded. */
	static FReplySinkId NullSink() { return FReplySinkId(); }

	/** True for the null sink. */
	static bool IsNull(FReplySinkId Sink) { return Sink.Index == 0; }

	/** Allocates a live sink (index >= 1); a reused slot gets a new generation. */
	FReplySinkId Allocate();

	/** Releases a sink: its buffered entries are discarded unsent, and the id never resolves again. */
	void Release(FReplySinkId Sink);

	/** True for an allocated, not yet released sink with a matching generation. */
	bool IsLive(FReplySinkId Sink) const;

	/** Appends Reply produced by the slot keyed SlotKey. Null, stale or released sinks discard (returns false). O(1). */
	bool Append(FReplySinkId Sink, FQueueKey SlotKey, const FCommandReply& Reply);

	/**
	 * Ends the pass: for every live sink with entries, sorts them by (SlotKey, append order) and calls the transport
	 * once; then clears every buffer. Sinks with no entries get no call. Returns the number of transport calls.
	 */
	int32 FlushPass(IReplyTransport& Transport);

	/** Entries currently buffered across every sink (0 after a flush). */
	int32 GetBufferedCount() const;

	/** Entries buffered for one sink. */
	int32 GetBufferedCount(FReplySinkId Sink) const;

private:
	struct FEntry
	{
		FQueueKey Key;
		uint32 Order = 0;
		FCommandReply Reply;
	};

	struct FSlot
	{
		uint16 Generation = 0;
		bool bLive = false;
		TArray<FEntry> Buffer;
	};

	FSlot* Resolve(FReplySinkId Sink);
	const FSlot* Resolve(FReplySinkId Sink) const;

	/** Index 0 is the null sink and is never allocated. */
	TArray<FSlot> Slots{ FSlot() };
	TArray<uint16> FreeIndices;
	uint32 NextOrder = 0;
};
