#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Random/StealthServerSeeds.h"

/**
 * Session's per-start round seed draw (ADR-0006 DrawRoundSeed(RoundIndex); Round GDD R5 step 8: "Session draws a
 * fresh seed for every start and an aborted start never reuses one"). Each draw takes the next attempt of the
 * match, so a start repeated with the same RoundIndex after a rollback still gets a new seed; 0 and any seed already
 * drawn in the match are discarded. Round never draws or stores the seed itself.
 *
 * Plain C++ over an IServerSubSeedSource, so tests run it on the fixed fixture secret (story rmf-002). Lives in
 * Foundation until the Session epic's USessionService owns it (D53 integration batch).
 */
class STEALTHFOUNDATION_API FStealthRoundSeedDrawer
{
public:
	/** Source must outlive the drawer. */
	explicit FStealthRoundSeedDrawer(const IServerSubSeedSource& InSource) : Source(InSource) {}

	/** A fresh, non-zero, never-before-drawn 31-bit seed for RoundIndex. */
	int32 DrawRoundSeed(uint32 RoundIndex);

	/** Forgets the drawn seeds (a new match). */
	void Reset();

	/** Number of seeds drawn in this match. */
	int32 GetDrawCount() const { return Drawn.Num(); }

private:
	const IServerSubSeedSource& Source;
	TSet<int32> Drawn;
	uint32 NextAttempt = 0;
};
