#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/**
 * Shared Crowd vocabulary (ADR-0016; GDD States and Transitions, CNB-R10, CNB-R11). Server-only, plain C++.
 */

/** The top-level crowd activity of a driven pawn (GDD States table; the Director's activity mirror, ADR-0016 section 3). */
enum class ECrowdActivity : uint8
{
	Dormant,
	Emerging,
	Routing,
	Pausing,
	Conversing,
	Tending,
	Bracing,
	Hiding,
	Exiting,
	Reacting,
	Halted,
	Count,
};

/** Display name of an activity (diagnostics and tests; never replicated). */
inline const TCHAR* LexToString(ECrowdActivity Activity)
{
	switch (Activity)
	{
	case ECrowdActivity::Dormant: return TEXT("Dormant");
	case ECrowdActivity::Emerging: return TEXT("Emerging");
	case ECrowdActivity::Routing: return TEXT("Routing");
	case ECrowdActivity::Pausing: return TEXT("Pausing");
	case ECrowdActivity::Conversing: return TEXT("Conversing");
	case ECrowdActivity::Tending: return TEXT("Tending");
	case ECrowdActivity::Bracing: return TEXT("Bracing");
	case ECrowdActivity::Hiding: return TEXT("Hiding");
	case ECrowdActivity::Exiting: return TEXT("Exiting");
	case ECrowdActivity::Reacting: return TEXT("Reacting");
	case ECrowdActivity::Halted: return TEXT("Halted");
	default: return TEXT("?");
	}
}

/**
 * Which behaviour set drives a pawn. Chosen by the ICrowdControl call that registered the driver (DriveBoundPawn ->
 * Restricted, an NPC spawn or AdoptReleasedPawn -> Full), never by reading a binding (CNB-R2, ADR-0016 section 7).
 */
enum class ECrowdDriverKind : uint8
{
	/** The full catalogue (ST_CrowdFull, FCrowdCommandSink). */
	Full,
	/** Routing, Pausing and Halted only, Move and Look only (ST_CrowdRestricted, FCrowdMotionSink). */
	Restricted,
};

/** The Director's round phase (CNB-R12). */
enum class ECrowdDirectorPhase : uint8
{
	/** Before OnRoundActive: no pawn issues a command. */
	Dormant,
	/** OnRoundActive happened: the crowd runs. */
	Active,
	/** Halt in the round-end step: every sink latched (terminal for the round). */
	Halted,
};
