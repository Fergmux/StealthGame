#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/StealthClock.h"

/**
 * Test double for IStealthClock (ADR-0008 section 4, story tdc-007). Compiled only under
 * WITH_STEALTH_TEST_SEAMS.
 *
 * The fake changes only when a test tells it to, so a core under test never sees wall-clock time.
 * Seconds and ticks advance independently: the fake invents no tick length. A test that needs
 * both to move together calls Advance(Seconds, Ticks) with the relation it wants. Instances hold
 * no shared state, so two fakes are fully independent.
 */
class FFakeStealthClock : public IStealthClock
{
public:
	/** Starts at the given time and tick (both 0 by default). */
	explicit FFakeStealthClock(double InitialSeconds = 0.0, uint64 InitialTick = 0)
		: Seconds(InitialSeconds)
		, Tick(InitialTick)
	{
	}

	virtual double GetServerTimeSeconds() const override { return Seconds; }
	virtual uint64 GetServerTickIndex() const override { return Tick; }

	/** Moves server time forward by DeltaSeconds (must be >= 0); the tick index is unchanged. */
	void AdvanceSeconds(double DeltaSeconds)
	{
		ensureMsgf(DeltaSeconds >= 0.0, TEXT("FFakeStealthClock: server time never runs backwards"));
		Seconds += FMath::Max(DeltaSeconds, 0.0);
	}

	/** Moves the tick index forward by DeltaTicks; server time is unchanged. */
	void AdvanceTicks(uint64 DeltaTicks)
	{
		Tick += DeltaTicks;
	}

	/** Moves both, with the relation between them chosen by the caller. */
	void Advance(double DeltaSeconds, uint64 DeltaTicks)
	{
		AdvanceSeconds(DeltaSeconds);
		AdvanceTicks(DeltaTicks);
	}

private:
	double Seconds;
	uint64 Tick;
};

#endif // WITH_STEALTH_TEST_SEAMS
