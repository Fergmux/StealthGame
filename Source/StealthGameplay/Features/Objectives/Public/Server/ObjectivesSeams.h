#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EventLog/StealthRoundLogTypes.h"
#include "View/CourtFixtureId.h"
#include "View/StealthQueueTypes.h"
#include "View/StealthRosterTypes.h"

class APawn;

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
// Downstream stand-ins of later stories (observed by the tests of stories oc-008)
// ---------------------------------------------------------------------------------------------------------

/**
 * The team-view write after a counted stage (ADR-0020 section 6). Stories oc-010 / oc-011 build FObjectiveTeamView and its
 * replicated component; until then the service forwards the push to this observer (the composition root's stand-in mirrors
 * the stage to the prototype HUD).
 */
class IObjectivesViewSink
{
public:
	virtual ~IObjectivesViewSink() = default;

	/** Team's view changed (Active entry, a counted stage). Never called for the final. */
	virtual void PushTeamView(uint8 Team) = 0;
};

/** The OC-12 rival release scheduling after a counted preliminary (stories oc-014 / oc-015 own the draw and the timer). */
class IObjectivesRivalRelease
{
public:
	virtual ~IObjectivesRivalRelease() = default;

	/** A counted preliminary completion of CompletingTeam with the round-global prelim ordinal (1-based). */
	virtual void ScheduleRivalRelease(uint8 CompletingTeam, uint32 PrelimOrdinal) = 0;
};
