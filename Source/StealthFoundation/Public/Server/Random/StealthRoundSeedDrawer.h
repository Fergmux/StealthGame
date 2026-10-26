#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Random/StealthServerSeeds.h"

/**
 * Session's per-start round seed draw (ADR-0006 section 3 DrawRoundSeed(RoundIndex); Session GDD F5; Round GDD R5
 * step 8: "Session draws a fresh seed for every start and an aborted start never reuses one").
 *
 * F5: Seed = (H64(ServerSecret, 0x01 | MatchNonce | RoundIndex | Draw) mod (2^31 - 1)) + 1, redrawn with Draw + 1
 * while Seed is in UsedSeeds (every seed drawn this match, aborted starts included) or in the Generator's golden
 * regression set, so a live round never uses a test city. Round never draws or stores the seed itself.
 *
 * Plain C++ over an IServerSubSeedSource, so tests run it on the fixed fixture secret (story rmf-002). Lives in
 * Foundation until the Session epic's USessionService owns it (D53 integration batch; story st-010 made it F5).
 */
class STEALTHFOUNDATION_API FStealthRoundSeedDrawer
{
public:
	/** Source must outlive the drawer. */
	explicit FStealthRoundSeedDrawer(const IServerSubSeedSource& InSource) : Source(InSource) {}

	/** A fresh, never-before-drawn seed in [1, 2^31 - 1] for RoundIndex; 0 when the source holds no secret. */
	int32 DrawRoundSeed(uint32 RoundIndex);

	/** The Generator's golden regression seeds (Generator change request 2): never drawn for a live round. */
	void SetGoldenSeeds(TConstArrayView<int32> InGoldenSeeds) { GoldenSeeds = TSet<int32>(InGoldenSeeds); }

	/** Forgets the drawn seeds (a new match). */
	void Reset();

	/** Number of seeds drawn in this match. */
	int32 GetDrawCount() const { return Drawn.Num(); }

	/** True when Seed was drawn in this match (F5 UsedSeeds). */
	bool WasDrawn(int32 Seed) const { return Drawn.Contains(Seed); }

private:
	const IServerSubSeedSource& Source;
	TSet<int32> Drawn;
	TSet<int32> GoldenSeeds;
};
