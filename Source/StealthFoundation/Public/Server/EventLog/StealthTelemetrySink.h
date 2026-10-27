#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthQueuePayloads.h"
#include "View/StealthQueueTypes.h"

/**
 * ADR-0023 (Playtest telemetry hooks) Key Interfaces: the server-only sink the owners' kind-blind probes call (story
 * asn-026 adds Assassination's StrikeResolved / BraceEnded call sites). The telemetry section of the Event Log store
 * (FPlaytestTelemetryCore, its bounded buffers, the post-termination JSON export and UStealthEventLogSubsystem::
 * GetTelemetrySink()) is the Round / Foundation epics' and does not exist yet: until it does, producers fall back to
 * FNullStealthTelemetrySink and record nothing.
 *
 * Rules (ADR-0023 Implementation Guidelines): plain structs (no USTRUCT, no ToString); implementations never log, trace or
 * replicate a value; probes never pass a kind or team (the sink resolves FSubjectRef through the Roster at write time);
 * nothing in the game reads telemetry back. Compiled to the null object in Shipping (STEALTH_WITH_PLAYTEST_TELEMETRY 0).
 *
 * Usage (an owner's probe at its existing server decision point):
 *   FTelemetryStrikeResolved Event;
 *   Event.Key = SlotKey; Event.Attacker = FSubjectRef::ForPawn(FPawnHandle{ Attacker }); ...
 *   Sink.Record(Event);   // Sink: an IStealthTelemetrySink&, never null (FNullStealthTelemetrySink::Get() when disabled)
 */

#ifndef STEALTH_WITH_PLAYTEST_TELEMETRY
#define STEALTH_WITH_PLAYTEST_TELEMETRY (!UE_BUILD_SHIPPING)
#endif

/** StrikeResolved outcome (ADR-0023 section 2.1). */
enum class ETelemetryStrikeOutcome : uint8
{
	Kill,
	Reversal,
	Whiff,
};

/** BraceEnded cause (ADR-0023 section 2.1). */
enum class ETelemetryBraceEnd : uint8
{
	/** The brace reversed a strike (the defender's window ended at the reversal). */
	Reversal,
	/** The window ran to its cap. */
	Expired,
	/** The holder released it. */
	Cancelled,
	/** The holder's own Attack superseded it. */
	Preempted,
	/** The round was reset (or the pawn left) while it was up. */
	Removed,
};

/** StrikeResolved: one Attack at its slot, after the outcome is decided. */
struct FTelemetryStrikeResolved
{
	FQueueKey Key;
	FSubjectRef Attacker;
	/** The ASN-R8 victim (none for a whiff); on a reversal the defender. */
	FSubjectRef Victim;
	ETelemetryStrikeOutcome Outcome = ETelemetryStrikeOutcome::Whiff;
	/** Contact time - the victim's current brace start, or -1 when it was not braced. */
	float VictimBraceAgeSec = -1.f;
	/** Contact time - the victim's last brace end, or -1 (braced now, or never braced). */
	float VictimSinceBraceEndSec = -1.f;
	/** The victim could have braced at contact (its defence cooldown was over). */
	bool bVictimDefenceReady = false;
	/** -1 until Objectives publishes its court table to the probe. */
	float DistToNearestCourtCm = -1.f;
	bool bNearestCourtIsFinalCandidate = false;
	bool bTendingAtContact = false;
};

/** BraceEnded: a pawn left Bracing. */
struct FTelemetryBraceEnded
{
	double ServerTime = 0.0;
	FSubjectRef Subject;
	double StartTime = 0.0;
	ETelemetryBraceEnd Cause = ETelemetryBraceEnd::Expired;
};

/** TendEnded (Objectives, ADR-0020; no producer yet). */
struct FTelemetryTendEnded
{
	double ServerTime = 0.0;
	FSubjectRef Subject;
	uint8 CourtId = 0;
	double StartTime = 0.0;
	uint8 Result = 0;
};

/** BushTransition (Concealment, ADR-0017; no producer yet). */
struct FTelemetryBushTransition
{
	double ServerTime = 0.0;
	FSubjectRef Subject;
	uint16 BushId = 0;
	bool bEnter = false;
};

/** NpcTellAccepted (Crowd, ADR-0016; the Crowd sampler forwards through ICrowdTelemetrySink until the section exists). */
struct FTelemetryNpcTellAccepted
{
	double ServerTime = 0.0;
	FSubjectRef Npc;
	uint8 TellId = 0;
	float GapSincePreviousTellSec = 0.f;
};

/** Server-only sink for playtest telemetry. Implementations never log, trace or replicate. Game thread only. */
class IStealthTelemetrySink
{
public:
	virtual ~IStealthTelemetrySink() = default;
	virtual void Record(const FTelemetryStrikeResolved& Event) = 0;
	virtual void Record(const FTelemetryBraceEnded& Event) = 0;
	virtual void Record(const FTelemetryTendEnded& Event) = 0;
	virtual void Record(const FTelemetryBushTransition& Event) = 0;
	virtual void Record(const FTelemetryNpcTellAccepted& Event) = 0;
};

/** The null object (disabled telemetry, Shipping, or no Event Log section yet): records nothing. */
class FNullStealthTelemetrySink final : public IStealthTelemetrySink
{
public:
	/** The process-wide instance (stateless). */
	static FNullStealthTelemetrySink& Get()
	{
		static FNullStealthTelemetrySink Instance;
		return Instance;
	}

	virtual void Record(const FTelemetryStrikeResolved&) override {}
	virtual void Record(const FTelemetryBraceEnded&) override {}
	virtual void Record(const FTelemetryTendEnded&) override {}
	virtual void Record(const FTelemetryBushTransition&) override {}
	virtual void Record(const FTelemetryNpcTellAccepted&) override {}
};
