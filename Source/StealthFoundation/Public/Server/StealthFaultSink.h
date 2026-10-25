#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/**
 * Fault categories reported to IStealthFaultSink (ADR-0008 section 4). Later stories append
 * members; a published value is never reused or renumbered. 0 is reserved (no fault).
 */
enum class EStealthFault : uint8
{
	/** An invariant check failed (Detail: the invariant's id). */
	InvariantViolation = 1,
	/** A weak registration was found dead when used (ADR-0007 section 4; Detail: registration slot). */
	DeadRegistration = 2,
	/** A tuning preset or value was rejected (Detail: the rejected key's id or reason code). */
	RejectedTuning = 3,
	/** A message carrying an out-of-date epoch was ignored (Detail: the stale epoch). */
	StaleEpoch = 4,
	/** A server-only Roster call ran without authority (GDD participant-state-roster R2; Detail: ERosterCall). */
	RosterNotAuthority = 5,
	/** A Roster mutator ran off the game thread (R2, I7; Detail: ERosterCall). */
	RosterWrongThread = 6,
	/** A live pawn binding was cleared by a Destroyed EndPlay during Round phase Active (R9; Detail: participant id). */
	RosterLiveBindingCleared = 7,
	/** A Roster query met a record state it does not recognise and answered not-targetable (R10; Detail: ERosterCall). */
	RosterUnrecognisedState = 8,
};

/**
 * Receives every fault the game detects instead of crashing (ADR-0008 section 4, ADR-0007
 * section 4). Production code holds an injected sink; tests inject a recording sink
 * (Server/Testing/StealthFaultSinkTestSupport.h) and assert zero unexpected faults at teardown.
 */
class IStealthFaultSink
{
public:
	virtual ~IStealthFaultSink() = default;

	/**
	 * Reports one fault.
	 * @param Fault   the category.
	 * @param Source  the reporting system or object (for example the service's name).
	 * @param Detail  a category-specific value (id, epoch, reason code); 0 when none.
	 */
	virtual void Report(EStealthFault Fault, FName Source, int64 Detail) = 0;
};
