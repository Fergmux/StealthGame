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
 * Penalty's per-participant sub-state (GDD States and Transitions; ADR-0019 sections 1, 3.1). Server-only; the Roster state
 * is authoritative. Exiled is never a resting state (the exile leaves the participant Awaiting).
 */
enum class EInformantLifeState : uint8
{
	/** Competing (Roster Active), or an informant whose request ended without a body. */
	Active,
	/** Informant, no pawn, one InformantRespawn item pending. */
	Awaiting,
	/** The item fired: drawing or retrying (the LSC widening level is the record's LastWideningLevel). */
	Picking,
	/** Informant with a live bound pawn. */
	Embodied,
	/** Informant, disconnected, no pawn, no item; t_lost and t_floor kept. */
	Absent,
	/** Frozen at the round end. */
	RoundEnded,
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
	/** F2 D: InformantRespawnDelaySec (Penalty's public key, story nkp-009). */
	double InformantRespawnDelaySec = 0.0;
	/** F2 floor input: Assassination's DeathFallSec. */
	double DeathFallSec = 0.0;
	/** F2 floor input: Camera's NoPawnFadeSec (a pending-owner constant until Camera registers it; TD-TDC-I3). */
	double NoPawnFadeSec = 0.0;
	/** The commit's regular retry cadence: LSC's EmergenceRetrySec (pending-owner constant until LSC registers it). */
	double EmergenceRetrySec = 0.0;

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

	/**
	 * True when the participant's Roster connection projection is Disconnected or Dormant (Session's Unresponsive collapses
	 * to Connected, Roster R11: an Unresponsive participant counts as connected; PEN-R8). Default: connected.
	 */
	virtual bool IsDisconnected(FParticipantId Participant) const { return false; }
	/** The participant's team (the respawn pick's region). Default: none. */
	virtual ERosterTeam GetTeam(FParticipantId Participant) const { return ERosterTeam::None; }
	/** True when the participant has a live (BoundLive) pawn (the commit's re-validation). Default: none. */
	virtual bool HasLivePawn(FParticipantId Participant) const { return false; }
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

	/**
	 * Writes the respawn due time T_due (0 clears) to Participant's C-TeamView owner part (ADR-0019 section 6 option (a);
	 * story nkp-011). Only the carrier adapter (FPenaltyDueAdapter) calls it, never while the round is latched.
	 */
	virtual void SetOwnerRespawnDue(FParticipantId Participant, double DueServerTime, uint32 ExpectedEpoch) = 0;
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
 * One TimerFired(InformantRespawn) item as Penalty asks for it (ADR-0019 section 3.2; ADR-0002 section 2a): the queue
 * adapter turns it into FTimerPayload {Kind = InformantRespawn, Subject = Participant, Seq = RequestSeq, Generation = 0,
 * Phase = 0} with {MatchId, RoundEpoch} as FQueueItem::Expected. Cause is an observation for tests and is never put in
 * the payload.
 */
struct FPenaltyRespawnItem
{
	FParticipantId Participant;
	uint32 RequestSeq = 0;
	uint32 RoundEpoch = 0;
	ERespawnCause Cause = ERespawnCause::Assassinated;
};

/**
 * The due-time heap as Penalty uses it (stories nkp-009 scheduling and cancellation, nkp-012 retries). Production:
 * FPenaltyRespawnQueueAdapter over Round's FResolutionQueueCore (ScheduleAt / CancelTimer), which is also the
 * InformantRespawn resolver. Penalty uses no engine timer, tick or latent action (ADR-0019 forbidden approach).
 */
class IPenaltyRespawnTimers
{
public:
	virtual ~IPenaltyRespawnTimers() = default;

	/** Schedules Item at DueServerTime; returns its handle (0 when it could not be scheduled). */
	virtual uint64 ScheduleRespawnItem(double DueServerTime, const FPenaltyRespawnItem& Item) = 0;
	/** Cancels a pending item by handle (Cancel(TimerHandle)); a handle no longer pending is a no-op. */
	virtual void CancelRespawnItem(uint64 Handle) = 0;
};

/** PickRespawnPawn's status (ADR-0014 section 7.3 FRespawnPick, confirmation requested; ADR-0019 section 4.1 step 2). */
enum class EPenaltyPickStatus : uint8
{
	Picked,
	/** No eligible unseen pawn in the region at the current widening level: the regular retry. */
	None,
	/** The one-pick-per-server-frame guard: the next-frame retry, no widening time consumed. */
	Deferred,
};

/** One pick (Pawn set only for Picked). */
struct FPenaltyRespawnPick
{
	EPenaltyPickStatus Status = EPenaltyPickStatus::None;
	FPenaltyPawnKey Pawn;
	/** The LSC widening level the pick used (0..2). */
	uint8 WideningLevel = 0;
};

/** TakeOverCrowdPawn's answer as Penalty acts on it (ADR-0010 section 3.2; ADR-0019 section 4.1 step 5). */
enum class EPenaltyTakeoverResult : uint8
{
	Applied,
	/** The factory's eligibility failed (a defect once the step-3 pre-check exists). */
	NotEligible,
	/** BindPawn rejected (Rejected_StaleEpoch / WrongState): the request is stale. */
	Rejected_Stale,
};

/**
 * The respawn commit's calls on other systems' facades (ADR-0019 section 4.1 steps 2-6; story nkp-012), by pawn key, in
 * one call stack inside the InformantRespawn slot. Production: the composition root's FStealthPenaltyRespawnGateway, whose
 * pick (Level Socket Contract story 033), takeover (Shared Character story 028) and card (Character Pool) are STAND-INs
 * behind this seam until those epics land; TryReleaseNpc and AdoptReleasedPawn are the Crowd Director's. The Logic tests
 * inject recording fakes (StealthGame.Penalty.CommitSequence).
 */
class IPenaltyRespawnGateway
{
public:
	virtual ~IPenaltyRespawnGateway() = default;

	/** ULayoutService::PickRespawnPawn(Team, RequestSeq, Exclude): the only random draw of the sequence. */
	virtual FPenaltyRespawnPick PickRespawnPawn(ERosterTeam Team, uint32 RequestSeq, TConstArrayView<FPenaltyPawnKey> Exclude) = 0;
	/** UStealthPawnFactory::CheckTakeOverEligibility(Pawn): side-effect free, the same predicate as TakeOverCrowdPawn's. */
	virtual bool CheckTakeOverEligibility(FPenaltyPawnKey Pawn) const = 0;
	/** ITakeoverEligibilitySource::TryReleaseNpc(Pawn, Expected): Penalty is its only caller. */
	virtual bool TryReleaseNpc(FPenaltyPawnKey Pawn, uint32 ExpectedRoundEpoch) = 0;
	/** UStealthPawnFactory::TakeOverCrowdPawn(Pawn, Participant, ExpectedEpoch): the factory binds on Penalty's behalf. */
	virtual EPenaltyTakeoverResult TakeOverCrowdPawn(FPenaltyPawnKey Pawn, FParticipantId Participant, uint32 ExpectedRoundEpoch) = 0;
	/** ICrowdControl::AdoptReleasedPawn(Pawn, {}): the exact reverse of TryReleaseNpc (a released pawn not taken over). */
	virtual void AdoptReleasedPawn(FPenaltyPawnKey Pawn) = 0;
	/** UAppearanceAllocator::TakeNpcCard(Participant, Pawn): NpcLive -> BoundLive; the AppearanceId, unset on failure. */
	virtual TOptional<uint16> TakeNpcCard(FParticipantId Participant, FPenaltyPawnKey Pawn) = 0;
	/** Roster RecordAppearance(Participant, AppearanceId) (log-only history). */
	virtual void RecordAppearance(FParticipantId Participant, uint16 AppearanceId) = 0;
};

/**
 * Where Penalty raises OnDueTimeChanged(P, T_due) (ADR-0019 section 6; stories nkp-009..011): set at scheduling and
 * readmission, unset at embodiment, cancellation, round end and reset. Production: UPenaltyService broadcasts its
 * server-only FOnDueTimeChanged, which the carrier adapter (FPenaltyDueAdapter) consumes.
 */
class IPenaltyDueTimeSink
{
public:
	virtual ~IPenaltyDueTimeSink() = default;

	virtual void OnDueTimeChanged(FParticipantId Participant, TOptional<double> DueServerTime) = 0;
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
	/** TakeOverCrowdPawn answered NotEligible after the step-3 pre-check passed (a defect; story nkp-012). */
	inline constexpr int64 TakeoverNotEligibleAfterPrecheck = 0x4E4B5007;
	/** TakeNpcCard failed after an Applied takeover (LedgerMismatch: the takeover stands, no rollback; story nkp-012). */
	inline constexpr int64 LedgerMismatch = 0x4E4B5008;
	/** A respawn item could not be scheduled (no timer seam, or the queue refused it; EStealthFault::DeadRegistration). */
	inline constexpr int64 RespawnNotScheduled = 0x4E4B5009;
}
