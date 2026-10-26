#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Templates/Function.h"

#include <atomic>

class IStealthFaultSink;

/**
 * Server seed derivation (ADR-0006 sections 2-4; Session GDD F5). Plain C++, no UWorld.
 *
 * ADR-0006 places the server seed service in FSessionCore on USessionService (ADR-0005). That core does not
 * exist yet; FStealthServerSeeds is the plain derivation core it will own (RMF Exit A build 2026-10-25,
 * decision log: built for the Round seams rmf-002/006/007 so tests exercise the real derivation, never a
 * stand-in). The session-travel stories st-009..011 completed it: the F5 round seed, the public sub-seed, the
 * fail-closed open and the GetServerSubSeed misuse branch (authority through an injected query, game thread,
 * FPublicOnlyScope), which USessionService will wrap.
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

	/** F5's round seed for (RoundIndex, Draw) in [1, 2^31 - 1]; 0 when no secret is held (a fake source may return 0). */
	virtual int32 GetRoundSeedCandidate(uint32 RoundIndex, uint32 Draw) const { return 0; }
};

/**
 * The production entropy source (ADR-0006 section 2): Windows BCryptGenRandom with BCRYPT_USE_SYSTEM_PREFERRED_RNG,
 * checked with BCRYPT_SUCCESS (never CreateGuid, never a time seed). Fill returns false when the OS call fails; the
 * caller fails closed. Other platforms (ADR-0024: Linux getrandom) report failure until implemented.
 */
class STEALTHFOUNDATION_API FOsEntropySource final : public IEntropySource
{
public:
	virtual bool Fill(TArrayView<uint8> Out) override;
};

#if !UE_BUILD_SHIPPING
/**
 * Test injection (ADR-0006 section 2 and Guideline 8; non-Shipping only): returns the given bytes in order, then
 * fails. ServerSecret and MatchNonce opened through it are exactly the injected bytes (secret first, then nonce).
 */
class STEALTHFOUNDATION_API FDeterministicEntropySource final : public IEntropySource
{
public:
	explicit FDeterministicEntropySource(TConstArrayView<uint8> InBytes) : Bytes(InBytes) {}

	virtual bool Fill(TArrayView<uint8> Out) override;

	/**
	 * The development override `-StealthServerSecret=<32 hex digits>` (non-Shipping only): on success Out holds the
	 * 16 secret bytes followed by 8 nonce bytes derived from them (SipHash of the secret under a fixed public key), so
	 * a run is reproducible from the command line. False when the switch is absent or malformed.
	 */
	static bool FromCommandLine(const TCHAR* CommandLine, TArray<uint8>& Out);

	/** The marker literal the AC-22 Shipping binary scan must not find (ADR-0004; referenced so it is not stripped). */
	static const TCHAR* Sentinel();

private:
	TArray<uint8> Bytes;
	int32 Cursor = 0;
};
#endif

/** Why OpenSession could not open (the user-facing result is "could not host"). */
enum class EStealthSessionOpenResult : uint8
{
	Opened,
	/** The entropy source failed: the session does not open and no weaker source is used. */
	CouldNotHost,
};

/** Seed API misuse kinds (EStealthFault::SeedMisuse detail, high 32 bits). */
enum class EStealthSeedMisuse : uint8
{
	NoAuthority = 1,
	NotGameThread = 2,
	InsidePublicOnlyScope = 3,
	NonLayoutPublicDomain = 4,
};

#if WITH_STEALTH_TEST_SEAMS
/**
 * The expected-error hook of the seed misuse branch (ADR-0006 Validation: "raises the expected assert through an
 * expected-error hook"). While one scope is alive, the misuse branch counts the assert here instead of firing the
 * ensure (the fault-sink report is unchanged). Thread-safe counters; the most recent scope wins.
 */
struct STEALTHFOUNDATION_API FStealthSeedMisuseExpectScope
{
	FStealthSeedMisuseExpectScope();
	~FStealthSeedMisuseExpectScope();
	FStealthSeedMisuseExpectScope(const FStealthSeedMisuseExpectScope&) = delete;
	FStealthSeedMisuseExpectScope& operator=(const FStealthSeedMisuseExpectScope&) = delete;

	/** Asserts raised while this scope was active, per misuse kind (index = EStealthSeedMisuse value). */
	int32 Count(EStealthSeedMisuse Misuse) const;
	int32 Total() const;

	/** Called by the misuse branch; true when a scope consumed the assert. */
	static bool Consume(EStealthSeedMisuse Misuse);

private:
	std::atomic<int32> Counts[8];
	FStealthSeedMisuseExpectScope* Previous = nullptr;
};
#endif

/**
 * Generator guard (ADR-0006 section 3, R-G2.2): the Layout Service wraps every provider call in this RAII scope; a
 * GetServerSubSeed call inside it takes the misuse branch. The depth counter is thread_local, so the guard also
 * covers a Generator stage run on a worker thread.
 */
struct STEALTHFOUNDATION_API FPublicOnlyScope
{
	FPublicOnlyScope();
	~FPublicOnlyScope();
	FPublicOnlyScope(const FPublicOnlyScope&) = delete;
	FPublicOnlyScope& operator=(const FPublicOnlyScope&) = delete;

	/** True while this thread is inside at least one scope. */
	static bool IsActive();
};

/** The session secret, the match nonce and the F5 derivations. */
class STEALTHFOUNDATION_API FStealthServerSeeds : public IServerSubSeedSource
{
public:
	/**
	 * Draws ServerSecret (16 bytes) then MatchNonce (8 bytes) from Entropy. On failure nothing is held, the
	 * fault sink (when given) is told, and false is returned: never silently replaced by a weak source.
	 * The fault sink is kept for later misuse reports.
	 */
	bool Open(IEntropySource& Entropy, IStealthFaultSink* InFaultSink = nullptr);

	/** OpenSession's seed half (story st-009): Open, mapped to the user-facing result. */
	EStealthSessionOpenResult OpenSession(IEntropySource& Entropy, IStealthFaultSink* InFaultSink);

	/** The user-facing text of a failed OpenSession. */
	static FText GetCouldNotHostText();

	/** Forgets the secret and the nonce. */
	void Close();

	/** True after a successful Open. */
	bool IsOpen() const { return bOpen; }

	/** The round index that F5 folds into every sub-seed derivation (Session sets it per round). */
	void SetRoundIndex(uint32 InRoundIndex) { RoundIndex = InRoundIndex; }
	uint32 GetRoundIndex() const { return RoundIndex; }

	/**
	 * The authority query of the owning facade (USessionService: the world is not a client). Unset means authority
	 * (a plain core in a test or on the host). A query answering false makes GetServerSubSeed a misuse.
	 */
	void SetAuthorityQuery(TFunction<bool()> InQuery) { AuthorityQuery = MoveTemp(InQuery); }

	/** The sink misuse is reported to (Open sets it too). */
	void SetFaultSink(IStealthFaultSink* InFaultSink) { FaultSink = InFaultSink; }

	/**
	 * F5: SipHash(ServerSecret, 0x02 | u64le(MatchNonce) | u32le(RoundIndex) | u32le(Domain) | u32le(Attempt)).
	 * Returns 0 when no secret is held. Misuse (no authority, off the game thread, inside FPublicOnlyScope) is a plain
	 * branch in every build: 0, a SeedMisuse fault with the domain, and in non-Shipping builds an ensure.
	 */
	virtual uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const override;

	/**
	 * F5's round seed for one draw: (SipHash(ServerSecret, 0x01 | u64le(MatchNonce) | u32le(RoundIndex) | u32le(Draw))
	 * mod (2^31 - 1)) + 1, in [1, 2^31 - 1]. The redraw rule (used and golden seeds) is FStealthRoundSeedDrawer's.
	 * 0 when no secret is held or on misuse (as GetServerSubSeed).
	 */
	virtual int32 GetRoundSeedCandidate(uint32 InRoundIndex, uint32 Draw) const override;

	/**
	 * F5 PublicSubSeed(Domain, Attempt) = Mix32(Seed, Domain, Attempt), the public layout sub-seed. Refuses every
	 * domain except Layout: 0, a SeedMisuse fault and (non-Shipping) an ensure.
	 */
	uint32 PublicSubSeed(int32 Seed, ESeedDomain Domain, uint32 Attempt) const;

private:
	/** The misuse branch of a secret call: true when the call must refuse (already reported). */
	bool RefuseSecretCall(ESeedDomain Domain) const;
	void ReportMisuse(EStealthSeedMisuse Misuse, ESeedDomain Domain) const;

	uint8 ServerSecret[16] = {};
	uint64 MatchNonce = 0;
	uint32 RoundIndex = 0;
	bool bOpen = false;
	IStealthFaultSink* FaultSink = nullptr;
	TFunction<bool()> AuthorityQuery;
};
