#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthQueuePayloads.h"
#include "View/StealthQueueTypes.h"
#include "View/StealthRosterTypes.h"

/**
 * Round's disconnect grace records (GDD round-match-framework R17.2, R17.7, F4; ADR-0002 section 8.7; story rmf-040,
 * shared with the grace-timeline stories rmf-060..071). Plain C++.
 *
 * One record per disconnected participant: {P, GraceExpiry, ExpectedEpoch = (MatchId, RoundEpoch),
 * ExpectedConnectionGeneration, Kicked}. The timer itself is the queue item TimerFired(GraceExpiry) (Subject the
 * participant, Generation the captured ConnectionGeneration, Expected the round epoch) scheduled with ScheduleAt; no
 * FTimerManager. A participant disconnected before Active gets its record at Active entry with the generation current
 * then, so a rejoin and re-drop in Preparing leaves no stale timer.
 */
struct FRoundGraceRecord
{
	FParticipantId Participant;
	double GraceExpiry = 0.0;
	FEpochToken ExpectedEpoch;
	uint32 ExpectedConnectionGeneration = 0;
	bool bKicked = false;
	/** The queue handle of the scheduled TimerFired item. */
	uint64 TimerHandleId = 0;
};

class STEALTHGAMEPLAY_API FRoundGraceTable
{
public:
	/** Adds or replaces P's record (a newer disconnect supersedes the older one). */
	void Open(const FRoundGraceRecord& Record);

	/** P's record, or null. */
	const FRoundGraceRecord* Find(FParticipantId Participant) const;

	/** Marks P's live record kicked (R17.2: no second timer, the expiry is kept). */
	void MarkKicked(FParticipantId Participant, bool bKicked);

	/** Removes P's record (a rejoin inside grace, a conversion); true when one existed. */
	bool Close(FParticipantId Participant);

	/** Every record (Reset cancels all of them). */
	void Reset() { Records.Reset(); }

	int32 Num() const { return Records.Num(); }

	/** The TimerFired(GraceExpiry) item for a record (Seq tags the participant; the epoch rides on Expected). */
	static FQueueItem MakeTimerItem(const FRoundGraceRecord& Record);

private:
	TArray<FRoundGraceRecord> Records;
};
