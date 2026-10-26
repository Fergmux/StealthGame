#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EventLog/StealthRoundLogTypes.h"
#include "Server/EventLog/StealthRoundLogWriter.h"

/**
 * The match-scoped archive of Round Event Log slices (GDD round-match-framework R10 (3), R19, R21; stories rmf-030,
 * rmf-034). Server only, held by FRoundMatchCore on the game-instance Round store, so it survives travel; never sent
 * to a client, a console command or a file (the release path is the Event Log's own read gate).
 *
 * FRoundLogSliceRecorder is the IRoundLogWriter Round and its queue append through: it forwards every entry to the
 * downstream writer (FRoundEventLog in production, a recording writer in tests) and keeps this round's entries, which
 * Reset step (3) moves into the archive (RoundLogRetainCount = every round of the match).
 */

/** One archived round. */
struct FRoundArchivedSlice
{
	uint32 RoundIndex = 0;
	uint32 RoundEpoch = 0;
	/** How the round left Active or Preparing (PreparingFailed carries AbortCode). */
	ERoundEndReason EndReason = ERoundEndReason::Aborted;
	/** ERoundPreparingAbortCode for PreparingFailed, else 0. */
	uint8 AbortCode = 0;
	/** True only for a round that reached Ended (its PostRoundEligible class may be released through the Policy). */
	bool bReachedEnded = false;
	TArray<FRoundLogEntry> Entries;
};

class STEALTHGAMEPLAY_API FRoundLogSliceRecorder final : public IRoundLogWriter
{
public:
	/** The writer every entry is forwarded to (may be null). */
	void SetDownstream(IRoundLogWriter* InDownstream) { Downstream = InDownstream; }

	virtual void Append(const FRoundLogEntry& Entry) override;

	/** This round's entries so far. */
	const TArray<FRoundLogEntry>& GetCurrentEntries() const { return Current; }

	/** Reset step (3): moves this round's entries into the archive and clears the round buffer. */
	void ArchiveCurrent(uint32 RoundIndex, uint32 RoundEpoch, ERoundEndReason EndReason, uint8 AbortCode, bool bReachedEnded);

	/** Drops the current buffer without archiving (a new match). */
	void ClearCurrent() { Current.Reset(); }

	/** Every archived slice of the match, oldest first. */
	const TArray<FRoundArchivedSlice>& GetArchive() const { return Archive; }

	/** A new match: the archive and the buffer are released. */
	void ResetForNewMatch();

private:
	IRoundLogWriter* Downstream = nullptr;
	TArray<FRoundLogEntry> Current;
	TArray<FRoundArchivedSlice> Archive;
};
