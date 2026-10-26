#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "View/StealthQueueTypes.h"

/**
 * Round Event Log entry vocabulary (ADR-0002 section 8.9; Round GDD R19, F8). Plain C++: no USTRUCT, UCLASS or
 * UPROPERTY, no ToString or LexToString, no text conversion of any kind (ADR-0007 section 6). The only text
 * conversion is the Ended/Reset disk writer inside FRoundEventLog. Never pass an entry, a header or any of
 * their fields to a log, check, ensure, trace, CSV or stat macro (ADR-0007 lint rule (i) lists these types).
 *
 * Declared by story rmf-008 (the queue's Voided entries) and used by the store of story rmf-011.
 */

/**
 * The closed entry-type list (R19 / ADR-0002 section 8.9; adding a type is an amendment there). Move and Look
 * are never logged. CascadeOverflow is the section 5 breach entry, added to the closed list by the RMF Exit A
 * amendment (2026-10-25). Values are stable: append only.
 */
enum class ERoundLogType : uint8
{
	PhaseChanged,
	StateTransition,
	KillResolved,
	Reversal,
	StrikeLimit,
	ObjectiveStage,
	FinalCompleted,
	Unresponsive,
	Disconnected,
	Rejoined,
	GraceExpired,
	ReleasedToCrowd,
	HandOver,
	HandBack,
	InformantRespawn,
	StrikeCounted,
	RespawnScheduled,
	RespawnCancelled,
	StageSignalReleased,
	TendCompleted,
	ClueTooBroad,
	ObjectivePacing,
	Voided,
	Rejected_Latched,
	LateWinSuppressed,
	PawnLost,
	PawnRecovered,
	StartRolledBack,
	Dropped_StaleConnection,
	MailboxOverflow,
	WinLatched,
	CascadeOverflow,
};

/** Number of ERoundLogType values (per-type counters are sized by it). */
inline constexpr int32 GRoundLogTypeCount = 32;

/** The kill timeline and the win record: these occupy ring slots and are never evicted (R19 Truncation). */
inline constexpr bool IsProtectedRoundLogType(ERoundLogType Type)
{
	return Type == ERoundLogType::KillResolved || Type == ERoundLogType::Reversal || Type == ERoundLogType::StateTransition
		|| Type == ERoundLogType::StrikeLimit || Type == ERoundLogType::ObjectiveStage || Type == ERoundLogType::FinalCompleted
		|| Type == ERoundLogType::WinLatched;
}

/**
 * R19 EndReason, server-only (the public form folds ObjectiveWin/Elimination/ForfeitElimination into the two
 * public wins). PreparingFailed carries its abort code in the entry data.
 */
enum class ERoundEndReason : uint8
{
	ObjectiveWin,
	Elimination,
	ForfeitElimination,
	Aborted,
	VoidAbandoned,
	VoidKickedLastActive,
	VoidPawnLost,
	PreparingFailed,
	SessionEnded,
	DebugForced,
};

/** Release classes (R19, provisional, under the Information Visibility Policy). */
enum class ERoundLogReleaseClass : uint8
{
	NeverReleased,
	PostRoundEligible,
	PublicResult,
};

/** TendCompleted result (R19 `Counted | NoEffect`). */
enum class ERoundTendResult : uint8
{
	Counted,
	NoEffect,
};

/**
 * KillResolved result codes the Round log tallies (the WrongTargetCount of TeamSummary, ADR-0002 section 8.9
 * reader (a)). Provisional: Assassination (ADR-0018) owns the full result set and maps onto these values.
 */
enum class ERoundKillResult : uint8
{
	ParticipantKilled,
	WrongTarget,
	Other,
};

/** No team (an entry about a non-team subject, or a void with no winner). */
inline constexpr uint32 GRoundLogNoTeam = 0xFF;

/** Entry flag bits. */
namespace RoundLogFlags
{
	/** TendCompleted: the attempt arrived after the latch (R19 `Latched = true`). */
	inline constexpr uint8 Latched = 1 << 0;
	/** The entry stems from a debug command (R22; excluded from playtest metrics). */
	inline constexpr uint8 Debug = 1 << 1;
}

/**
 * One fixed-size entry (F8: key 16 B, time 8, epoch 4, type, mailbox, participant ids, data; 72 bytes here).
 *
 * Field use by type (Data index: meaning; Code is the 16-bit code named):
 *   StateTransition   P0 participant; Code ERosterReason; D0 old state, D1 new state, D2 team, D3 EventSeq,
 *                     D4 ActiveCountAfter (team 0 low 16 bits, team 1 high), D5 InformantCountAfter (same packing)
 *   KillResolved      P0 attacker participant (invalid for none), P1 victim participant; Code ERoundKillResult;
 *                     D0 attacker team, D1 victim kind
 *   ObjectiveStage    D0 team, D1 stage
 *   FinalCompleted    D0 team
 *   TendCompleted     P0 participant; Code ERoundTendResult; D0 court socket id; Flags Latched
 *   PhaseChanged      D0 old phase, D1 new phase (Round's values; Lobby..Reset = 1..5)
 *   Voided            Code EVoidReason; D0 EQueueItemType, D1 owner-specific code
 *   Rejected_Latched  D0 EQueueItemType
 *   Dropped_StaleConnection  P0 participant; D0 EQueueItemType, D1 item generation, D2 current generation
 *   MailboxOverflow   D0 items dropped from SourceMailbox in this drain
 *   CascadeOverflow   D0 1 (written once per slot, at the first breach; every moved follow-on gets its own slot)
 *   WinLatched        Code ERoundEndReason; D0 winner team (GRoundLogNoTeam for none), D1 PreparingFailed code;
 *                     Key is the terminal key
 * Never store a server secret, sub-seed, stream key or entropy (R19 "Secrets are not stored").
 */
struct FRoundLogEntry
{
	FQueueKey Key;
	double ServerTime = 0.0;
	uint32 RoundEpoch = 0;
	ERoundLogType Type = ERoundLogType::PhaseChanged;
	uint8 Flags = 0;
	uint16 Code = 0;
	FMailboxId SourceMailbox;
	uint32 Participants[2] = { FParticipantId::InvalidValue, FParticipantId::InvalidValue };
	uint32 Data[6] = { 0, 0, 0, 0, 0, 0 };

	bool operator==(const FRoundLogEntry& Other) const
	{
		return Key == Other.Key && ServerTime == Other.ServerTime && RoundEpoch == Other.RoundEpoch && Type == Other.Type
			&& Flags == Other.Flags && Code == Other.Code && SourceMailbox == Other.SourceMailbox
			&& Participants[0] == Other.Participants[0] && Participants[1] == Other.Participants[1]
			&& FMemory::Memcmp(Data, Other.Data, sizeof(Data)) == 0;
	}
	bool operator!=(const FRoundLogEntry& Other) const { return !(*this == Other); }
};
