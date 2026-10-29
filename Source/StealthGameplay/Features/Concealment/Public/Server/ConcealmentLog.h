#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EventLog/StealthRoundLogTypes.h"
#include "View/StealthRosterTypes.h"

/**
 * The concealment slice of the Round Event Log (ADR-0017 section 10; GDD concealment-bushes.md CON-R15; stories con-004,
 * con-030). Server-only rows {Tick, BushId, PawnKind, Event} appended through IRoundLogWriter by the Commit, Cross, Emerge,
 * kill and bush-strike resolvers. Never written through UE_LOG / UE_LOGF or any log, trace or check macro (ADR-0007 lint
 * rule (i)), never released to clients in MVP (FRoundEventLog::GetReleaseClass: NeverReleased). PawnKind is read only
 * here, for metrics (ConcealmentMetrics.h); no legality, timing or reply reads it.
 *
 * Field use (FRoundLogEntry, Type Concealment): Code EConcealmentLogEvent; P0 the pawn's participant (invalid for an NPC);
 * D0 BushId; D1 PawnKind (0 NPC, 1 player); BushStrike D2 candidates before the reach filter, D3 after it; Emerge D4 1 when
 * the strike killed; Tick = Key.ServerTickIndex.
 */

/** Concealment's Event Log events (FRoundLogEntry::Code). Values are stable: append only. */
enum class EConcealmentLogEvent : uint8
{
	Commit,
	ExitCross,
	Emerge,
	Killed,
	BushStrike,
};

/** One concealment row read back from an FRoundLogEntry (metrics, tests). */
struct FConcealmentLogRow
{
	uint64 Tick = 0;
	double ServerTime = 0.0;
	EConcealmentLogEvent Event = EConcealmentLogEvent::Commit;
	FParticipantId Participant;
	uint16 BushId = 0;
	bool bPlayer = false;
	/** BushStrike: candidates before the reach filter. */
	int32 CandidatesBeforeReach = 0;
	/** BushStrike: candidates after the reach filter (the uniform pick's set). */
	int32 CandidatesAfterReach = 0;
	/** Emerge: the strike that broke concealment killed. */
	bool bKilled = false;
};

namespace ConcealmentLog
{
	/** The one row builder (every concealment row goes through it). */
	STEALTHGAMEPLAY_API FRoundLogEntry MakeEntry(FQueueKey Key, double ServerTime, EConcealmentLogEvent Event, FParticipantId Participant,
		uint16 BushId, bool bPlayer, uint32 D2 = 0, uint32 D3 = 0, uint32 D4 = 0);

	/** Reads Entry as a concealment row. False for any other type or an unknown code. */
	STEALTHGAMEPLAY_API bool Decode(const FRoundLogEntry& Entry, FConcealmentLogRow& OutRow);

	/** Every concealment row of Entries, in log order. */
	STEALTHGAMEPLAY_API TArray<FConcealmentLogRow> DecodeAll(TConstArrayView<FRoundLogEntry> Entries);
}
