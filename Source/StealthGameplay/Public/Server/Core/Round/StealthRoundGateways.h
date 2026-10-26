#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Roster/StealthRosterCallTypes.h"
#include "Server/StealthQueuePayloads.h"
#include "UObject/Interface.h"
#include "View/StealthQueueTypes.h"
#include "View/StealthRosterTypes.h"

#include "StealthRoundGateways.generated.h"

class ICrowdControl;

/**
 * Round's gateways to the systems it calls (Round GDD R1, R2, R5, R6 and "Interactions with Other Systems";
 * ADR-0002 sections 8.6-8.8; ADR-0007 consumer-owned interfaces).
 *
 * RMF-003 ruling (Technical Director call delegated to the RMF Exit A build, 2026-10-25; decision log and ADR-0002
 * Amendments): Round decides and six systems act. Round lives in StealthGameplay Core, so Round owns these
 * interfaces here (ADR-0007: the consumer declares the interface; Foundation cannot include StealthGameplay). Each
 * producing system implements its gateway (a thin adapter over its own facade) and the composition root
 * registers it weakly, which is why they are UINTERFACEs; Round's plain cores take the raw pointers bundled in
 * FRoundGateways, so headless tests inject the recording fakes of Server/Testing/StealthRoundFakeGateways.h with
 * no singleton lookup. The signatures name only Foundation types (ids, epochs, pawn handles, Roster result
 * codes); the producing ADRs own the meaning of each call. Method sets are the calls Round's GDD rules make
 * today; later Round stories extend them by amendment, never by a side channel.
 *
 * ICrowdControl (the sixth gateway) is declared in StealthCrowdControl.h exactly as ADR-0016 states it.
 */

// ---------------------------------------------------------------------------------------------------------
// Session & Travel
// ---------------------------------------------------------------------------------------------------------

/** Session admission modes Round pushes on every phase change (Round R4; Session rule 20). */
enum class ERoundAdmissionMode : uint8
{
	Open,
	RejoinOnly,
	Closed,
};

/** Session's connection state of a participant, as Round reads it (Round R5 step 3, R6 P5/P5b). */
enum class ERoundConnectionState : uint8
{
	Connecting,
	Connected,
	Unresponsive,
	Disconnected,
	Dormant,
};

/** Session's answer to RequestRoundTravel (Round R5 steps 8-9). */
enum class ERoundTravelRequestResult : uint8
{
	Accepted,
	Refused,
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URoundSessionGateway : public UInterface
{
	GENERATED_BODY()
};

/** Round -> Session (Session owns every call; Round owns every consequence). */
class STEALTHGAMEPLAY_API IRoundSessionGateway
{
	GENERATED_BODY()

public:
	/** R5 (5b): a side-effect-free "could a round travel start now" query (provisional Session interface). */
	virtual bool CanRequestRoundTravel() const = 0;

	/** R5 (8): request the round travel; Session draws a fresh seed for every start. */
	virtual ERoundTravelRequestResult RequestRoundTravel(FName MapId, FName PresetId, uint32 RoundIndex) = 0;

	/** R8: return everyone to the lobby. */
	virtual void RequestLobbyTravel() = 0;

	/** R4: the admission mode for the phase being entered. */
	virtual void SetAdmissionMode(ERoundAdmissionMode Mode) = 0;

	/** R5 step 3, R6 P5b: a participant's connection state. */
	virtual ERoundConnectionState GetConnectionState(FParticipantId Participant) const = 0;

	/** R22 host kick (Disconnected(Kicked)). */
	virtual void KickParticipant(FParticipantId Participant) = 0;

	/** ADR-0006: a server sub-seed (SpawnOrder at P6; the queue's SeqOrder/TieRank use the same source). */
	virtual uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const = 0;

	/**
	 * R6 P5 / R17: the participant's current Session ConnectionGeneration (bumped by every rejoin and supersede). A
	 * round acknowledgement delivered under an older generation came from a replaced controller and is dropped
	 * (story rmf-033). Amendment of the RMF Exit B Preparing batch (stories rmf-030..047).
	 */
	virtual uint32 GetConnectionGeneration(FParticipantId Participant) const = 0;

	/**
	 * R22 / Edge Cases: a host abort while the round travel has not reported P1 asks Session to cancel the travel, so
	 * the watchdog is not the only exit (story rmf-059). Session answers with NotifyRoundTravelCancelled on Round.
	 * Amendment of the RMF round-end batch (stories rmf-048..065).
	 */
	virtual void CancelRoundTravel() = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Participant State & Roster
// ---------------------------------------------------------------------------------------------------------

/** One participant's team for AssignTeams (Round R3: one atomic batch). */
struct FRoundTeamAssignment
{
	FParticipantId Participant;
	ERosterTeam Team = ERosterTeam::None;
};

/** One rostered participant as Round reads it (Round R3 counts, R5 checks 3-4): id and team, Dormant included. */
struct FRoundRosterMember
{
	FParticipantId Participant;
	ERosterTeam Team = ERosterTeam::None;
	/** The Roster state (F2 ConnectedActiveCount and the objective Valid(i) check read it). Amendment, stories rmf-043..047. */
	EParticipantState State = EParticipantState::RoundEnded;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URoundRosterGateway : public UInterface
{
	GENERATED_BODY()
};

/** Round -> Roster (the Roster defines the calls; Round decides when and in which order). */
class STEALTHGAMEPLAY_API IRoundRosterGateway
{
	GENERATED_BODY()

public:
	virtual ERosterResult BeginMatch(int32 TeamSize) = 0;
	virtual ERosterResult SetTeamSize(int32 TeamSize) = 0;
	virtual ERosterResult SetTeamsLocked(bool bLocked) = 0;
	virtual ERosterResult AssignTeams(TConstArrayView<FRoundTeamAssignment> Assignments) = 0;
	virtual ERosterResult RemoveParticipant(FParticipantId Participant) = 0;
	virtual ERosterResult ResetAll(uint32 RoundIndex) = 0;
	virtual ERosterResult EndRoundAll() = 0;

	/** R17.4 / ADR-0002 section 8.7: the guarded transition (epoch, and the generation for Disconnect). */
	virtual ERosterResult RequestTransition(FParticipantId Participant, EParticipantState NewState, ERosterReason Reason,
		const FEpochToken& ExpectedEpoch, TOptional<uint32> ExpectedConnectionGeneration) = 0;

	virtual ERosterResult BindPawn(FParticipantId Participant, const FPawnHandle& Pawn) = 0;
	virtual ERosterResult ReleaseToCrowd(const FPawnHandle& Pawn) = 0;

	/** R7: Roster I4 check immediately before Active entry. */
	virtual void GetUnboundActiveParticipants(TArray<FParticipantId>& OutParticipants) const = 0;

	/** R6 P6: copy the allocator's AssignedAppearanceId to the Roster field. */
	virtual ERosterResult RecordAppearance(FParticipantId Participant, uint16 AppearanceId) = 0;

	/** The current epoch token (MatchId, RoundEpoch). */
	virtual FEpochToken GetEpochToken() const = 0;

	/**
	 * Every rostered participant with its team, Dormant records included (a Dormant slot is freed only by remove, R3),
	 * in ParticipantId order. Round's team counts (R3) and Start checks (R5) read it. Amendment of the D53 integration
	 * batch (stories rmf-018..022): the RMF-003 interface named no count query.
	 */
	virtual void GetRosteredParticipants(TArray<FRoundRosterMember>& OutMembers) const = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Tuning Data & Config
// ---------------------------------------------------------------------------------------------------------

/** TDC's dry-run of a requested preset (R5 step 5, provisional PreviewPreset). */
enum class ERoundPresetPreview : uint8
{
	Valid,
	Invalid,
	/** No dry-run available: the pre-check is skipped and Preparing catches the fault later. */
	Unavailable,
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URoundTuningGateway : public UInterface
{
	GENERATED_BODY()
};

/** Round -> TDC. OnTuningReady / Failed arrive as events on Round, not as gateway calls. */
class STEALTHGAMEPLAY_API IRoundTuningGateway
{
	GENERATED_BODY()

public:
	/** R6 P2: "Preparing entered"; TDC freezes and later raises OnTuningReady(TuningHash) or Failed. */
	virtual void SignalPreparingEntered() = 0;

	/** R5 step 5 best-effort pre-check. */
	virtual ERoundPresetPreview PreviewPreset(FName PresetId) const = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Character Pool & Appearance
// ---------------------------------------------------------------------------------------------------------

/** AllocateAppearances results Round acts on (R6 P3). */
enum class ERoundAllocateResult : uint8
{
	Ok,
	DeckInvalid,
	Error_DeckTooSmall,
	Error_AlreadyDealt,
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URoundCharacterPoolGateway : public UInterface
{
	GENERATED_BODY()
};

/** Round -> Character Pool (Round is the caller). */
class STEALTHGAMEPLAY_API IRoundCharacterPoolGateway
{
	GENERATED_BODY()

public:
	/** R6 P3, once per round: Players = every rostered participant, connected or not. */
	virtual ERoundAllocateResult AllocateAppearances(TConstArrayView<FParticipantId> Players, int32 NpcCount) = 0;

	/** R6 P6: the participant's card from the allocator's map (0 = none). */
	virtual uint16 GetAssignedAppearance(FParticipantId Participant) const = 0;

	/** R10 Reset: discard the deal. */
	virtual void DiscardDeal() = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Level Socket Contract
// ---------------------------------------------------------------------------------------------------------

/** One zone of the start placement (GDD F5 ZoneShare_z): players and NPCs placed within the zone radius. */
struct FRoundPlacementZone
{
	int32 Players = 0;
	int32 Npcs = 0;
};

/** RequestStartPlacement results Round acts on (R6 P6). */
enum class ERoundPlacementResult : uint8
{
	Ok,
	PlacementInfeasible,
	NpcCountTooLow,
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URoundLevelSocketGateway : public UInterface
{
	GENERATED_BODY()
};

/** Round -> Level Socket Contract. OnLayoutReady / OnLayoutAborted arrive as events on Round. */
class STEALTHGAMEPLAY_API IRoundLevelSocketGateway
{
	GENERATED_BODY()

public:
	/** R6 P3. */
	virtual void RequestLayout(int32 Seed, uint64 TuningHash) = 0;

	/** R6 P6, once: the start placement for both teams and the NPC count. */
	virtual ERoundPlacementResult RequestStartPlacement(TConstArrayView<FParticipantId> Team0, TConstArrayView<FParticipantId> Team1, int32 NpcCount) = 0;

	/**
	 * R6 P6 / F5: the zones of the last accepted placement (P_z, N_z per zone), which Round checks against the share
	 * invariant before any pawn spawns (story rmf-038). Empty when the provider reports no zones. Amendment of the RMF
	 * Exit B Preparing batch.
	 */
	virtual void GetPlacementZones(TArray<FRoundPlacementZone>& OutZones) const = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Shared Character & Locomotion: the P6 start spawn (Round decides order and control; the factory spawns)
// ---------------------------------------------------------------------------------------------------------

/** Who drives a live pawn at the round-end step (R9 (f): the stop set covers all four alike). */
enum class ERoundPawnDriver : uint8
{
	Npc,
	/** Participant-bound and AI-driven (hand-over or grace). */
	AiDriven,
	HumanDriven,
	/** A takeover or hand-over committed earlier in the drain, not yet in effect (R9 (f), cross-review S-W1). */
	InHandOver,
};

/** One live pawn of the round (R9 (f)). PawnId is a server-only identity for the stop set (never replicated). */
struct FRoundLivePawn
{
	FPawnHandle Pawn;
	uint32 PawnId = 0;
	ERoundPawnDriver Driver = ERoundPawnDriver::Npc;
};

/** Who drives a start pawn (R6 P6 and the spawn gate): a participant not Connected at P6 gets Ai (never Player). */
enum class ERoundSpawnControl : uint8
{
	Player,
	Ai,
};

/** One entry of the P6 spawn list (F5): a canonical slot, the participant (invalid for an NPC) and its control. */
struct FRoundSpawnEntry
{
	/** The canonical slot: players first in ParticipantId order, then NPCs (independent of who is a player, R23 (a)). */
	int32 Slot = 0;
	/** Invalid for an NPC entry. */
	FParticipantId Participant;
	ERosterTeam Team = ERosterTeam::None;
	ERoundSpawnControl Control = ERoundSpawnControl::Ai;
	/** The allocator's card for a participant (0 = none; NPC cards are Crowd's). */
	uint16 AppearanceId = 0;
	/** Spawn gate (Level Socket Contract R4.8): closed until Session has the participant Connected for this round. */
	bool bGateClosed = false;

	bool IsPlayer() const { return Participant.IsValid(); }
};

/** What one start spawn produced. */
enum class ERoundSpawnResult : uint8
{
	Spawned,
	/** BindPawn was rejected: the factory destroyed the pawn before FinishSpawning (R6 P6: abort BindFailed). */
	BindFailed,
	/** No pawn (no free spot, no class); an NPC entry only costs the crowd one member, a player entry aborts. */
	Failed,
};

/**
 * Round -> the one spawn site (UStealthPawnFactory through the composition root; ADR-0010 section 3.1). Round calls
 * SpawnStartPawn once per entry, in sigma order, in one server frame (R6 P6); it never calls Possess, RestartPlayer
 * or SpawnDefaultPawnFor itself. Plain interface (the factory is a world subsystem; the composition root adapts it).
 * Declared by the RMF Exit B Preparing batch (stories rmf-039, rmf-040).
 */
class IRoundStartSpawner
{
public:
	virtual ~IRoundStartSpawner() = default;

	/** Deferred spawn, appearance, BindPawn (participant), FinishSpawning, possess (Player only). */
	virtual ERoundSpawnResult SpawnStartPawn(const FRoundSpawnEntry& Entry, const FEpochToken& ExpectedEpoch) = 0;

	/** Every start pawn exists (P7 begins); D is the pawn count the round expects. */
	virtual void OnStartSpawnComplete(int32 ExpectedPawnCount) {}

	/**
	 * The participant became Connected: its gated pawn becomes relevant to its connection and is possessed. False when
	 * it could not open yet (no controller mapped): Round keeps the gate and retries at the next Connected fact.
	 */
	virtual bool OpenSpawnGate(FParticipantId Participant) = 0;

	/** R10 (4): destroy every player pawn of the round (an abort leaves 0 pawns); returns how many were destroyed. */
	virtual int32 DestroyRoundPawns() = 0;

	/**
	 * R9 (f): every live pawn of the round (strike role not Dead; a body awaiting its death write still counts) with who
	 * drives it, each pawn once. The factory owns pawn lifetime and the pending hand-over table (ADR-0010), so the
	 * enumeration is its. Amendment of the RMF round-end batch (story rmf-055). Default: none.
	 */
	virtual void GetLivePawns(TArray<FRoundLivePawn>& OutPawns) const {}
};

// ---------------------------------------------------------------------------------------------------------
// Pawn control: takeover and hand-back (R17.1-R17.3; ADR-0010 section 4: the factory's hand-over pass possesses)
// ---------------------------------------------------------------------------------------------------------

/**
 * Round -> the shared pawn's control path (UStealthPawnFactory hand-over pass through the composition root; Crowd
 * registers the restricted Move/Look driver). Round asks; it never calls Possess or BindPawn itself. A request is
 * committed in the drain, bumps the pawn's control epoch and takes effect at the next frame start (ADR-0010 4.3).
 * Amendment of the RMF round-end batch (stories rmf-060, rmf-061).
 */
class IRoundPawnControlGateway
{
public:
	virtual ~IRoundPawnControlGateway() = default;

	/** R17.1 / R17.2: the participant's bound pawn is driven by the crowd AI by the normal blend path. False: no bound pawn. */
	virtual bool RequestTakeover(FParticipantId Participant) = 0;

	/** R17.3: control of the same bound pawn returns to the human by the reverse blend. False: no bound pawn. */
	virtual bool RequestHandBack(FParticipantId Participant) = 0;

	/** The participant's live bound pawn (unset when none). */
	virtual FPawnHandle GetBoundPawn(FParticipantId Participant) const = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Input & Interaction Framework: the round-end step's held replies and frozen holds (R9 (d), (g))
// ---------------------------------------------------------------------------------------------------------

/**
 * Round -> IIF in the round-end step (ADR-0002 section 8.4 (d), (g)). Between the latch and the round-end step the
 * IIF holds its owed NotNow replies and every hold is frozen; Round releases both in the step, in its fixed order.
 * Amendment of the RMF round-end batch (stories rmf-052, rmf-053).
 */
class IRoundCommandGateway
{
public:
	virtual ~IRoundCommandGateway() = default;

	/** (d): the latch's held-reply buffer is released (the NotNow replies are sent). */
	virtual void ReleaseHeldReplies() = 0;

	/** (g): every frozen hold cancels through the normal release blend (no failure cue). */
	virtual void CancelFrozenHolds() = 0;
};

// ---------------------------------------------------------------------------------------------------------
// NPC-Kill Penalty & Informants: the informant body provider (R18; ADR-0019 section 1)
// ---------------------------------------------------------------------------------------------------------

/**
 * Declared by Round, bound by the composition root to Penalty (ADR-0019 section 1). Penalty owns the C1 respawn
 * chain and its retries (PEN-R9, PEN-R10). Declared by the RMF round-end batch (story rmf-062).
 */
class IInformantBodyProvider
{
public:
	virtual ~IInformantBodyProvider() = default;

	/** R18: an Informant returned during Active (after its Session acknowledgement); called inside Round's slot. */
	virtual void OnInformantReadmitted(FParticipantId Participant) = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Objectives Chain: the team's completed stages for TeamSummary (R9 (a))
// ---------------------------------------------------------------------------------------------------------

/** Round -> Objectives' server store (story rmf-054). Amendment of the RMF round-end batch. */
class IRoundObjectiveProgress
{
public:
	virtual ~IRoundObjectiveProgress() = default;

	/** Preliminary (non-final) stages Team completed this round; Round adds the final for the objective winner. */
	virtual int32 GetCompletedPreliminaryStages(int32 Team) const = 0;
};

// ---------------------------------------------------------------------------------------------------------
// The bundle a plain Round core receives
// ---------------------------------------------------------------------------------------------------------

/** Raw gateway pointers handed to Round's plain cores (registered weakly by the composition root in production). */
struct FRoundGateways
{
	IRoundSessionGateway* Session = nullptr;
	IRoundRosterGateway* Roster = nullptr;
	IRoundTuningGateway* Tuning = nullptr;
	IRoundCharacterPoolGateway* CharacterPool = nullptr;
	IRoundLevelSocketGateway* LevelSocket = nullptr;
	ICrowdControl* Crowd = nullptr;
	/** The P6 spawn site (optional until Preparing runs; a null spawner skips P6 spawning). */
	IRoundStartSpawner* Spawner = nullptr;
	/** Optional (null: no takeover or hand-back is requested). */
	IRoundPawnControlGateway* PawnControl = nullptr;
	/** Optional (null: no held replies, no frozen holds). */
	IRoundCommandGateway* Commands = nullptr;
	/** Optional (null: an informant returner waits on Penalty's own path). */
	IInformantBodyProvider* InformantBodies = nullptr;
	/** Optional (null: StepsCompleted counts only the objective winner's final). */
	IRoundObjectiveProgress* ObjectiveProgress = nullptr;

	/** True when all six gateways are present (the spawner is not one of them). */
	bool IsComplete() const
	{
		return Session != nullptr && Roster != nullptr && Tuning != nullptr && CharacterPool != nullptr && LevelSocket != nullptr && Crowd != nullptr;
	}
};
