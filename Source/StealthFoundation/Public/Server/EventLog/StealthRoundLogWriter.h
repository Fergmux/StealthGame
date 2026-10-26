#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EventLog/StealthRoundLogTypes.h"

/**
 * The single Round Event Log write path (ADR-0002 section 8.9). Every producer (Round, Roster, Assassination,
 * Penalty, Objectives, IIF, Concealment, the queue itself) appends only through Append: game thread only, from
 * inside the drain slot that produced the fact, in key order. No other write path exists and no producer keeps
 * a private log. FRoundEventLog (story rmf-011) is the store behind it.
 *
 * Plain C++: held by reference or pointer by producers, never by a weak interface pointer.
 */
class IRoundLogWriter
{
public:
	virtual ~IRoundLogWriter() = default;

	/** Appends one entry. The store may rate-limit Voided entries and evict unprotected ones (R19). */
	virtual void Append(const FRoundLogEntry& Entry) = 0;
};
