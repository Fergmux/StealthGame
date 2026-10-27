#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EncounterTypes.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Server/Queue/StealthStagedWriteApplier.h"
#include "Server/StrikeWarpPointCore.h"
#include "Server/StrikeRoleTable.h"
#include "Server/VictimResolverCore.h"

class IBushSpotQuery;
class IRoundLogWriter;
class IServerSubSeedSource;
class IStealthClock;

/** The seams FAttackResolverCore is built with. Clock, Queue, Roster, Pawns and Defence are required. */
struct FAttackResolverDeps
{
	const IStealthClock* Clock = nullptr;
	IEncounterQueue* Queue = nullptr;
	IEncounterRosterGateway* Roster = nullptr;
	const IEncounterPawnQuery* Pawns = nullptr;
	IEncounterDefenceStore* Defence = nullptr;
	/** Rule 2's BushSpotPick sub-seed source (null: every bush strike is a whiff, a wiring defect). */
	const IServerSubSeedSource* SubSeeds = nullptr;
	/** Null: default-deny, no PawnRef is honoured. */
	IEncounterVisibilityQuery* Visibility = nullptr;
	/** Null: no spot zones, nobody concealed. */
	IBushSpotQuery* Bushes = nullptr;
	IEncounterActionEvents* Events = nullptr;
	IEncounterBraceStanceSink* Stance = nullptr;
	IEncounterCardLedger* Cards = nullptr;
	/** Round's Event Log writer (KillResolved / Reversal rows with the slot key); null: no rows. */
	IRoundLogWriter* Log = nullptr;
	/** The replicated pawn state (FActionState, FDeathState; stories asn-012, 013); null: nothing written on the wire. */
	IEncounterPawnStateWriter* PawnState = nullptr;
};

/** One staged write the applier applied (observation for tests and the next stories; story asn-013). */
struct FAppliedEncounterWrite
{
	EStagedWriteType Type = EStagedWriteType::DeathWrite;
	APawn* Pawn = nullptr;
	/** The server time the staging pass applied it. */
	double AppliedServerTime = 0.0;
	/** DeathWrite: DeathServerTime; CounterPose: the reversed strike's ContactServerTime. */
	double EventServerTime = 0.0;
	bool bDiedConcealed = false;
};

#if WITH_STEALTH_TEST_SEAMS
namespace StealthEncounterTestSeams
{
	/**
	 * ADR-0008 section 3 negative control of StealthGame.Assassination.OutcomeBlindWire (story asn-012 AC2): when set, the
	 * slot writes the outcome into FActionState.Variant (bits 3-4), the exact leak the outcome-blind wire forbids.
	 */
	extern STEALTHGAMEPLAY_API bool bWriteOutcomeIntoVariant;
}
#endif

/**
 * FAttackResolverCore: the strike resolver of the round's single queue (GDD ASN-R2, R5, R8-R15, F2-F4; ADR-0002 sections
 * 5 and 8, ADR-0018 section 2.1; stories asn-005 to asn-011). One IQueueResolver for EQueueItemType::PawnCommand and one
 * for EQueueItemType::HoldTimer, registered from the composition root on URoundMatchStore::GetRoundQueue(); headless, the
 * same object is registered on a test FResolutionQueueCore or called directly with fabricated keys.
 *
 * PawnCommand slot (Kind from FCommandPayload::Kind):
 *   Attack        live re-check (ASN-R2): attacker gone, Dead or not legality-Active -> Voided(NotNow); running or
 *                 traversing -> Voided(NotNow) (IIF R7); Striking or Countering -> Voided(Busy). Otherwise it EXECUTES,
 *                 whatever its target (ASN-R1: no target-dependent rejection), in this order:
 *                   own brace superseded (Bracing attacker: window ended at this key, Cancelled(Superseded));
 *                   (1) victim (FVictimResolverCore) and outcome: Whiff (no victim), Reversal (Braced(V, k), F2), Kill;
 *                       the fallen pawn's role is Dead with DeathServerTime = ContactServerTime from here;
 *                   attacker Striking and LockoutEnd = t_exec + AttackAttemptLockoutSec on the defence state (every
 *                       outcome, including a whiff and a reversal); OnActionStarted(Attacker, Attack) (outcome-blind: the
 *                       crowd's R8 stimulus and Concealment's break-on-Attack);
 *                   (3) RequestTransition before (6) OnPawnKilled: kill of a Player -> (V, Informant, Assassinated); reversal
 *                       of a Player attacker -> (A, Informant, ReversedAgainst), RetireAtServerTime = ContactServerTime;
 *                       Round's elimination check runs inline from the Roster event and may latch in this slot;
 *                   (4) VacateCard(fallen pawn); (5) Concealment occupancy exit of a victim killed concealed;
 *                   (6) OnPawnKilled(Victim, Killer, Kind, Cause) for every kill and reversal, never for a whiff;
 *                   reversal: the defender's brace consumed (window ended at this key, Cancelled(Reversed)) and the
 *                       defender Countering for CounterDurationSec from the slot; nothing else changes on it;
 *                   (7) staging: DeathWrite{fallen pawn, ContactServerTime} and, for a reversal, CounterPose{defender}, both
 *                       at ContactServerTime - StrikeNetLeadSec (the applier and its timing tests are story asn-013);
 *                   (8) the FAttackResolution and the Executed reply recorded for the reply step (story asn-015);
 *                   (9) Event Log rows KillResolved{victim kind} / Reversal{attacker, defender} with the slot key.
 *                 Assassination holds no strike counter, creates no item carrying the outcome and never calls Latch.
 *   PrepareBegin  stand-in for the IIF hold machinery (IIF stories 042-050): gone, Dead or not Active -> Voided(NotNow);
 *                 running, traversing or concealed -> Voided(NotNow); Striking or Countering -> Voided(Busy); F3 cooldown
 *                 running -> Voided(Cooldown) (no stamp change); otherwise T_act is stamped, the window opens at this key,
 *                 the role is Bracing, the planted stance is set and HoldTimer(CapEnd) is scheduled at T_act + Duration.
 *   PrepareEnd    a live window schedules HoldTimer(DebounceExpiry) at now + ReleaseDebounceSec (a pending End still
 *                 protects, ASN-R5); otherwise Voided(NotNow).
 *   other kinds   the action slot: Dead -> Voided(NotNow); Striking/Countering -> Voided(Busy); Bracing -> Voided(Busy);
 *                 otherwise forwarded to the fallback resolver (the stand-in dispatch until IIF stage 2; none: Voided).
 * HoldTimer slot (ActionType Prepare; any other type goes to the fallback hold resolver): the timer of the current
 * activation (BeginKey matches) ends the window at its key, Bracing -> Cooling, stance off; a stale one is Voided.
 *
 * Rejected_Latched (OnRejectedLatched): an Attack gets no lockout, pose, outcome or result; its owner reply NotNow is
 * recorded as held (the held-reply buffer releases it at the round-end step). Other kinds go to the fallback.
 */
class STEALTHGAMEPLAY_API FAttackResolverCore final : public IQueueResolver
{
public:
	/** HoldTimer(ActionType Attack) ActionEpoch values: the end of a Strike pose and of a Counter pose (story asn-012). */
	static constexpr uint32 StrikePoseEndEpoch = 0;
	static constexpr uint32 CounterPoseEndEpoch = 1;

	explicit FAttackResolverCore(const FAttackResolverDeps& InDeps, const FEncounterParams& InParams = FEncounterParams::Defaults());

	/** Replaces the parameters (the next round's frozen set). */
	void SetParams(const FEncounterParams& InParams) { Params = InParams; }
	const FEncounterParams& GetParams() const { return Params; }
	const FAttackResolverDeps& GetDeps() const { return Deps; }

	/** The stand-in dispatch for the non-Assassination commands and holds sharing the slot (IIF stage 2 replaces it). */
	void SetFallbackResolvers(IQueueResolver* InCommands, IQueueResolver* InHolds) { FallbackCommands = InCommands; FallbackHolds = InHolds; }

	// --- IQueueResolver -------------------------------------------------------------------------------------------------

	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;
	virtual void OnRejectedLatched(const FQueueItem& Item, FQueueKey Key) override;

	// --- Queries --------------------------------------------------------------------------------------------------------

	/** The action-slot answer for Kind now (unset: the slot is free). Dead -> NotNow; Striking/Countering -> Busy. */
	TOptional<ECommandRejectCode> CheckSlot(const APawn* Pawn, ECommandKind Kind) const;
	/** The pawn's strike role now. */
	EStrikeRole GetRole(const APawn* Pawn) const;
	bool IsDead(const APawn* Pawn) const { return Roles.IsDead(Pawn); }
	/** DeathServerTime recorded in the kill slot (valid inside the OnPawnKilled fan-out and the Roster flush). */
	TOptional<double> GetScheduledDeathServerTime(const APawn* Pawn) const { return Roles.GetScheduledDeathServerTime(Pawn); }
	const FStrikeRoleTable& GetRoles() const { return Roles; }
	/** The victim resolver's seams as this core binds them (tests probe rule 2 with them). */
	FVictimResolverDeps MakeResolverDeps() const;

	// --- Events and records ---------------------------------------------------------------------------------------------

	/** Fired inline in the slot, after the Roster transition (server-only native; Penalty, Crowd vacancy, Round log). */
	FOnPawnKilled OnPawnKilled;

	/** Every executed Attack since the last Take, in key order. */
	const TArray<FAttackResolution>& GetResolutions() const { return Resolutions; }
	TArray<FAttackResolution> TakeResolutions() { return MoveTemp(Resolutions); }
	/** Every Attack / Prepare / slot-gated reply since the last Take, in key order (the IIF reply step's input). */
	const TArray<FEncounterCommandReply>& GetReplies() const { return Replies; }
	TArray<FEncounterCommandReply> TakeReplies() { return MoveTemp(Replies); }

	// --- Staged writes (story asn-013) ------------------------------------------------------------------------------

	/**
	 * The apply-only IStagedWriteApplier to register for EStagedWriteType::DeathWrite and CounterPose (ADR-0002 section 6,
	 * ADR-0018 section 3.2). It decides nothing, never stages or schedules, and treats a stale pawn handle as a no-op:
	 *   DeathWrite  FDeathState{DeathServerTime, bDiedConcealed} on the fallen pawn inside FStealthForcedNetUpdate(DeathWrite)
	 *               and the freeze (from now, or from the pawn's contact while its own lunge runs); then OnDeathWriteApplied.
	 *               Any victim kind, after the latch too; nothing on the Roster, a card, the score or legality.
	 *   CounterPose WriteActionState(Defender, {Counter, 0, now, ContactServerTime, 0}); never a freeze; then
	 *               OnCounterPoseApplied. Skipped for a defender killed since (Dead).
	 * Writes of a reset round never arrive (the queue drops them by epoch).
	 */
	IStagedWriteApplier* GetStagedWriteApplier() { return &Applier; }
	/** The applier's body (also called directly by headless tests). */
	void ApplyStagedWrite(const FStagedWrite& Write);
	/** Every write applied since the last Reset, in apply order. */
	const TArray<FAppliedEncounterWrite>& GetAppliedWrites() const { return AppliedWrites; }

	/** Fired after a DeathWrite's pawn half (the prototype death presentation stand-in subscribes until story asn-019). */
	FOnDeathWriteApplied OnDeathWriteApplied;
	/** Fired after a CounterPose write. */
	FOnCounterPoseApplied OnCounterPoseApplied;

	/** The warp derivation of one Attack at its slot (pure; story asn-012). */
	FStrikeWarpPointResult ComputeWarp(const FEncounterPawnFacts& Attacker, const FVector& WorldPoint, const FVictimResolution& Victim, EStrikeReachVariant Variant) const;

	/** Round Reset (story asn-019 asserts it): every role Ready; records dropped. Defence components are cleared by their owner. */
	void Reset();

private:
	EResolveResult ResolveCommand(const FQueueItem& Item, const FCommandPayload& Cmd, FQueueKey Key);
	EResolveResult ResolveAttack(const FQueueItem& Item, const FCommandPayload& Cmd, FQueueKey Key);
	EResolveResult ResolvePrepareBegin(const FQueueItem& Item, const FCommandPayload& Cmd, FQueueKey Key);
	EResolveResult ResolvePrepareEnd(const FQueueItem& Item, const FCommandPayload& Cmd, FQueueKey Key);
	EResolveResult ResolveHoldTimer(const FQueueItem& Item, const FHoldTimerPayload& Hold, FQueueKey Key);
	/** HoldTimer(ActionType Attack): the Strike / Counter pose returns to None (outcome-blind housekeeping). */
	EResolveResult ResolvePoseEnd(const FHoldTimerPayload& Hold);
	/** The planted stance: the SCL sink (tests) and the FActionState Brace / None write (production). */
	void SetBraceStance(APawn* Pawn, bool bBraced, bool bWritePose = true);
	void WritePose(APawn* Pawn, EActionPose Pose, uint8 Variant, double Start, double Contact, const FFixedWarpOffset& WarpPoint);
	void SchedulePoseEnd(const FQueueItem& Item, APawn* Pawn, FQueueKey Key, double Due, uint32 Epoch);

	/** The kill branch (steps 1-9 for a kill). */
	void ApplyKill(const FQueueItem& Item, FQueueKey Key, APawn* Attacker, FAttackResolution& Resolution);
	/** The reversal branch (attacker eliminated, brace consumed, Countering). */
	void ApplyReversal(const FQueueItem& Item, FQueueKey Key, APawn* Attacker, APawn* Defender, FAttackResolution& Resolution);
	void StageDeath(APawn* Fallen, double ContactServerTime);
	void LogRow(const FQueueItem& Item, FQueueKey Key, bool bReversal, APawn* Attacker, APawn* Other, EVictimKind Kind);
	/** Ends Pawn's open window at Key (supersede, reversal): role Cooling-or-Countering is set by the caller. */
	void EndWindow(APawn* Pawn, FQueueKey Key);

	EResolveResult Void(const FCommandPayload& Cmd, FQueueKey Key, ECommandRejectCode Code);
	void RecordExecuted(const FCommandPayload& Cmd, FQueueKey Key);
	double Now() const;
	static EVictimKind ToVictimKind(ERosterVictimKind Kind);

	FAttackResolverDeps Deps;
	FEncounterParams Params;
	FStrikeRoleTable Roles;
	IQueueResolver* FallbackCommands = nullptr;
	IQueueResolver* FallbackHolds = nullptr;
	TArray<FAttackResolution> Resolutions;
	TArray<FEncounterCommandReply> Replies;

	/** The registered applier: forwards to ApplyStagedWrite. */
	class FApplier final : public IStagedWriteApplier
	{
	public:
		explicit FApplier(FAttackResolverCore& InOwner) : Owner(InOwner) {}
		virtual void ApplyStagedWrite(const FStagedWrite& Write) override { Owner.ApplyStagedWrite(Write); }

	private:
		FAttackResolverCore& Owner;
	};
	FApplier Applier{ *this };
	/** Victims killed while concealed (FDeathState.bDiedConcealed at their death write). */
	TSet<FObjectKey> DiedConcealed;
	TArray<FAppliedEncounterWrite> AppliedWrites;
};
