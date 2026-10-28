#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EventLog/StealthRoundLogTypes.h"
#include "View/CourtFixtureId.h"
#include "View/ObjectiveTeamView.h"
#include "View/StealthQueueTypes.h"
#include "View/StealthRosterTypes.h"

class APawn;
class UObjectiveTeamViewComponent;

/**
 * The Objectives Chain's consumer-owned seams (ADR-0020 Key Interfaces; stories oc-003..oc-009). FObjectivesCore reads the
 * world only through these plain C++ interfaces, so headless tests inject stubs and the composition root injects adapters
 * over the Roster, Session, Round, the pawn factory and the layout. Server-only (guarded header): no client module, Crowd,
 * IIF or Presentation code may include it (ADR-0020 Implementation Guidelines).
 */

// ---------------------------------------------------------------------------------------------------------
// Level Socket Contract: the server-secret site assignment and the court geometry
// ---------------------------------------------------------------------------------------------------------

/**
 * The server-secret site assignment (GDD objectives-chain.md Dependencies: LSC FSiteAssignment {Final, P1[2], P2[2], ZoneTeam};
 * ADR-0014 section 7.4). Declared here by story oc-003 because the Level Socket Contract epic is not built yet; that epic
 * takes the declaration over (its layout service owns the value). ZoneTeam (start zones) is not read by Objectives and is
 * left out. Never replicated, never copied to a client-reachable place, never logged outside the server-only Event Log
 * header (ADR-0014 Guideline 13).
 */
struct FSiteAssignment
{
	FCourtFixtureId Final;
	FCourtFixtureId P1[2];
	FCourtFixtureId P2[2];
};

/**
 * The one read path of the assignment (ADR-0020 Guideline: FSiteAssignment is read only through
 * GetSiteAssignment_Server() inside UObjectivesService / FObjectivesCore). Production: the composition root's layout adapter
 * (STAND-IN over L_Prototype's objective actors until the Level Socket Contract's UStealthLayoutService exists).
 */
class IObjectivesAssignmentSource
{
public:
	virtual ~IObjectivesAssignmentSource() = default;

	/** The current layout's assignment; false when no layout is ready. Server only. */
	virtual bool GetSiteAssignment_Server(FSiteAssignment& OutAssignment) const = 0;
};

/** Public court geometry: the stand point of each fixture face (LSC request: four faces; FixtureInteractSideCount uses the first N). */
class IObjectivesCourtGeometry
{
public:
	virtual ~IObjectivesCourtGeometry() = default;

	/** The world positions of Court's fixture faces in face-index order; false for an unknown court. */
	virtual bool GetFixtureFaces(FCourtFixtureId Court, TArray<FVector, TInlineAllocator<4>>& OutFaces) const = 0;
};

/**
 * One court's public ClueTag record (LSC R3 court row; ADR-0014 section 7.4; story oc-013): the court and the two role-blind
 * values the Level Socket Contract publishes for it, a pure function of the published geometry. Public data: it names a court
 * and its surroundings, nothing about its role.
 */
struct FCourtClueRecord
{
	FCourtFixtureId Court;
	/** LSC AreaTypeId (EClueAreaType values; 0 = None). */
	uint8 AreaTypeId = 0;
	/** The nearest distinctive public feature (EClueFeatureKind values; 0 = None, the area-only fallback). */
	uint8 NearestFeatureKind = 0;
};

/**
 * Every court of the current layout with its public ClueTag record (story oc-013). Production: the composition root's layout
 * adapter (STAND-IN over L_Prototype until the Level Socket Contract's layout service publishes the records). Read once at
 * OnLayoutReady; the role assignment is never an input.
 */
class IObjectivesCourtClues
{
public:
	virtual ~IObjectivesCourtClues() = default;

	/** The layout's courts and their ClueTag records, in any order (each court at most once). */
	virtual void GetCourtClueRecords(TArray<FCourtClueRecord>& OutRecords) const = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Participant State & Roster, Session, pawn control (the gate's server-only queries, ADR-0020 5.4)
// ---------------------------------------------------------------------------------------------------------

/** The legality class the Interact predicate reads (ADR-0020 5.3; Roster GetLegalityClass: an unbound pawn is Active). */
enum class EObjectivesLegalityClass : uint8
{
	Active,
	Informant,
	RoundEnded,
};

/**
 * The Roster queries Objectives makes. GetLegalityClass is the one call the predicate may make; GetParticipant is the
 * server-only binding lookup only the completion gate makes (the named bound/unbound exception, ADR-0020 Guidelines).
 */
class IObjectivesRosterView
{
public:
	virtual ~IObjectivesRosterView() = default;

	virtual EObjectivesLegalityClass GetLegalityClass(const APawn& Pawn) const = 0;
	/** The participant bound to Pawn, or an invalid id (an unbound crowd pawn). */
	virtual FParticipantId GetParticipant(const APawn& Pawn) const = 0;
	virtual EParticipantState GetState(FParticipantId Participant) const = 0;
	/** 0 or 1, or INDEX_NONE without a team. */
	virtual int32 GetTeam(FParticipantId Participant) const = 0;
};

/** Session's own connection state (gate (c) reads it, never the Roster's collapsed value; cross-review S-I4). */
enum class EObjectivesConnectionState : uint8
{
	Connecting,
	Connected,
	Unresponsive,
	Disconnected,
	Dormant,
};

class IObjectivesSessionView
{
public:
	virtual ~IObjectivesSessionView() = default;

	virtual EObjectivesConnectionState GetConnectionState(FParticipantId Participant) const = 0;
};

/** ADR-0010 section 4.4: the pawn's server-only control epoch and the pending hand-over (gate (c)). */
class IObjectivesPawnControlView
{
public:
	virtual ~IObjectivesPawnControlView() = default;

	virtual uint32 GetControlEpoch(const APawn& Pawn) const = 0;
	virtual bool IsHandOverPending(const APawn& Pawn) const = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Round & Match Framework: phase, latch, win declaration, Event Log, queue key and time
// ---------------------------------------------------------------------------------------------------------

/** ADR-0020 IRoundObjectiveHooks {IsActive, DeclareObjectiveWin, AppendEvent} plus the queue reads the core needs. */
class IRoundObjectiveHooks
{
public:
	virtual ~IRoundObjectiveHooks() = default;

	/** Round phase Active. */
	virtual bool IsActive() const = 0;
	/** UResolutionQueue::IsLatched() (gate (a)'s defensive re-check). */
	virtual bool IsLatched() const = 0;
	/** Round.DeclareObjectiveWin(Team, Key): called in the HoldTimer(Complete) slot with CurrentKey(). True when it latched. */
	virtual bool DeclareObjectiveWin(uint8 Team, FQueueKey Key) = 0;
	/** The Round Event Log's Append (the only log path; never UE_LOG). */
	virtual void AppendEvent(const FRoundLogEntry& Entry) = 0;
	/** The current queue slot's key (unset outside a slot). */
	virtual FQueueKey CurrentKey() const = 0;
	/** Round's server time (seconds). */
	virtual double ServerNow() const = 0;
};

// ---------------------------------------------------------------------------------------------------------
// The team view and the rival release (stories oc-010 / oc-011; oc-014 / oc-015)
// ---------------------------------------------------------------------------------------------------------

/**
 * The team-level view write (ADR-0020 section 6; stories oc-010 / oc-011). FObjectivesCore builds the team's view
 * (BuildTeamView) and calls PushTeamView only when the value differs from the last one it pushed for that team (change-only):
 * at Active entry (both teams), after a counted preliminary stage, and when the view is cleared. Never for the final's
 * completion. UObjectivesService is the production sink: it writes the value to every current controller of the team's
 * members (IObjectivesViewTargets); tests observe the pushes with a stub.
 */
class IObjectivesViewSink
{
public:
	virtual ~IObjectivesViewSink() = default;

	/** Team's view changed to View. */
	virtual void PushTeamView(uint8 Team, const FObjectiveTeamView& View) = 0;
};

/**
 * Where the service writes a team's view (ADR-0020 section 6: Roster GetTeamMembers(T), controller from Session's connection
 * table; production: the composition root's adapter over URosterStore and its controller map). Server only.
 */
class IObjectivesViewTargets
{
public:
	virtual ~IObjectivesViewTargets() = default;

	/** Team's participants (Active and Informant alike: the Team audience includes the team's informants), ascending. */
	virtual void GetTeamMembers(uint8 Team, TArray<FParticipantId>& OutMembers) const = 0;

	/** The Objectives view component of Participant's current controller, or null (no controller connected). */
	virtual UObjectiveTeamViewComponent* FindTeamView(FParticipantId Participant) const = 0;
};

/**
 * One pending OC-12 release (stories oc-014 / oc-015): the core drew its delay and placed it on the global grid. The timer
 * payload it becomes is ADR-0002 section 2a's FTimerPayload {Kind StageSignal, Subject none, Generation 0, Seq Ordinal,
 * Phase ViewingTeam}: no court, completer or step travels.
 */
struct FObjectivesRivalRelease
{
	/** The team that completed the preliminary (kept server-side for the log and the tests; not in the payload). */
	uint8 CompletingTeam = 0;
	/** The team whose RivalPrelimCount the release raises (the completer's opponent). */
	uint8 ViewingTeam = 1;
	/** The round-global preliminary ordinal k (1-based; both teams together). */
	uint32 Ordinal = 0;
	/** The drawn delay Delta_ms (server-only; tests and the log). */
	int32 DeltaMs = 0;
	/** The completion time t_c in integer milliseconds of server time. */
	int64 CompletedMs = 0;
	/** The release time on the grid in integer milliseconds of server time (t_rel_ms). */
	int64 ReleaseMs = 0;

	/** The queue due time (t_rel_ms / 1000; equal milliseconds give identical doubles, so one grid tick is one drain). */
	double DueServerTime() const { return static_cast<double>(ReleaseMs) / 1000.0; }
};

/**
 * The OC-12 rival release scheduling after a counted preliminary (stories oc-014 / oc-015). The core draws and places the
 * release; this seam schedules its one TimerFired(StageSignal) item. Production: UObjectivesService itself (the round queue's
 * ScheduleAt, Expected = the round epoch); tests record the requests. Never a tick, FTimerManager timer or latent action.
 */
class IObjectivesRivalRelease
{
public:
	virtual ~IObjectivesRivalRelease() = default;

	/** Schedules Release's TimerFired(StageSignal) at Release.DueServerTime(). */
	virtual void ScheduleRivalRelease(const FObjectivesRivalRelease& Release) = 0;
};

/**
 * The Objectives-local secret draw seam (story oc-014; ADR-0006): the server sub-seed of the StageSignal domain for the
 * round-global ordinal k, GetServerSubSeed(StageSignal, k). Keyed by the ordinal ONLY (no team, step, completer or court
 * parameter exists, so "keyed by ordinal only" is structural). Production: the composition root's adapter over Session's
 * sub-seed source; tests supply fixed sub-seeds. 0 when no secret is held (nothing is then drawn).
 */
class IObjectivesSecretDraw
{
public:
	virtual ~IObjectivesSecretDraw() = default;

	virtual uint64 GetStageSignalSubSeed(uint32 Ordinal) const = 0;
};
