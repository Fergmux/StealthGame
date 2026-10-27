#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/EventLog/StealthRoundLogTypes.h"
#include "Server/StealthPenaltySeams.h"

class IRoundLogWriter;
class IStealthClock;
class IStealthFaultSink;

/**
 * Penalty's Round Event Log rows (GDD npc-kill-penalty-informants.md PEN-R17; ADR-0019 section 7; ADR-0002 section 8.9;
 * story nkp-004). Server-only, every one PostRoundEligible (FRoundEventLog::GetReleaseClass), written only from inside
 * the queue slot that produced the fact, through Round's single write path (IRoundLogWriter). Fixed-size payloads: no
 * FString, no allocation in the slot, nothing text-shaped. Never pass a row, an entry or any field to UE_LOG / UE_LOGF,
 * ClientMessage, a check or trace macro (include-lint rule (i) lists these types; StealthGame.Penalty.NoLoggingLint).
 *
 * Not Penalty's: StrikeLimit (Round writes it with the transition) and RespawnStarved (LSC's).
 *
 * Packing into FRoundLogEntry (Participants[0] is always the row's participant; doubles are two Data words, low first):
 *   StrikeCounted     D0 StrikeAfter, D1 TallyAfter, D2 Graced (0/1)
 *   RespawnScheduled  Code ERespawnCause; D0-D1 t_lost, D2-D3 T_due
 *   InformantRespawn  D0-D1 PickLatencySec, D2 WideningLevel
 *   RespawnCancelled  Code ERespawnCancelReason
 *   PawnLost          Code EPenaltyPawnLostCause
 *
 * Usage (a producing story, inside its slot):
 *   FPenaltyEventLog Log(RoundLogWriter, SlotContext, Clock, Faults);
 *   Log.Write(FPenaltyStrikeCountedRow{ Killer, StrikeAfter, TallyAfter, bGraced }, RoundEpoch);
 */

/** RespawnCancelled's reason (PEN-R10, PEN-R11; the value set is story nkp-009's proposal). Never free text. Append only. */
enum class ERespawnCancelReason : uint8
{
	Disconnect,
	RoundEnded,
	Reset,
	Stale,
};

/** PawnLost's cause: how an informant body was lost without a kill (PEN-R11). Append only. */
enum class EPenaltyPawnLostCause : uint8
{
	/** Fell out of the world. */
	KillZ,
	/** Destroyed by anything else (a defect). */
	Destroyed,
};

/** StrikeCounted{Killer, StrikeAfter, TallyAfter, Graced}: every NPC or informant kill by an Active participant (story nkp-002). */
struct FPenaltyStrikeCountedRow
{
	FParticipantId Killer;
	uint8 StrikeAfter = 0;
	uint8 TallyAfter = 0;
	bool bGraced = false;

	static constexpr ERoundLogType Type = ERoundLogType::StrikeCounted;

	bool operator==(const FPenaltyStrikeCountedRow& O) const { return Killer == O.Killer && StrikeAfter == O.StrikeAfter && TallyAfter == O.TallyAfter && bGraced == O.bGraced; }
};

/** RespawnScheduled{Participant, Cause, t_lost, T_due} (story nkp-009). */
struct FPenaltyRespawnScheduledRow
{
	FParticipantId Participant;
	ERespawnCause Cause = ERespawnCause::Assassinated;
	double LostServerTime = 0.0;
	double DueServerTime = 0.0;

	static constexpr ERoundLogType Type = ERoundLogType::RespawnScheduled;

	bool operator==(const FPenaltyRespawnScheduledRow& O) const
	{
		return Participant == O.Participant && Cause == O.Cause && LostServerTime == O.LostServerTime && DueServerTime == O.DueServerTime;
	}
};

/** InformantRespawn{Participant, PickLatencySec, WideningLevel} (story nkp-012). */
struct FPenaltyInformantRespawnRow
{
	FParticipantId Participant;
	double PickLatencySec = 0.0;
	uint8 WideningLevel = 0;

	static constexpr ERoundLogType Type = ERoundLogType::InformantRespawn;

	bool operator==(const FPenaltyInformantRespawnRow& O) const { return Participant == O.Participant && PickLatencySec == O.PickLatencySec && WideningLevel == O.WideningLevel; }
};

/** RespawnCancelled{Participant, Reason} (story nkp-009). */
struct FPenaltyRespawnCancelledRow
{
	FParticipantId Participant;
	ERespawnCancelReason Reason = ERespawnCancelReason::Disconnect;

	static constexpr ERoundLogType Type = ERoundLogType::RespawnCancelled;

	bool operator==(const FPenaltyRespawnCancelledRow& O) const { return Participant == O.Participant && Reason == O.Reason; }
};

/** PawnLost{Participant, Cause}: an informant body lost without a kill (story nkp-010). */
struct FPenaltyPawnLostRow
{
	FParticipantId Participant;
	EPenaltyPawnLostCause Cause = EPenaltyPawnLostCause::KillZ;

	static constexpr ERoundLogType Type = ERoundLogType::PawnLost;

	bool operator==(const FPenaltyPawnLostRow& O) const { return Participant == O.Participant && Cause == O.Cause; }
};

/** Encode (row -> entry payload) and decode (entry -> row; false for another type) of each row. */
namespace PenaltyEventRows
{
	STEALTHGAMEPLAY_API void Encode(const FPenaltyStrikeCountedRow& Row, FRoundLogEntry& Out);
	STEALTHGAMEPLAY_API void Encode(const FPenaltyRespawnScheduledRow& Row, FRoundLogEntry& Out);
	STEALTHGAMEPLAY_API void Encode(const FPenaltyInformantRespawnRow& Row, FRoundLogEntry& Out);
	STEALTHGAMEPLAY_API void Encode(const FPenaltyRespawnCancelledRow& Row, FRoundLogEntry& Out);
	STEALTHGAMEPLAY_API void Encode(const FPenaltyPawnLostRow& Row, FRoundLogEntry& Out);

	STEALTHGAMEPLAY_API bool Decode(const FRoundLogEntry& Entry, FPenaltyStrikeCountedRow& Out);
	STEALTHGAMEPLAY_API bool Decode(const FRoundLogEntry& Entry, FPenaltyRespawnScheduledRow& Out);
	STEALTHGAMEPLAY_API bool Decode(const FRoundLogEntry& Entry, FPenaltyInformantRespawnRow& Out);
	STEALTHGAMEPLAY_API bool Decode(const FRoundLogEntry& Entry, FPenaltyRespawnCancelledRow& Out);
	STEALTHGAMEPLAY_API bool Decode(const FRoundLogEntry& Entry, FPenaltyPawnLostRow& Out);
}

/**
 * Penalty's Event Log writer: stamps a row with the current slot key, the slot's server time and the round epoch and
 * appends it through IRoundLogWriter. A write outside a queue slot is refused (EStealthFault::QueueMisuse,
 * PenaltyFaultDetail::RowOutsideSlot) and a write with no writer faults DeadRegistration; neither crashes.
 */
class STEALTHGAMEPLAY_API FPenaltyEventLog
{
public:
	FPenaltyEventLog(IRoundLogWriter* InWriter, const IPenaltySlotContext* InSlot, const IStealthClock* InClock, IStealthFaultSink* InFaults)
		: Writer(InWriter), Slot(InSlot), Clock(InClock), Faults(InFaults)
	{
	}

	/** Writes one row; true when it was appended. */
	template <typename RowType>
	bool Write(const RowType& Row, uint32 RoundEpoch)
	{
		FRoundLogEntry Entry;
		PenaltyEventRows::Encode(Row, Entry);
		return Append(Entry, RoundEpoch);
	}

private:
	bool Append(FRoundLogEntry& Entry, uint32 RoundEpoch);

	IRoundLogWriter* Writer = nullptr;
	const IPenaltySlotContext* Slot = nullptr;
	const IStealthClock* Clock = nullptr;
	IStealthFaultSink* Faults = nullptr;
};
