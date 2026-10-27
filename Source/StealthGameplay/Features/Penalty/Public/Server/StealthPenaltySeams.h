#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Roster/StealthRosterCallTypes.h"
#include "View/CommandReplyTypes.h"
#include "View/StealthQueueTypes.h"
#include "View/StealthRosterTypes.h"

class APawn;
struct FStealthServerTuning;

/**
 * The NPC-Kill Penalty & Informants vocabulary and the consumer-owned seams its plain core is injected with (GDD
 * design/gdd/npc-kill-penalty-informants.md; ADR-0019 sections 1-2, 7; ADR-0007 section 4; stories nkp-001 to nkp-004).
 *
 * Plain C++ interfaces, held by raw pointer, never by a weak interface pointer: the service (UPenaltyService) builds
 * the production adapters over URosterStore, the round queue and the tuning stand-in, and the Logic tests inject
 * fakes (StealthGame.Penalty.*). Nothing here is a USTRUCT, a UPROPERTY or a replicated type (ADR-0019 section 1).
 */

/**
 * An opaque, stable key of one pawn (the service builds it from the pawn's object index and serial number, so a
 * recycled address never aliases an older pawn). The core never resolves it: it only compares and hashes it, so a test
 * makes one from any number without a UObject.
 */
struct FPenaltyPawnKey
{
	uint64 Value = 0;

	constexpr FPenaltyPawnKey() = default;
	explicit constexpr FPenaltyPawnKey(uint64 InValue) : Value(InValue) {}

	/** False for "no pawn". */
	constexpr bool IsSet() const { return Value != 0; }

	constexpr bool operator==(const FPenaltyPawnKey& Other) const { return Value == Other.Value; }
	constexpr bool operator!=(const FPenaltyPawnKey& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FPenaltyPawnKey& Key) { return ::GetTypeHash(Key.Value); }
};

/**
 * Every tuning value Penalty reads, latched once per round at Preparing (TDC rule CR-11 Latching; PEN-R4). Never a
 * literal in Penalty code: the values come from the server tuning face through IPenaltyTuningSource.
 *
 * Usage:
 *   const FPenaltyParams Params = FPenaltyParams::FromTuning(Frozen);   // L, the release grace
 */
struct STEALTHGAMEPLAY_API FPenaltyParams
{
	/** L: NpcKillStrikeLimit (removal on the L-th ungraced NPC kill; F1). */
	int32 NpcKillStrikeLimit = 0;
	/** PEN-R5 StrikeOutReleaseGraceSec (inclusive window after a release in which a kill adds no strike). */
	double StrikeOutReleaseGraceSec = 0.0;

	/** The Penalty values of a tuning set (server face). */
	static FPenaltyParams FromTuning(const FStealthServerTuning& Values);
};

/** The live server tuning face, as Penalty reads it (ADR-0004; read only when a round latches, never mid-round). */
class IPenaltyTuningSource
{
public:
	virtual ~IPenaltyTuningSource() = default;

	/** The current values of the Penalty keys (the core calls this once per Preparing and once at construction). */
	virtual FPenaltyParams ReadPenaltyParams() const = 0;
};

/** The Roster reads Penalty makes (server-only; the production adapter is over URosterStore's records). */
class IPenaltyRosterQuery
{
public:
	virtual ~IPenaltyRosterQuery() = default;

	/** The participant's Roster state; unset for an unknown or removed participant. */
	virtual TOptional<EParticipantState> GetState(FParticipantId Participant) const = 0;
	/** The Roster's current RoundEpoch (the ExpectedEpoch of every owner-view write). */
	virtual uint32 GetRoundEpoch() const = 0;
};

/**
 * The Roster's owner-view feed (Roster R6, CR-19 approved 2026-10-25; roster story psr-038 builds the server face).
 * Penalty calls SetOwnerTally only on reclaim, rejoin (OnViewRebound) and OnRosterReset, never per strike (PEN-R3,
 * AC-NKP-27): that discipline is Penalty's, the Roster cannot police it. A stale ExpectedEpoch is the Roster's
 * Rejected_StaleEpoch. Production: none until psr-038 lands (the service then has no feed and re-pushes nothing).
 */
class IPenaltyOwnerViewFeed
{
public:
	virtual ~IPenaltyOwnerViewFeed() = default;

	/** Writes {WrongTargetTally, LastResultSeq} to Participant's C-TeamView owner part (owner audience only). */
	virtual void SetOwnerTally(FParticipantId Participant, uint8 WrongTargetTally, FClientCommandSeq LastResultSeq, uint32 ExpectedEpoch) = 0;
};

/**
 * Whether the caller runs inside a resolution-queue slot, and which (ADR-0002 section 5). Penalty's slot-local flags
 * (the exile flag bExiled reads, the pending StrikeCounted row) and every Event Log write are keyed by the slot.
 * Production: FResolutionQueueCore::IsInSlot / CurrentKey.
 */
class IPenaltySlotContext
{
public:
	virtual ~IPenaltySlotContext() = default;

	virtual bool IsInSlot() const = 0;
	/** The current slot's key (unset outside a slot). */
	virtual FQueueKey CurrentKey() const = 0;
};

/**
 * The calls the exile (ADR-0019 section 2.3 steps 1-4; story nkp-005) and the informant kill (ADR-0019 section 2.1;
 * story nkp-008) make on other systems' facades, by pawn key. The core calls them inline in the killing Attack's slot,
 * in one call stack, with no await, no timer and no early return after step 1 (except a rejected step 1). Production:
 * UPenaltyService's adapter resolves the key to the slot's pawn and forwards to URosterStore, ICrowdControl and
 * IPenaltyPawnGateway; the Logic tests record the order (StealthGame.Penalty.ExileOnLthNpcKill).
 */
class IPenaltyBodyGateway
{
public:
	virtual ~IPenaltyBodyGateway() = default;

	/**
	 * Step 1: Roster RequestTransition(Participant, Informant, StrikeLimit, ExpectedEpoch). The Roster releases the live
	 * pawn to the crowd inside it (decision D42: "release, not retire"), so the body is Unbound (an NPC) on return; its
	 * Roster event drives Round's elimination check in the same slot.
	 */
	virtual ERosterResult RequestStrikeLimit(FParticipantId Participant, uint32 ExpectedRoundEpoch) = 0;
	/** Step 2: Character Pool ReleaseCard(Body): BoundLive -> NpcLive with the same AppearanceId. */
	virtual void ReleaseCard(FPenaltyPawnKey Body) = 0;
	/** Step 3: Crowd AdoptReleasedPawn(Body, {RestrictedUntilServerTime, bExitAtNextScheduledSlot}). */
	virtual void AdoptReleasedPawn(FPenaltyPawnKey Body, double RestrictedUntilServerTime, bool bExitAtNextScheduledSlot) = 0;
	/**
	 * Step 4: the pawn factory's RequestHandOver(Body, Ai, ExpectedEpoch): the control epoch is bumped now (a command
	 * stamped under the player's control is stale from here); the AI possession waits for PossessNotBeforeServerTime,
	 * the end of the body's strike lockout (amendment request to ADR-0010: never during a root-motion action pose).
	 */
	virtual void RequestHandOverToAi(FPenaltyPawnKey Body, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime) = 0;
	/**
	 * Informant kill: Roster RetireBinding(Body, RetireAtServerTime, InformantKilled) (Roster story psr-020; decision D10:
	 * not a transition, no event), so a second strike on the falling body resolves NotTargetable(AlreadyResolved).
	 */
	virtual ERosterResult RetireBinding(FPenaltyPawnKey Body, double RetireAtServerTime) = 0;
};

/**
 * The respawn scheduler of story nkp-009 (F2 T_due, the InformantRespawn heap item), as the informant kill calls it
 * (ADR-0019 section 2.1: ScheduleRespawn(VictimParticipant, InformantKilled, slot time)). Null until nkp-009 lands: the
 * production respawn is still the prototype game mode's (AStealthGameMode::HandleEncounterBodyReleased).
 */
class IPenaltyRespawnScheduler
{
public:
	virtual ~IPenaltyRespawnScheduler() = default;

	/** Schedules Participant's informant body for Cause, the body lost at LostServerTime (inside the causing slot). */
	virtual void ScheduleRespawn(FParticipantId Participant, ERespawnCause Cause, double LostServerTime) = 0;
};

/**
 * The pawn-level half of the exile that other epics own and the composition root registers with UPenaltyService
 * (Character Pool's ReleaseCard, the pawn factory's RequestHandOver; consumer-owned per ADR-0007). Null: those two
 * steps are skipped (headless worlds). Production today: FStealthPenaltyPawnGateway (StealthGame composition), whose
 * ReleaseCard is a STAND-IN (the Character Pool epic is not built; the prototype pawn keeps its look, so its card is
 * unchanged) and whose hand-over goes through UStealthPawnFactory::RequestHandOver.
 */
class IPenaltyPawnGateway
{
public:
	virtual ~IPenaltyPawnGateway() = default;

	/** Character Pool ReleaseCard(Pawn): BoundLive -> NpcLive, the same AppearanceId. */
	virtual void ReleaseCard(APawn* Pawn) = 0;
	/** The factory's RequestHandOver(Pawn, Ai): epoch bumped now, AI possession not before PossessNotBeforeServerTime. */
	virtual void RequestHandOverToAi(APawn* Pawn, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime) = 0;
};

/** Fault-sink Detail codes Penalty reports (EStealthFault::InvariantViolation unless stated). Append only. */
namespace PenaltyFaultDetail
{
	/** An Informant participant reached OnPawnKilled as the killer (ASN-R2 voids it; a defect). */
	inline constexpr int64 InformantKiller = 0x4E4B5001; // "NKP" 01
	/** StrikeCount > WrongTargetTally after a tally update (F1 invariant). */
	inline constexpr int64 StrikeAboveTally = 0x4E4B5002;
	/** An Event Log row was written outside a queue slot (EStealthFault::QueueMisuse). */
	inline constexpr int64 RowOutsideSlot = 0x4E4B5003;
	/** An Event Log row was written with no log writer (EStealthFault::DeadRegistration). */
	inline constexpr int64 NoLogWriter = 0x4E4B5004;
	/** The exile's RequestTransition(StrikeLimit) was rejected: nothing else of the exile ran (story nkp-005). */
	inline constexpr int64 ExileTransitionRejected = 0x4E4B5005;
	/** An informant kill's RetireBinding did not apply (the body stays BoundLive; story nkp-008). */
	inline constexpr int64 InformantRetireRejected = 0x4E4B5006;
}
