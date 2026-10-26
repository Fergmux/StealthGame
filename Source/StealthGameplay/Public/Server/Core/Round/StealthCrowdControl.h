#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "View/StealthQueueTypes.h"

#include "StealthCrowdControl.generated.h"

class AStealthPawn;

/**
 * ICrowdControl: the Core-owned consumer interface Round calls on Crowd (ADR-0016 Key Interfaces and section 7;
 * ADR-0002 section 8.4 (f); ADR-0007 consumer-owned interfaces). Crowd's Director implements it; the composition
 * root registers it weakly. None of these calls possesses anything: the caller also calls
 * UStealthPawnFactory::RequestHandOver in the same slot (ADR-0010); these only register the driver.
 *
 * Declared by the RMF-003 gateway ruling (2026-10-25) with Round's other gateways so the fake set is complete.
 */

/** ReleaseToCrowd adoption parameters (ADR-0016). */
USTRUCT()
struct FCrowdAdoptParams
{
	GENERATED_BODY()

	/** Strike lockout end; -1 = none. */
	double RestrictedUntilServerTime = -1.0;

	/** Exile body (Penalty CR-4): leave at the next scheduled exit slot. */
	bool bExitAtNextScheduledSlot = false;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UCrowdControl : public UInterface
{
	GENERATED_BODY()
};

class STEALTHGAMEPLAY_API ICrowdControl
{
	GENERATED_BODY()

public:
	/**
	 * Round, in the Active-entry step (R7): start every NPC and every AI-driven pawn (nothing moves in Preparing).
	 * Amendment of the RMF Exit B Preparing batch (story rmf-042): ADR-0016 named no start call.
	 */
	virtual void StartRound() = 0;

	/** Round, inside the round-end step: halt latch on every sink, clear held Move, stop trees and the schedule. */
	virtual void Halt() = 0;

	/** Round: register the restricted driver (Move and Look only) for a participant-bound pawn. */
	virtual void DriveBoundPawn(AStealthPawn* Pawn) = 0;

	/** Round: hand a bound pawn back to its human. */
	virtual void ReleaseControl(AStealthPawn* Pawn) = 0;

	/** ReleaseToCrowd adoption: the pawn becomes an NPC with the same look. */
	virtual void AdoptReleasedPawn(AStealthPawn* Pawn, const FCrowdAdoptParams& Params) = 0;

	/**
	 * Round, in the round-end step after Halt (R9 (f)): one kind-blind server stop-and-blend for this pawn, bounded by
	 * BoundSec (RunStopSec), whoever drives it (NPC, AI-driven, human-driven or in hand-over; ADR-0010 owns the stop
	 * path). Amendment of the RMF round-end batch (story rmf-055).
	 */
	virtual void RequestPawnStop(const struct FRoundLivePawn& Pawn, double BoundSec) = 0;
};

/**
 * Per unbound NPC pawn, why it may or may not be taken over by an informant (CNB-R11, ADR-0016 section 8). Only
 * Eligible pawns are candidates. Never computed from appearance, team or who is watching.
 */
enum class ETakeoverEligibility : uint8
{
	Eligible,
	Tending,
	Bracing,
	Hiding,
	Emerging,
	Exiting,
	Reacting,
	Conversing,
	Bound,
	AlleyExit,
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTakeoverEligibilitySource : public UInterface
{
	GENERATED_BODY()
};

/**
 * ITakeoverEligibilitySource: the Core-owned consumer interface the Layout Service and Penalty read for informant
 * takeover (ADR-0016 section 8, ADR-0019; story cnb-004 declares it, cnb-030 fills it). Crowd's Director implements it;
 * the composition root registers it weakly.
 */
class STEALTHGAMEPLAY_API ITakeoverEligibilitySource
{
	GENERATED_BODY()

public:
	/** The pawn's eligibility, computed from authoritative sources at call time. */
	virtual ETakeoverEligibility GetTakeoverEligibility(const AStealthPawn* Pawn) const = 0;

	/** Penalty, inside its commit slot: re-checks Eligible atomically and releases the NPC from the crowd. */
	virtual bool TryReleaseNpc(AStealthPawn* Pawn, FEpochToken Expected) = 0;
};

