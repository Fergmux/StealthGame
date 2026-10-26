#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

class IStealthFaultSink;

/**
 * Server seed derivation (ADR-0006 sections 2-4; Session GDD F5). Plain C++, no UWorld.
 *
 * ADR-0006 places the server seed service in FSessionCore on USessionService (ADR-0005). That core does not
 * exist yet; FStealthServerSeeds is the plain derivation core it will own (RMF Exit A build 2026-10-25,
 * decision log: built for the Round seams rmf-002/006/007 so tests exercise the real derivation, never a
 * stand-in). The authority, game-thread and FPublicOnlyScope misuse checks belong to the USessionService
 * facade (session-travel story st-011) and are not here.
 *
 * Never log, trace, replicate or expose ServerSecret, MatchNonce or a sub-seed (ADR-0006 section 7).
 */

/** ADR-0006 section 4 domain registry: fixed explicit values, append-only, never renumbered or reused. */
enum class ESeedDomain : uint32
{
	Layout = 1,
	ObjectiveSites = 2,
	AppearanceDeal = 3,
	PlayerPlacement = 4,
	NpcRoutes = 5,
	SpawnOrder = 6,
	EmergencePick = 7,
	RespawnPick = 8,
	AmbientSchedule = 9,
	StageSignal = 10,
	SeqOrder = 11,
	TieRank = 12,
	CrowdBehaviour = 13,
	/** Amendment 2026-10-24, ADR-0017. */
	BushSpotPick = 14,
	/** Amendment 2026-10-25b, Roster R3. */
	PublicHandle = 15,
	/** Amendment 2026-10-25b, SCL R8.9. */
	IdleGaze = 16,
	/** Amendment 2026-10-25, decision D46, Roster R3. */
	MatchId = 17,
};

/** The highest registered domain value (for tests that iterate the registry). */
inline constexpr uint32 GStealthSeedDomainMax = 17;

/**
 * Entropy for the session secret (ADR-0006 section 2). Production: the OS CSPRNG (session-travel story st-009);
 * tests inject a deterministic fixture source (Guideline 8).
 */
class IEntropySource
{
public:
	virtual ~IEntropySource() = default;

	/** Fills Out completely with entropy; false on failure (a failing source is fatal to OpenSession). */
	virtual bool Fill(TArrayView<uint8> Out) = 0;
};

/** Read-only access to server sub-seeds, the seam consumers such as the resolution queue are given. */
class IServerSubSeedSource
{
public:
	virtual ~IServerSubSeedSource() = default;

	/** GetServerSubSeed(Domain, Attempt) of ADR-0006 section 3; 0 when no secret is held. */
	virtual uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const = 0;
};

/** The session secret, the match nonce and the F5 sub-seed derivation. */
class STEALTHFOUNDATION_API FStealthServerSeeds : public IServerSubSeedSource
{
public:
	/**
	 * Draws ServerSecret (16 bytes) then MatchNonce (8 bytes) from Entropy. On failure nothing is held, the
	 * fault sink (when given) is told, and false is returned: never silently replaced by a weak source.
	 */
	bool Open(IEntropySource& Entropy, IStealthFaultSink* FaultSink = nullptr);

	/** Forgets the secret and the nonce. */
	void Close();

	/** True after a successful Open. */
	bool IsOpen() const { return bOpen; }

	/** The round index that F5 folds into every derivation (Session sets it per round). */
	void SetRoundIndex(uint32 InRoundIndex) { RoundIndex = InRoundIndex; }
	uint32 GetRoundIndex() const { return RoundIndex; }

	/**
	 * F5: SipHash(ServerSecret, 0x02 | u64le(MatchNonce) | u32le(RoundIndex) | u32le(Domain) | u32le(Attempt)).
	 * Returns 0 when no secret is held.
	 */
	virtual uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const override;

private:
	uint8 ServerSecret[16] = {};
	uint64 MatchNonce = 0;
	uint32 RoundIndex = 0;
	bool bOpen = false;
};
