#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Server/Core/CancelReason.h"
#include "Server/Roster/StealthRosterCore.h"
#include "Server/StealthQueuePayloads.h"
#include "Server/StealthTuningSubstructs.h"
#include "Server/StrikeReachCore.h"
#include "View/CommandReplyTypes.h"
#include "View/NetPawnCommand.h"
#include "View/StealthEncounterWire.h"
#include "View/StealthTuningPublicSubstructs.h"

class APawn;
class FDefenceStateCore;
struct FStealthServerTuning;

/**
 * The shared vocabulary of the strike resolver (GDD assassination-defence-reversal.md ASN-R2, R8-R15, F2, F4; ADR-0018
 * section 2.1; stories asn-005 to asn-011): the outcome enums, OnPawnKilled, the resolver's parameters, the per-pawn facts
 * it reads and the consumer-owned seams it is injected with. Plain C++: every engine contact is behind one of the
 * interfaces below, so FAttackResolverCore runs headless on fakes (StealthGame.Assassination.*), and UEncounterService
 * binds the production adapters.
 *
 * Pawn identity: APawn pointers are opaque keys here. The core never dereferences one; positions, kinds, relations and
 * concealment come only through the seams, read at the Attack's slot (ASN-R2: live state, nothing at submission).
 */

/** OnPawnKilled's victim kind (ASN-R10; the Roster's ResolveVictimKind, NotTargetable never reaches a kill). */
enum class EVictimKind : uint8
{
	Player,
	Npc,
	Informant,
};

/** OnPawnKilled's cause (ASN-R10, R11): a strike kill, or the attacker eliminated by a reversal. */
enum class EKillCause : uint8
{
	Strike,
	Reversal,
};

/** The per-pawn strike role (GDD States and Transitions), layered on the IIF action slot. */
enum class EStrikeRole : uint8
{
	Ready,
	/** A Prepare(Begin) executed and its window is not ended: protected (F2 Slot(V) = Bracing). */
	Bracing,
	/** After a reversal, for CounterDurationSec from the slot: pose-locked, every discrete command Busy, NOT protected. */
	Countering,
	/** After any executed Attack, for AttackAttemptLockoutSec: every discrete command Busy. */
	Striking,
	/** The brace ended and the 60 s cooldown still runs (Prepare replies Cooldown; Attack is legal). */
	Cooling,
	/** Killed or reversed in a slot (death write possibly still pending): never a candidate again (R8, F4). */
	Dead,
};

/** One executed Attack's outcome (ASN-R10, F4). */
enum class EAttackOutcome : uint8
{
	Whiff,
	Kill,
	Reversal,
};

/** Which ASN-R8 rule found the victim (None: no victim, a whiff). Server-only observation; never sent. */
enum class EVictimRule : uint8
{
	None,
	PawnRef,
	BushSpot,
	OpenSpot,
};

/** OnPawnKilled(Victim, Killer, VictimKind, Cause): server-only, fired inline in the slot after the Roster transition. */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnPawnKilled, APawn* /*Victim*/, APawn* /*Killer*/, EVictimKind /*VictimKind*/, EKillCause /*Cause*/);

/**
 * Keys other owners have not registered yet (the TD-TDC-I3 pattern, as StealthTuningPendingOwnerKeys). When the IIF
 * registers ReleaseDebounceSec the params read it from the frozen set instead.
 */
namespace StealthEncounterPendingKeys
{
	/** IIF ReleaseDebounceSec (IIF F2 default 0.10 s): a released brace protects until arrival + this. */
	inline constexpr double ReleaseDebounceSec = 0.10;
}

/** Every value the resolver reads (validated tuning plus the foreign keys), frozen per round. Never literals in the core. */
struct STEALTHGAMEPLAY_API FEncounterParams
{
	FAssassinationTuning_Public Public;
	FAssassinationTuning_Server Server;
	/** SCL capsule radius r_c (schema constant, provisional 35). */
	double CapsuleRadiusCm = 35.0;
	/** Concealment's BushStrikeEmergeSec (pending owner key, 0.35). */
	double BushStrikeEmergeSec = 0.35;
	/** The IIF's PawnRefAgreementCm (pending owner key, 75). */
	double PawnRefAgreementCm = 75.0;
	/** The IIF's ReleaseDebounceSec (pending owner key, 0.10). */
	double ReleaseDebounceSec = StealthEncounterPendingKeys::ReleaseDebounceSec;
	/** The round's layout attempt: the BushSpotPick sub-seed is GetServerSubSeed(BushSpotPick, LayoutAttempt) (ADR-0006 domain 14). */
	uint32 LayoutAttempt = 0;

	/** The parameters of a frozen tuning set (foreign keys through FAssassinationInvariantInputs::FromValues). */
	static FEncounterParams FromTuning(const FStealthServerTuning& Values);
	/** The schema defaults (the stand-in until the TDC tuning store feeds features the frozen set). */
	static FEncounterParams Defaults();

	/** F1's parameters of Variant. */
	FStrikeReachParams ReachParams(EStrikeReachVariant Variant) const;
	/** T_v of Variant: StrikeContactSec, plus BushStrikeEmergeSec for the bush strike. */
	double ContactSec(EStrikeReachVariant Variant) const;
	/** r_c + OpenSpotToleranceCm: the open-spot candidate radius around the WorldPoint (rule 3). */
	double OpenSpotRadiusCm() const { return CapsuleRadiusCm + Server.OpenSpotToleranceCm; }
};

/** What the resolver reads about one pawn at the slot (server capsule state; never bones, never kind or team). */
struct FEncounterPawnFacts
{
	APawn* Pawn = nullptr;
	/** Server capsule centre. */
	FVector Location = FVector::ZeroVector;
	/** Body facing (actor yaw), degrees. */
	double FacingYawDeg = 0.0;
	/** Server velocity. */
	FVector Velocity = FVector::ZeroVector;
	/** SCL baseline braking of the pawn's current gait (F1 a_b). */
	double BrakingCmPerSec2 = 0.0;
	/** A value every pawn has (the open-spot tie rule, ASN-R8.3). */
	uint64 SpawnSerial = 0;
	/** SCL IsRunning / in traversal (IIF R7: an Attack or Prepare by such a pawn is NotNow). */
	bool bRunning = false;
	bool bTraversing = false;
};

// ---------------------------------------------------------------------------------------------------------
// Consumer-owned seams (the composition root binds the production adapters; tests bind fakes)
// ---------------------------------------------------------------------------------------------------------

/** The live pawns of the round and their facts (production: the world's AStealthPawns; tests: scripted facts). */
class IEncounterPawnQuery
{
public:
	virtual ~IEncounterPawnQuery() = default;

	/** Every live pawn (any kind) that could be a candidate. */
	virtual void GetPawns(TArray<FEncounterPawnFacts>& OutPawns) const = 0;
	/** The facts of one pawn; false when it is gone. */
	virtual bool GetFacts(const APawn* Pawn, FEncounterPawnFacts& OutFacts) const = 0;
};

/**
 * The Roster as the resolver needs it (Roster R8, R10; GetRelation is Roster story 032). Production: URosterStore;
 * tests: a fake whose RequestElimination drives Round's elimination check exactly as the Roster event does.
 */
class IEncounterRosterGateway
{
public:
	virtual ~IEncounterRosterGateway() = default;

	/** ResolveVictimKind (R10): Player, Informant, Npc or NotTargetable with its reason. */
	virtual FRosterVictimKind ResolveVictimKind(const APawn* Pawn) = 0;
	/** GetRelation(A, B) = Teammate: both bound to participants of one team. */
	virtual bool AreTeammates(const APawn* A, const APawn* B) = 0;
	/** The legality class is Active (an Unbound pawn answers Active, R10). */
	virtual bool IsLegalityActive(const APawn* Pawn) = 0;
	/** RequestTransition(Participant(Pawn), Informant, Reason) with RetireAtServerTime (Roster story 021). Inline: its event reaches Round in this slot. */
	virtual void RequestElimination(const APawn* Pawn, ERosterReason Reason, double RetireAtServerTime) = 0;
	/** The bound participant (invalid for an NPC), for the Event Log rows. */
	virtual FParticipantId GetParticipant(const APawn* Pawn) = 0;
	/** The bound participant's team (0, 1), or GRoundLogNoTeam (0xFF) for an unbound pawn. */
	virtual uint32 GetTeamIndex(const APawn* Pawn) = 0;
};

/** The Visibility Policy's CanViewerKnow(Viewer, Pawn, Position) (ADR-0003 default-deny; IVP story 010). */
class IEncounterVisibilityQuery
{
public:
	virtual ~IEncounterVisibilityQuery() = default;

	virtual bool CanViewerKnow(const APawn* Viewer, const APawn* Pawn) = 0;
};

/** The per-pawn server-only defence state (UDefenceStateComponent in production; a map in tests). */
class IEncounterDefenceStore
{
public:
	virtual ~IEncounterDefenceStore() = default;

	/** The state, or null when the pawn never had a defence write (T_act none, no lockout). Never creates one. */
	virtual FDefenceStateCore* Find(const APawn* Pawn) = 0;
	/** The state, created on the first defence write (an executed Prepare or Attack). */
	virtual FDefenceStateCore* FindOrAdd(const APawn* Pawn) = 0;
};

/** What the resolver asks of the round queue from inside its slot (production: FEncounterQueueAdapter). */
class IEncounterQueue
{
public:
	virtual ~IEncounterQueue() = default;

	/** Schedules a timed item (the Prepare hold timers). */
	virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) = 0;
	/** The owner code the Voided entry carries (an ECommandRejectCode value). */
	virtual void SetVoidCode(uint32 OwnerCode) = 0;
	/** Stages an already-decided presentation write (DeathWrite, CounterPose; story asn-013 owns the applier). */
	virtual bool StageWrite(const FStagedWrite& Write) = 0;
};

/** The action hub's events (UActionHub::OnActionStarted / Cancelled / Completed in production). Optional. */
class IEncounterActionEvents
{
public:
	virtual ~IEncounterActionEvents() = default;

	virtual void OnActionStarted(APawn* Pawn, EActionType Type) = 0;
	virtual void OnActionCancelled(APawn* Pawn, EActionType Type, ECancelReason Reason) = 0;
	virtual void OnActionCompleted(APawn* Pawn, EActionType Type) = 0;
};

/**
 * The planted brace stance (SCL BracePlanted, ASN-R4): on at an executed Prepare(Begin), off when the window's ending
 * item resolves or the pawn's own Attack supersedes it. A stand-in for the FActionState Brace write of story asn-012;
 * a reversing defender keeps it until its window item, then the CounterPose applier (asn-013) pose-locks it. Optional.
 */
class IEncounterBraceStanceSink
{
public:
	virtual ~IEncounterBraceStanceSink() = default;

	virtual void SetBraceStance(APawn* Pawn, bool bBraced) = 0;
};

/** The server-only card ledger's VacateCard(Dead pawn) (ADR-0018 section 2.1 step 4); no kind parameter. Optional. */
class IEncounterCardLedger
{
public:
	virtual ~IEncounterCardLedger() = default;

	virtual void VacateCard(APawn* Pawn) = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Observation records (the IIF reply step and story asn-015's owner results consume them)
// ---------------------------------------------------------------------------------------------------------

/** One command's reply as the slot decided it (the IIF's reply step delivers it at drain end; story asn-015). */
struct FEncounterCommandReply
{
	FQueueKey Key;
	APawn* Pawn = nullptr;
	ECommandKind Kind = ECommandKind::Attack;
	/** Executed (Allow) or voided with Code. */
	bool bExecuted = false;
	ECommandRejectCode Code = ECommandRejectCode::NotNow;
	/** Rejected_Latched: the NotNow reply is held in the held-reply buffer until the round-end step (ADR-0002 8.1). */
	bool bHeld = false;
	FReplySinkId Sink;
	FClientCommandSeq ClientSeq;

	/** The reply as the owner sees it: identical for every target (ASN-R1, story asn-008). */
	bool SameReplyAs(const FEncounterCommandReply& Other) const
	{
		return Kind == Other.Kind && bExecuted == Other.bExecuted && (bExecuted || Code == Other.Code) && bHeld == Other.bHeld
			&& Key.ServerTickIndex == Other.Key.ServerTickIndex;
	}
};

/** One executed Attack, everything its slot decided (stories asn-012, 013, 015 read it). */
struct FAttackResolution
{
	FQueueKey Key;
	APawn* Attacker = nullptr;
	/** The ASN-R8 victim (null for a whiff). On a reversal the pawn that fell is the attacker. */
	APawn* Victim = nullptr;
	EAttackOutcome Outcome = EAttackOutcome::Whiff;
	EVictimRule Rule = EVictimRule::None;
	uint16 BushId = 0;
	EStrikeReachVariant Variant = EStrikeReachVariant::Standard;
	/** F1 for the victim (default for a whiff). */
	FStrikeReachResult Reach;
	/** Execution and contact server times (ContactServerTime = t_exec + T_v, the whiff's nominal contact too). */
	double ExecServerTime = 0.0;
	double ContactServerTime = 0.0;
	/** The attacker's owner result (TallyAfter and bExiled are Penalty's, filled in the slot by story asn-015). */
	FStrikeResult StrikeResult;
	FReplySinkId AttackerSink;
	/** The kind OnPawnKilled carried (Kill: the victim's; Reversal: the attacker's). */
	EVictimKind FallenKind = EVictimKind::Npc;
	/** A kill of a pawn concealed at the slot (FDeathState.bDiedConcealed, stories asn-013 / asn-023). */
	bool bVictimConcealed = false;
	/** Reversal: the defender's Prepare sink (index 0 = the null sink of an AI-driven pawn: no FDefenceResult). */
	FReplySinkId DefenderSink;

	/** True when a bound defender receives FDefenceResult {Reversed} (a reversal with a non-null sink). */
	bool HasDefenceResult() const { return Outcome == EAttackOutcome::Reversal && DefenderSink.Index != 0; }

	/** Field equality of everything a strike decides (the "byte-identical result" checks). */
	bool SameOutcomeAs(const FAttackResolution& Other) const
	{
		return Victim == Other.Victim && Outcome == Other.Outcome && Rule == Other.Rule && BushId == Other.BushId && Variant == Other.Variant
			&& ExecServerTime == Other.ExecServerTime && ContactServerTime == Other.ContactServerTime && StrikeResult.Result == Other.StrikeResult.Result
			&& StrikeResult.ContactServerTime == Other.StrikeResult.ContactServerTime && StrikeResult.TallyAfter == Other.StrikeResult.TallyAfter
			&& StrikeResult.bExiled == Other.StrikeResult.bExiled && FallenKind == Other.FallenKind && bVictimConcealed == Other.bVictimConcealed
			&& Reach.bInReach == Other.Reach.bInReach && Reach.DistanceCm == Other.Reach.DistanceCm && Reach.WarpTranslationEffCm == Other.Reach.WarpTranslationEffCm
			&& Reach.BearingDeg == Other.Reach.BearingDeg;
	}
};
