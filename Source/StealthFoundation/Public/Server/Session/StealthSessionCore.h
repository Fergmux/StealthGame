#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Random/StealthRoundSeedDrawer.h"
#include "Server/Random/StealthServerSeeds.h"
#include "View/StealthRosterTypes.h"

class IStealthFaultSink;

/** The session exit paths (ADR-0005 section 2 Lifetime, Implementation Guideline 12): each one runs EndMatch on every store. */
enum class ESessionEndReason : uint8
{
	HostQuit,
	ReturnToMainMenu,
	SessionEnding,
	GameInstanceShutdown,
	LastWorldCleanup,
};

/**
 * A match-scoped game-instance store Session fans EndMatch out to (ADR-0005 section 2: the four stores Session, Roster,
 * Round and Tuning preset are the only authoritative match state). The consumer declares, Session calls (ADR-0007): a
 * store in a higher module (Round in StealthGameplay, Tuning) implements this and registers with USessionService.
 */
class IStealthMatchStore
{
public:
	virtual ~IStealthMatchStore() = default;

	/** A short name for faults and test messages. */
	virtual FName GetMatchStoreName() const = 0;

	/** Clears every match-scoped value (idempotent; safe when nothing is open). */
	virtual void EndMatch(ESessionEndReason Reason) = 0;

	/** True when the store holds no match state (the state a fresh OpenSession requires). */
	virtual bool IsMatchStateEmpty() const = 0;
};

/**
 * FSessionCore: Session's plain-C++ store core (ADR-0005 section 2; ADR-0007 section 5: no UWorld). This story
 * (session-travel st-014) owns its lifetime half: the session secret and match nonce (ADR-0006, opened fail-closed
 * from an injected entropy source), F5's UsedSeeds (the round seed drawer), the kick list, and End, which clears all of
 * it. Connection tables, admission, liveness and travel arrive with their own stories (015-057).
 *
 * Never log, trace or replicate the secret, the nonce or a sub-seed (ADR-0006 section 7).
 */
class STEALTHFOUNDATION_API FSessionCore
{
public:
	FSessionCore();
	FSessionCore(const FSessionCore&) = delete;
	FSessionCore& operator=(const FSessionCore&) = delete;

	/** OpenSession's core half: draws ServerSecret and MatchNonce (story st-009). CouldNotHost leaves nothing open. */
	EStealthSessionOpenResult Open(IEntropySource& Entropy, IStealthFaultSink* FaultSink);

	/** EndMatch for Session: forgets the secret, nonce, used seeds and kick list. True when a session was open. */
	bool End(ESessionEndReason Reason);

	bool IsOpen() const { return bOpen; }

	/** True when Session holds no match state (the requirement at the next OpenSession). */
	bool IsEmpty() const;

	/** The secret-holding derivation core (sub-seeds, round seed candidates, public sub-seeds). */
	FStealthServerSeeds& GetSeeds() { return Seeds; }
	const FStealthServerSeeds& GetSeeds() const { return Seeds; }

	/** F5 DrawRoundSeed for RoundIndex (the seed core's round index follows it). 0 when no session is open. */
	int32 DrawRoundSeed(uint32 RoundIndex);

	/** Number of seeds drawn this match (F5 UsedSeeds). */
	int32 GetUsedSeedCount() const { return Drawer.GetDrawCount(); }

	/** Kick list (rule 7 gate 3; keyed by identity key, never by address). */
	void AddKickedKey(const FIdentityKey& Key) { KickedKeys.Add(Key); }
	bool IsKicked(const FIdentityKey& Key) const { return KickedKeys.Contains(Key); }

	/** Exit-path bookkeeping (tests and dumps). */
	int32 GetEndCount() const { return EndCount; }
	ESessionEndReason GetLastEndReason() const { return LastEndReason; }

private:
	FStealthServerSeeds Seeds;
	FStealthRoundSeedDrawer Drawer;
	TSet<FIdentityKey> KickedKeys;
	bool bOpen = false;
	int32 EndCount = 0;
	ESessionEndReason LastEndReason = ESessionEndReason::GameInstanceShutdown;
};
