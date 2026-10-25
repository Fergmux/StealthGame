#pragma once

#include "CoreMinimal.h"

/**
 * Participant State & Roster id types and wire-safe enums (GDD design/gdd/participant-state-roster.md
 * R3; ADR-0005 section 2; story psr-001).
 *
 * FParticipantId and FIdentityKey are distinct plain struct types, never raw integers or strings, so a
 * reflection scan (story psr-039) can tell them apart from every other parameter type. Neither is a
 * USTRUCT: neither can be a UPROPERTY, a Blueprint type or an RPC parameter (R2). Both are server-only
 * values (R3 audience ServerOnly); they live in View/ only because the C-TeamView and Session code that
 * names them must compile on both sides without including a Server/ header.
 */

/** Match-stable participant id: monotonic per match, never reused, never sent to a client (R3). */
struct FParticipantId
{
	/** Value used for "no participant". */
	static constexpr uint32 InvalidValue = MAX_uint32;

	uint32 Value = InvalidValue;

	FParticipantId() = default;
	explicit constexpr FParticipantId(uint32 InValue) : Value(InValue) {}

	/** True for an id the Roster issued (it may since have been removed). */
	bool IsValid() const { return Value != InvalidValue; }

	bool operator==(const FParticipantId& Other) const { return Value == Other.Value; }
	bool operator!=(const FParticipantId& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FParticipantId& Id) { return ::GetTypeHash(Id.Value); }
};

/**
 * The client-asserted 128-bit install key used for rejoin (R3, Session rule 6). In MVP it is
 * unauthenticated; the Roster never receives it from a gameplay RPC (R2).
 */
struct FIdentityKey
{
	uint64 Hi = 0;
	uint64 Lo = 0;

	FIdentityKey() = default;
	constexpr FIdentityKey(uint64 InHi, uint64 InLo) : Hi(InHi), Lo(InLo) {}

	bool operator==(const FIdentityKey& Other) const { return Hi == Other.Hi && Lo == Other.Lo; }
	bool operator!=(const FIdentityKey& Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FIdentityKey& Key) { return HashCombine(::GetTypeHash(Key.Hi), ::GetTypeHash(Key.Lo)); }
};

/** Lifecycle state of a participant (R3 `State`, States and Transitions table). Owner and Team audience only. */
enum class EParticipantState : uint8
{
	/** Competing this round. */
	Active,
	/** Eliminated or removed for the rest of the round; walks, observes, can be killed and respawns. */
	Informant,
	/** Not in a round: round over, newly registered and not yet reset in, or Dormant. */
	RoundEnded,
};

/** A participant's team (R3 `Team`): 0, 1 or None. Never replicated to opponents (R6). */
enum class ERosterTeam : uint8
{
	Team0 = 0,
	Team1 = 1,
	None = 0xFF,
};
