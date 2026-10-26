#pragma once

#include "CoreMinimal.h"
#include "View/CommandReplyTypes.h"
#include "View/StealthRosterTypes.h"

/**
 * Resolution-queue core value types (ADR-0002 section 2: Foundation Public/View, because every layer names
 * them; plain ids with no server-only content). The queue payloads themselves are server-only and live in
 * Server/StealthQueuePayloads.h (ADR-0002 section 2a).
 *
 * Built by the Round & Match Framework Exit A stories rmf-004 and rmf-006.
 */

/**
 * The total-order key (ADR-0002 section 1): compared lexicographically, unique, drained ascending.
 * ServerTickIndex increments once per server frame at drain start; Seq restarts at 1 each round and is
 * assigned when the item is stamped, never on arrival. Seq 0 never names a stamped item.
 */
struct FQueueKey
{
	uint64 ServerTickIndex = 0;
	uint64 Seq = 0;

	constexpr FQueueKey() = default;
	constexpr FQueueKey(uint64 InServerTickIndex, uint64 InSeq) : ServerTickIndex(InServerTickIndex), Seq(InSeq) {}

	/** True for a key the queue stamped (Seq starts at 1). */
	constexpr bool IsSet() const { return Seq != 0; }

	constexpr bool operator==(const FQueueKey& Other) const { return ServerTickIndex == Other.ServerTickIndex && Seq == Other.Seq; }
	constexpr bool operator!=(const FQueueKey& Other) const { return !(*this == Other); }
	constexpr bool operator<(const FQueueKey& Other) const
	{
		return ServerTickIndex < Other.ServerTickIndex || (ServerTickIndex == Other.ServerTickIndex && Seq < Other.Seq);
	}
	constexpr bool operator>(const FQueueKey& Other) const { return Other < *this; }

	/** Lexicographic three-way compare: -1, 0 or 1 (ADR-0002 Compare). */
	static constexpr int32 Compare(const FQueueKey& A, const FQueueKey& B) { return A < B ? -1 : (B < A ? 1 : 0); }
};

/**
 * One intake mailbox (ADR-0002 section 3): one per participant plus one server-local mailbox for AI and server
 * sources. Keyed by participant, never connection or controller (controllers are replaced at travel). The
 * server-local value sorts after every participant, which gives the canonical base order "ascending
 * ParticipantId, server-local last" by Value alone.
 */
struct FMailboxId
{
	/** Value of the server-local mailbox (above every issued ParticipantId, below FParticipantId::InvalidValue). */
	static constexpr uint32 ServerLocalValue = MAX_uint32 - 1;

	uint32 Value = FParticipantId::InvalidValue;

	constexpr FMailboxId() = default;

	/** The mailbox of a participant. */
	static constexpr FMailboxId ForParticipant(FParticipantId Participant) { FMailboxId Id; Id.Value = Participant.Value; return Id; }

	/** The single server-local mailbox (AI, host-local and server sources). */
	static constexpr FMailboxId ServerLocal() { FMailboxId Id; Id.Value = ServerLocalValue; return Id; }

	constexpr bool IsServerLocal() const { return Value == ServerLocalValue; }
	constexpr bool IsValid() const { return Value != FParticipantId::InvalidValue; }

	/** The owning participant (invalid for the server-local mailbox). */
	constexpr FParticipantId GetParticipant() const { return IsServerLocal() ? FParticipantId() : FParticipantId(Value); }

	constexpr bool operator==(const FMailboxId& Other) const { return Value == Other.Value; }
	constexpr bool operator!=(const FMailboxId& Other) const { return Value != Other.Value; }
	constexpr bool operator<(const FMailboxId& Other) const { return Value < Other.Value; }

	friend uint32 GetTypeHash(const FMailboxId& Id) { return ::GetTypeHash(Id.Value); }
};

/** The epoch token carried by every queued item (ADR-0002 section 2; Roster R8 `(MatchId, RoundEpoch)`). */
struct FEpochToken
{
	uint64 MatchId = 0;
	uint32 RoundEpoch = 0;

	constexpr FEpochToken() = default;
	constexpr FEpochToken(uint64 InMatchId, uint32 InRoundEpoch) : MatchId(InMatchId), RoundEpoch(InRoundEpoch) {}

	constexpr bool operator==(const FEpochToken& Other) const { return MatchId == Other.MatchId && RoundEpoch == Other.RoundEpoch; }
	constexpr bool operator!=(const FEpochToken& Other) const { return !(*this == Other); }
};

// FClientCommandSeq (ADR-0011) moved to View/CommandReplyTypes.h as a USTRUCT (story iif-001): it travels in
// ServerPawnCommand and the reply batch. FCommandPayload (ADR-0002 section 2a) still names it through the include above.
