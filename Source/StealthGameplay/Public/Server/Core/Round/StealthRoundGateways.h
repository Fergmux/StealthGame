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

	/** True when all six gateways are present. */
	bool IsComplete() const
	{
		return Session != nullptr && Roster != nullptr && Tuning != nullptr && CharacterPool != nullptr && LevelSocket != nullptr && Crowd != nullptr;
	}
};
