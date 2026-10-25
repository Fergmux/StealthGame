#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/**
 * The server clock a plain core reads (ADR-0008 section 4; ADR-0002 "Server clock").
 *
 * Plain cores take an IStealthClock by injected reference and never read wall-clock time, so a
 * test can drive them with FFakeStealthClock (Server/Testing/StealthFakeClock.h). The real
 * implementation, backed by the server world's TimeSeconds and the resolution queue's tick
 * counter, arrives with the queue epic (ADR-0002).
 *
 * A plain C++ interface, not a UINTERFACE: it is held by reference, never by TWeakInterfacePtr
 * (ADR-0007 section 4).
 */
class IStealthClock
{
public:
	virtual ~IStealthClock() = default;

	/** Server time in seconds, double precision (ADR-0002: ServerNow() is the server world's TimeSeconds as a double). */
	virtual double GetServerTimeSeconds() const = 0;

	/** Server tick index, incremented once per server frame (ADR-0002: FQueueKey::ServerTickIndex is a uint64). */
	virtual uint64 GetServerTickIndex() const = 0;
};
