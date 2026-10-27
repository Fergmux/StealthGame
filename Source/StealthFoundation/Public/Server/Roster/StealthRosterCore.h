#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Roster/StealthRosterDeps.h"
#include "Server/Roster/StealthRosterCallTypes.h"
#include "View/StealthRosterTypes.h"

/**
 * FRosterCore: the plain-C++ Participant State & Roster core (GDD design/gdd/participant-state-roster.md;
 * ADR-0005 section 2 and its Technical Setup 2026-10-25b amendments; ADR-0007 section 5).
 *
 * No UWorld, no UObject, no engine singleton: every input arrives through FRosterCoreDeps, so the whole
 * behaviour is testable headless (StealthGame.Roster.*). The URosterStore game-instance facade (story
 * psr-061, decision D43) adapts engine objects to it.
 *
 * Built by stories psr-001..014: record schema (001), BeginMatch and registration (002), the shared
 * authority/thread precheck (003, 004), the lock (005), atomic team batches (006, 007), the FIFO event
 * queue with EventSeq (008), the connection projection (009), pawn binding, tombstones and release (010,
 * 011, 012), ResolveVictimKind (013) and RequestTransition with the full R8 precedence (014).
 */

// ---------------------------------------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------------------------------------

// ERosterConnection, ERosterReason and ERosterResult live in Server/Roster/StealthRosterCallTypes.h (moved so the
// Round gateways in StealthGameplay can name them; RMF-003 ruling, 2026-10-25).


/**
 * Identifies the calling API in fault-sink reports (the fault's Detail) and in the client-call table of
 * StealthGame.Roster.ClientCallsRejected. Append only.
 */
enum class ERosterCall : uint8
{
	BeginMatch,
	EndMatch,
	SetTeamSize,
	RegisterParticipant,
	RemoveParticipant,
	SetTeamsLocked,
	AssignTeams,
	NotifyConnectionChanged,
	BindPawn,
	ReleaseToCrowd,
	ClearBinding,
	InvalidateAllBindings,
	RequestTransition,
	ResolveVictimKind,
	IsPawnBound,
	GetLegalityClass,
	CountActive,
	CountInformant,
	CountRoster,
	IsTeamEliminated,
	GetPawn,
	GetParticipant,
	GetUnboundActiveParticipants,
	/** Appended for ResetAll (story psr-022 function, landed early for the D53 integration batch). */
	ResetAll,
	/** Appended for RetireBinding (story psr-020, landed with NPC-Kill Penalty story nkp-008). */
	RetireBinding,
};

/** Status of a row of the pawn-binding table; a pawn with no row is Unbound (an NPC, R9). */
enum class ERosterBindingStatus : uint8
{
	BoundLive,
	Retired,
};

/** Core-side mirror of the engine EEndPlayReason values ClearBinding distinguishes (R9), so the core includes no engine header. */
enum class ERosterEndPlayReason : uint8
{
	Destroyed,
	LevelTransition,
	EndPlayInEditor,
	RemovedFromWorld,
	Quit,
};

/** ResolveVictimKind's kind (R10). */
enum class ERosterVictimKind : uint8
{
	Player,
	Informant,
	Npc,
	NotTargetable,
};

/** Why a pawn is NotTargetable (R10). None for the three targetable kinds. */
enum class ERosterNotTargetableReason : uint8
{
	None,
	AlreadyResolved,
	Stale,
	RoundOver,
};

/** Roster event types (R12). Every emitted event of any type consumes exactly one EventSeq (I6). Append only. */
enum class ERosterEventType : uint8
{
	ParticipantRegistered,
	ParticipantReclaimed,
	ParticipantRemoved,
	TeamsChanged,
	ConnectionChanged,
	ParticipantStateChanged,
	BindingLost,
	RosterRoundEnded,
	RosterReset,
};

/** OnBindingLost cause (R12). */
enum class ERosterBindingLostCause : uint8
{
	PawnDestroyed,
};

// ---------------------------------------------------------------------------------------------------------
// The record (R3) and its enumerable field list (AC-PSR-01)
// ---------------------------------------------------------------------------------------------------------

/** Name and declared type of one record field, emitted by the field-list macro (no hand-kept second list). */
struct FRosterFieldInfo
{
	const TCHAR* Name;
	const TCHAR* Type;
};

/**
 * The Connection projection fields (R3 `Connection` with ServerOnly `DisconnectedAt` and
 * `ConnectionGeneration`). X(Type, Name, Default).
 */
#define STEALTH_ROSTER_CONNECTION_FIELDS(X) \
	X(ERosterConnection, Projection, ERosterConnection::Connected) \
	X(int64, DisconnectedAtMs, 0) \
	X(uint32, ConnectionGeneration, 0)

/**
 * The per-record fields of R3, exactly (R1: no strike, defence, concealment or objective field). Roster-wide
 * values (MatchId, RoundEpoch, EventSeq) live on FRosterCore, not here; the owner-view cache of R6 is not
 * part of the record. PawnBinding is the record's row of the pawn binding (its live pawn; tombstones live in
 * the core's binding table). X(Type, Name, Default). StealthGame.Roster.RecordSchemaWhitelist pins this list.
 */
#define STEALTH_ROSTER_RECORD_FIELDS(X) \
	X(FParticipantId, ParticipantId, FParticipantId()) \
	X(uint32, PublicHandle, 0) \
	X(FIdentityKey, IdentityKey, FIdentityKey()) \
	X(FString, DisplayName, FString()) \
	X(ERosterTeam, Team, ERosterTeam::None) \
	X(FRosterConnection, Connection, FRosterConnection()) \
	X(TArray<int32>, AppearanceHistory, TArray<int32>()) \
	X(bool, bInRound, false) \
	X(EParticipantState, State, EParticipantState::RoundEnded) \
	X(ERosterReason, LastTransitionReason, ERosterReason::None) \
	X(uint64, LastEventSeq, 0) \
	X(int32, AssignedAppearanceId, INDEX_NONE) \
	X(FRosterPawnHandle, PawnBinding, FRosterPawnHandle())

#define STEALTH_ROSTER_DECLARE_FIELD(Type, Name, Default) Type Name = Default;
#define STEALTH_ROSTER_FIELD_INFO(Type, Name, Default) FRosterFieldInfo{ TEXT(#Name), TEXT(#Type) },
#define STEALTH_ROSTER_FIELD_EQUAL(Type, Name, Default) && Name == Other.Name

/** The Connection projection of one record. Written only by NotifyConnectionChanged (Session is the sole caller, R11). */
struct FRosterConnection
{
	STEALTH_ROSTER_CONNECTION_FIELDS(STEALTH_ROSTER_DECLARE_FIELD)

	/** The fields above, in declaration order. */
	static TConstArrayView<FRosterFieldInfo> GetFieldInfo();

	bool operator==(const FRosterConnection& Other) const { return true STEALTH_ROSTER_CONNECTION_FIELDS(STEALTH_ROSTER_FIELD_EQUAL); }
	bool operator!=(const FRosterConnection& Other) const { return !(*this == Other); }
};

/** One human participant's record (R3). Authoritative only inside FRosterCore. */
struct FParticipantRecord
{
	STEALTH_ROSTER_RECORD_FIELDS(STEALTH_ROSTER_DECLARE_FIELD)

	/** The fields above, in declaration order. */
	static TConstArrayView<FRosterFieldInfo> GetFieldInfo();

	/** True when the participant has a live (BoundLive) pawn. */
	bool HasLivePawn() const { return PawnBinding.IsSet(); }

	bool operator==(const FParticipantRecord& Other) const { return true STEALTH_ROSTER_RECORD_FIELDS(STEALTH_ROSTER_FIELD_EQUAL); }
	bool operator!=(const FParticipantRecord& Other) const { return !(*this == Other); }
};

// ---------------------------------------------------------------------------------------------------------
// Call shapes
// ---------------------------------------------------------------------------------------------------------

/** The epoch token (R8): (MatchId, RoundEpoch). */
struct FRosterEpochToken
{
	uint64 MatchId = 0;
	uint32 RoundEpoch = 0;

	bool operator==(const FRosterEpochToken& Other) const { return MatchId == Other.MatchId && RoundEpoch == Other.RoundEpoch; }
	bool operator!=(const FRosterEpochToken& Other) const { return !(*this == Other); }
};

/** RegisterParticipant's answer (R4): a result code and, for Registered or Reclaimed, the id. */
struct FRosterRegisterResult
{
	ERosterResult Result = ERosterResult::Rejected_NotAuthority;
	FParticipantId Id;
};

/** One entry of an AssignTeams batch (R5). Team is an int so an out-of-range team (2) is representable and rejected. */
struct FRosterTeamAssignment
{
	FParticipantId Id;
	int32 Team = 0;
};

/** Optional arguments of RequestTransition (R8). */
struct FRosterTransitionOptions
{
	/** (MatchId, RoundEpoch) captured by the caller; a mismatch is Rejected_StaleEpoch. */
	TOptional<FRosterEpochToken> ExpectedEpoch;
	/** Required for Reason Disconnect: the ConnectionGeneration captured when the grace timer started. */
	TOptional<uint32> ExpectedConnectionGeneration;
	/** Server time (ms) the retire or release writes for teammates (R8); defaults to the call's server time. */
	TOptional<int64> RetireAtServerTimeMs;
};

/** ResolveVictimKind's answer (R10). Server-only; never sent to a client. */
struct FRosterVictimKind
{
	ERosterVictimKind Kind = ERosterVictimKind::NotTargetable;
	ERosterNotTargetableReason Reason = ERosterNotTargetableReason::Stale;

	static FRosterVictimKind Make(ERosterVictimKind InKind) { return { InKind, ERosterNotTargetableReason::None }; }
	static FRosterVictimKind NotTargetable(ERosterNotTargetableReason InReason) { return { ERosterVictimKind::NotTargetable, InReason }; }

	bool operator==(const FRosterVictimKind& Other) const { return Kind == Other.Kind && Reason == Other.Reason; }
	bool operator!=(const FRosterVictimKind& Other) const { return !(*this == Other); }
};

/** One row of the pawn-binding table (R9). Rows exist only for BoundLive and Retired pawns. */
struct FRosterBindingEntry
{
	ERosterBindingStatus Status = ERosterBindingStatus::BoundLive;
	FParticipantId Participant;
	/** Written when the row becomes Retired: the RetireAtServerTime teammates' C-TeamView entry carries (R8, R9). */
	int64 RetireAtServerTimeMs = 0;

	bool operator==(const FRosterBindingEntry& Other) const
	{
		return Status == Other.Status && Participant == Other.Participant && RetireAtServerTimeMs == Other.RetireAtServerTimeMs;
	}
	bool operator!=(const FRosterBindingEntry& Other) const { return !(*this == Other); }
};

/**
 * One Roster event (R12): a flat payload tagged by Type, so later event kinds extend the enum, not a fixed
 * switch. Fields a type does not use keep their defaults.
 */
struct FRosterEvent
{
	ERosterEventType Type = ERosterEventType::ParticipantRegistered;
	uint64 EventSeq = 0;
	uint32 RoundEpoch = 0;
	/** The participant (not set for TeamsChanged and the batch events). */
	FParticipantId Id;
	/** ParticipantStateChanged: the participant's team. */
	ERosterTeam Team = ERosterTeam::None;
	/** ParticipantStateChanged: old and new state and the reason. */
	EParticipantState Old = EParticipantState::RoundEnded;
	EParticipantState New = EParticipantState::RoundEnded;
	ERosterReason Reason = ERosterReason::None;
	/** ParticipantStateChanged: F1 and F2 snapshots taken at emission, per team. */
	int32 ActiveCountAfter[2] = { 0, 0 };
	int32 InformantCountAfter[2] = { 0, 0 };
	/** ConnectionChanged: the new projection. */
	ERosterConnection Connection = ERosterConnection::Connected;
	/** ConnectionChanged: Session flagged the change as a host kick (selects the C-TeamView clear cause, ruling D23). */
	bool bKicked = false;
	/** BindingLost: the cause. */
	ERosterBindingLostCause Cause = ERosterBindingLostCause::PawnDestroyed;
};

/**
 * Server-side Roster event listener (R12). Listeners act on applied events only and must be idempotent per
 * EventSeq. A listener may call back into the core: the call sees live state and its own events queue behind
 * the current ones (FIFO).
 */
class IRosterEventListener
{
public:
	virtual ~IRosterEventListener() = default;

	virtual void OnRosterEvent(const FRosterEvent& Event) = 0;
};

/** A value copy of the whole core state, for "nothing changed" comparisons (story psr-003). */
struct FRosterSnapshot
{
	/** Indexed by ParticipantId.Value; unset for a removed id. */
	TArray<TOptional<FParticipantRecord>> Records;
	/** Binding rows sorted by (ObjectKey, Generation). */
	TArray<TPair<FRosterPawnHandle, FRosterBindingEntry>> Bindings;
	uint64 MatchId = 0;
	uint32 RoundEpoch = 0;
	uint64 EventSeq = 0;
	int32 TeamSize = 0;
	bool bTeamsLocked = false;

	bool operator==(const FRosterSnapshot& Other) const
	{
		return Records == Other.Records && Bindings == Other.Bindings && MatchId == Other.MatchId && RoundEpoch == Other.RoundEpoch
			&& EventSeq == Other.EventSeq && TeamSize == Other.TeamSize && bTeamsLocked == Other.bTeamsLocked;
	}
	bool operator!=(const FRosterSnapshot& Other) const { return !(*this == Other); }
};

/** R4 TeamSize hard range. Rule constants of the GDD (R4), not tuning: TeamSize itself is injected by the caller. */
inline constexpr int32 RosterMinTeamSize = 1;
inline constexpr int32 RosterMaxTeamSize = 5;

/** Fault-sink source name of every Roster report. */
inline FName RosterFaultSource() { return FName(TEXT("Roster")); }

// ---------------------------------------------------------------------------------------------------------
// FRosterCore
// ---------------------------------------------------------------------------------------------------------

/**
 * The Roster store core. Server-only: every mutator runs the shared precheck (authority, then game thread)
 * first; every server query checks authority and answers the non-revealing client value on a client
 * (StealthGame.Roster.ClientCallsRejected pins the per-call table):
 *
 *   mutators (every call returning ERosterResult)    -> Rejected_NotAuthority
 *   RegisterParticipant                              -> { Rejected_NotAuthority, invalid id }
 *   ResolveVictimKind                                -> NotTargetable(Stale)
 *   IsPawnBound, IsTeamEliminated                    -> false
 *   CountActive, CountInformant, CountRoster         -> 0
 *   GetLegalityClass                                 -> Active (the Unbound answer, so it reveals no binding, R10)
 *   GetPawn                                          -> null handle;  GetParticipant -> invalid id
 *   GetUnboundActiveParticipants                     -> empty
 *
 * and each such call reports exactly one fault (RosterNotAuthority, Detail = ERosterCall). Never Npc, never a
 * real count on a client (R2, R10).
 *
 * State is written immediately; events are queued with their EventSeq at emission and dispatched FIFO after
 * the outermost mutating call (R12, I6). Rejections and no-ops emit nothing and consume no EventSeq.
 */
class STEALTHFOUNDATION_API FRosterCore
{
public:
	/** Every dependency pointer must be non-null and outlive the core. */
	explicit FRosterCore(const FRosterCoreDeps& InDeps);

	FRosterCore(const FRosterCore&) = delete;
	FRosterCore& operator=(const FRosterCore&) = delete;

	// --- Match lifetime (R4; ADR-0005 section 2) -----------------------------------------------------------

	/**
	 * Clears the store, draws a new MatchId, resets ParticipantId, EventSeq and RoundEpoch to 0 and unlocks
	 * teams. TeamSize is injected by the caller (Round owns the key). Outside 1-5: Rejected_InvalidTeamSize.
	 */
	ERosterResult BeginMatch(int32 InTeamSize);

	/** Clears the store on a session exit path (ADR-0005 section 2): no record, binding or match id survives. */
	ERosterResult EndMatch();

	/**
	 * Changes TeamSize (R4). Locked: Rejected_TeamsLocked (AC-PSR-06); outside 1-5: Rejected_InvalidTeamSize;
	 * below the largest team's non-Dormant count: Rejected_TeamSizeBelowRoster.
	 */
	ERosterResult SetTeamSize(int32 InTeamSize);

	// --- Registration and teams (R4, R5) ---------------------------------------------------------------------

	/**
	 * Registers or reclaims by key (R4). Check order: NotAuthority, WrongThread, reclaim match, TeamsLocked,
	 * RosterFull. A new record is RoundEnded / None / Connected and gets a fresh random PublicHandle.
	 */
	FRosterRegisterResult RegisterParticipant(const FIdentityKey& Key, const FString& DisplayName);

	/** Removes a record while teams are unlocked (Lobby kick, EndMatch). The id is retired, never reused. */
	ERosterResult RemoveParticipant(FParticipantId Id);

	/**
	 * Locks or unlocks teams (R5). Locking while a non-Dormant record has Team None is
	 * Rejected_UnassignedParticipants; locking when locked (or unlocking when unlocked) is NoOp_Unchanged.
	 */
	ERosterResult SetTeamsLocked(bool bLocked);

	/**
	 * Applies a team batch atomically, validated against the final state (R5, F5). Order: TeamsLocked,
	 * UnknownParticipant, InvalidBatch (empty, duplicate id, team not 0/1), TeamFull. One OnTeamsChanged.
	 */
	ERosterResult AssignTeams(TConstArrayView<FRosterTeamAssignment> Batch);

	/** Pure validation of AssignTeams: no side effect, no fault report (precheck excluded). */
	ERosterResult ValidateTeamBatch(TConstArrayView<FRosterTeamAssignment> Batch) const;

	// --- Connection projection (R11) ----------------------------------------------------------------------------

	/**
	 * Session's projection notify (R11). Changes only Connection, DisconnectedAtMs and ConnectionGeneration;
	 * the generation increases by 1 only on entry to Disconnected (decision D45), where DisconnectedAtMs is
	 * written. A notify that does not change the projection is NoOp_Unchanged and emits nothing. bKicked is
	 * accepted for the C-TeamView reconnect clear cause (ruling D23) and changes nothing in the core.
	 */
	ERosterResult NotifyConnectionChanged(FParticipantId Id, ERosterConnection Projection, int64 ServerTimeMs, bool bKicked = false);

	// --- Pawn binding (R9) ---------------------------------------------------------------------------------------

	/**
	 * Binds Pawn to Id (R9). Precedence: NotAuthority, WrongThread, UnknownParticipant, StaleEpoch, WrongState
	 * (not Active/Informant), AlreadyBound (has a live pawn), PawnInUse (bound, retired, null, destroyed or
	 * pending kill), Applied. Emits no event.
	 */
	ERosterResult BindPawn(FParticipantId Id, const FRosterPawnHandle& Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch = {});

	/**
	 * Erases a live or retired binding so the pawn becomes Unbound (R9). Rejected_NotDisconnected when the
	 * bound participant is Connected and not RoundEnded; NoOp_NotBound for an Unbound pawn. Emits no event.
	 */
	ERosterResult ReleaseToCrowd(const FRosterPawnHandle& Pawn);

	/**
	 * Retires a live binding without a lifecycle transition (R9, R10; story psr-020; decision D10): the informant-kill
	 * call NPC-Kill Penalty makes inline in the killing Attack's slot. BoundLive -> Retired (a tombstone keyed by the
	 * handle's object key and generation, so a recycled pawn never inherits it) with RetireAtServerTimeMs, and the
	 * participant's live pawn is cleared, so AwaitingPawn(T) rises by one while the state, InformantCount(T) and EventSeq
	 * are unchanged: no event is emitted (nothing observable differs from an NPC kill). Reason is accepted for the
	 * ServerOnly log (InformantKilled for an informant kill) and stored nowhere else: no replicated field, no record field.
	 * Wrong inputs (R9): an Unbound pawn -> NoOp_NotBound; a Retired pawn -> NoOp_AlreadyRetired; neither changes anything.
	 */
	ERosterResult RetireBinding(const FRosterPawnHandle& Pawn, int64 RetireAtServerTimeMs, ERosterReason Reason = ERosterReason::InformantKilled);

	/**
	 * Pawn EndPlay hook (R9). Ends a live binding or a tombstone. A live binding cleared with Destroyed during
	 * Round phase Active reports RosterLiveBindingCleared; for an Active participant a Destroyed clear raises
	 * OnBindingLost(PawnDestroyed). Teardown reasons clear silently. NoOp_NotBound for an Unbound pawn.
	 */
	ERosterResult ClearBinding(const FRosterPawnHandle& Pawn, ERosterEndPlayReason Reason);

	/** Session's OnTravelStarting (R9): clears every live binding and tombstone, no lifecycle state, no event. */
	ERosterResult InvalidateAllBindings();

	// --- Round scope (R7) ------------------------------------------------------------------------------------

	/**
	 * The once-per-round batch reset (R7; story psr-022 function, landed early for the D53 integration batch so
	 * Round's Start ordering can call the real Roster). Teams must be locked (else Rejected_TeamsUnlocked). Every
	 * non-Dormant record becomes Active with InRound true; every Dormant record RoundEnded with InRound false;
	 * bindings, tombstones, LastTransitionReason and AssignedAppearanceId clear; Team and Connection are kept;
	 * RoundEpoch +1 and RoundIndex is recorded. Emits exactly one OnRosterReset and no per-participant event.
	 * A repeat with the same RoundIndex (after an aborted start) applies in full again.
	 */
	ERosterResult ResetAll(uint32 RoundIndex);

	/** True when ResetAll(RoundIndex) is the last round-scope reset of the match (R7). */
	bool IsRoundScopeReset(uint32 RoundIndex) const { return bRoundScopeReset && LastResetRoundIndex == RoundIndex; }

	// --- Transitions (R8) -------------------------------------------------------------------------------------------

	/**
	 * The only writer of State, LastTransitionReason and LastEventSeq (ADR-0005 2026-10-25b). Applies the R8
	 * precedence in order: NotAuthority, WrongThread, UnknownParticipant, StaleEpoch, BatchOnlyReason,
	 * IllegalReason, NoOp_AlreadyInState, IllegalTransition, NotDisconnected, StaleConnection, Applied.
	 * Applied Active -> Informant retires the live pawn with RetireAtServerTime, except StrikeLimit, which
	 * releases it (decision D42: the pawn resolves Npc). Emits one OnParticipantStateChanged.
	 */
	ERosterResult RequestTransition(FParticipantId Id, EParticipantState NewState, ERosterReason Reason, const FRosterTransitionOptions& Options = {});

	/** True when Reason is an entering reason for NewState (R8 step 6). */
	static bool IsEnteringReason(EParticipantState NewState, ERosterReason Reason);

	/** True when From -> To is a legal single-participant transition (R8 step 8; the legal-transition table). */
	static bool IsLegalSingleTransition(EParticipantState From, EParticipantState To);

	// --- Queries (R10). Server-only; client answers in the class comment. ------------------------------------

	/** "NPC or player?" in the fixed R10 order: Stale, AlreadyResolved, RoundOver, Player/Informant, Npc. Never Npc for a NotTargetable input. */
	FRosterVictimKind ResolveVictimKind(const FRosterPawnHandle& Pawn) const;

	/** True only for a BoundLive pawn (the Visibility Policy's IsPlayer fact). */
	bool IsPawnBound(const FRosterPawnHandle& Pawn) const;

	/** Action-legality class (R10): a BoundLive or Retired pawn answers its participant's state; an Unbound pawn answers Active. */
	EParticipantState GetLegalityClass(const FRosterPawnHandle& Pawn) const;

	/** F1: InRound records of Team in Active. */
	int32 CountActive(int32 Team) const;
	/** F2: InRound records of Team in Informant. */
	int32 CountInformant(int32 Team) const;
	/** F3: InRound records of Team; before the first ResetAll of the match, non-Dormant records of Team. */
	int32 CountRoster(int32 Team) const;
	/** F4: RosterCount >= 1 and ActiveCount = 0, only in Round phase Active (false otherwise). */
	bool IsTeamEliminated(int32 Team) const;

	/** The participant's live pawn, or a null handle. */
	FRosterPawnHandle GetPawn(FParticipantId Id) const;

	/** The participant a BoundLive or Retired pawn belongs to, or an invalid id. */
	FParticipantId GetParticipant(const FRosterPawnHandle& Pawn) const;

	/** Round's pre-Active check (R10, I4): Active, InRound participants with no live pawn. Cold path. */
	void GetUnboundActiveParticipants(TArray<FParticipantId>& OutIds) const;

	// --- Server-internal reads (no authority check: the facade exposes typed queries) -------------------------

	/** The record of Id, or nullptr for an unknown or removed id. */
	const FParticipantRecord* FindRecord(FParticipantId Id) const;
	/** Binding row of Pawn, or nullptr when Unbound. */
	const FRosterBindingEntry* FindBinding(const FRosterPawnHandle& Pawn) const;

	/** Every live (not removed) participant id, ascending (cold path: dumps, the C-TeamView push step). */
	void GetParticipantIds(TArray<FParticipantId>& OutIds) const;

	/**
	 * The Retired row of Id with the latest RetireAtServerTime (the C-TeamView retiring update, story psr-062).
	 * Cold path: scans the binding table. False when Id has no Retired row.
	 */
	bool FindLatestRetiredPawn(FParticipantId Id, FRosterPawnHandle& OutPawn, int64& OutRetireAtServerTimeMs) const;

	uint64 GetMatchId() const { return MatchId; }
	uint32 GetRoundEpoch() const { return RoundEpoch; }
	uint64 GetEventSeq() const { return EventSeq; }
	int32 GetTeamSize() const { return TeamSize; }
	bool AreTeamsLocked() const { return bTeamsLocked; }
	FRosterEpochToken GetEpochToken() const { return { MatchId, RoundEpoch }; }

	/** A value copy of the whole state (records, bindings, counters) for "nothing changed" comparisons. */
	FRosterSnapshot Snapshot() const;

	// --- Listeners (R12) ---------------------------------------------------------------------------------------------

	/** Registers a server-side listener. The pointer must stay valid until RemoveListener. */
	void AddListener(IRosterEventListener* Listener);
	void RemoveListener(IRosterEventListener* Listener);

#if WITH_STEALTH_TEST_SEAMS
	// --- Core test seams (ADR-0008: compiled only under WITH_STEALTH_TEST_SEAMS, never into Shipping) ----------
	// Fixtures for states with no production path yet: Active and InRound before ResetAll (story psr-022) and
	// Retired rows before RetireBinding (story psr-020). They write directly and emit nothing.

	/** Sets a record's lifecycle state and InRound flag. */
	void SeamSetState(FParticipantId Id, EParticipantState NewState, bool bInRound = true);
	/** Writes a binding row directly (a Retired row does not touch the record's live pawn). */
	void SeamSetBinding(const FRosterPawnHandle& Pawn, FParticipantId Id, ERosterBindingStatus Status, int64 RetireAtServerTimeMs = 0);
#endif

private:
	/** RAII outermost-call tracker: the event queue drains when the outermost mutating call returns. */
	struct FCallScope
	{
		explicit FCallScope(FRosterCore& InCore);
		~FCallScope();
		FRosterCore& Core;
	};

	/**
	 * Shared mutator precheck (stories psr-003/004): authority (R8 step 1), then game thread (step 2). Applied when
	 * both pass. A wrong-thread call reports from the calling thread, so the production fault sink must accept
	 * reports from any thread.
	 */
	ERosterResult CheckCallPreconditions(ERosterCall Call) const;

	/** Query precheck: false (after one fault report) on a client. */
	bool CheckQueryAuthority(ERosterCall Call) const;

	FParticipantRecord* FindRecordMutable(FParticipantId Id);
	/**
	 * The reclaim seam of RegisterParticipant (R4 step "reclaim match"; story psr-005): false when the key is
	 * unknown; otherwise fills Out with Reclaimed or, for a Dormant record while locked, Rejected_TeamsLocked.
	 * Story psr-029 extends the restoration semantics here.
	 */
	bool TryReclaim(const FIdentityKey& Key, FRosterRegisterResult& Out);
	FParticipantId FindIdByKey(const FIdentityKey& Key) const;

	/** Number of non-Dormant records (R4 capacity: 2 x TeamSize). */
	int32 CountNonDormant() const;
	/** Number of non-Dormant records with Team, optionally skipping one id. */
	int32 CountNonDormantOnTeam(ERosterTeam Team, FParticipantId Skip = FParticipantId()) const;
	int32 CountInState(int32 Team, EParticipantState InState) const;
	int32 CountRosterUnchecked(int32 Team) const;

	uint32 DrawPublicHandle();
	uint64 DrawMatchId();
	int64 NowMs() const;
	void ClearState();

	/** Retires the participant's live pawn (BoundLive -> Retired) and clears its live pawn. */
	void RetireLivePawnInternal(FParticipantRecord& Record, int64 RetireAtServerTimeMs);
	/** Erases Pawn's row (live or retired) and clears the owner's live pawn if it was this pawn. No precheck, no connection rule (R8 StrikeLimit path). Takes the handle by value: callers pass the record's own PawnBinding, which this clears. */
	void ReleaseBindingInternal(FRosterPawnHandle Pawn);

	/** Active -> Informant pawn disposition (R8): retire the live pawn, or release it for StrikeLimit (D42). */
	void DisposeLivePawnOnElimination(FParticipantRecord& Record, ERosterReason Reason, const TOptional<int64>& RetireAtServerTimeMs);
	/** Queues OnParticipantStateChanged for Record (already written) with the F1/F2 snapshots; returns its EventSeq. */
	uint64 EmitStateChanged(const FParticipantRecord& Record, EParticipantState Old);
	/** Queues an event with the next EventSeq and returns that EventSeq. */
	uint64 EmitEvent(FRosterEvent Event);
	void FlushEvents();

	static ERosterTeam TeamFromInt(int32 Team);
	static int32 TeamIndex(ERosterTeam Team);

	FRosterCoreDeps Deps;

	/** Records indexed by ParticipantId.Value; unset once removed (ids are never reused). */
	TArray<TOptional<FParticipantRecord>> Records;
	TMap<FIdentityKey, FParticipantId> IdByKey;
	TSet<uint32> IssuedHandles;
	/** The pawn-binding table (R9): one row per BoundLive or Retired pawn, keyed by object identity plus generation. */
	TMap<FRosterPawnHandle, FRosterBindingEntry> PawnTable;

	uint64 MatchId = 0;
	uint32 RoundEpoch = 0;
	uint64 EventSeq = 0;
	int32 TeamSize = 0;
	bool bTeamsLocked = false;
	/** R7: the RoundIndex of the last ResetAll of the match (valid when bRoundScopeReset). */
	uint32 LastResetRoundIndex = 0;
	bool bRoundScopeReset = false;
	uint32 NextParticipantId = 0;
	uint64 HandleDrawCounter = 0;
	/** Lifetime counter of MatchId draws: survives BeginMatch so two matches never share an attempt. */
	uint64 MatchDrawCounter = 0;

	TArray<IRosterEventListener*> Listeners;
	TArray<FRosterEvent> PendingEvents;
	int32 CallDepth = 0;
	bool bFlushing = false;
};
