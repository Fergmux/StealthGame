#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "View/CourtFixtureId.h"

/**
 * Red-herring and reaction vocabulary (GDD CNB-R7, CNB-R8; ADR-0016 sections 3 and 5; stories cnb-019 to cnb-024).
 * Server-only, plain C++. Nothing here names a player, a binding, a team, a participant state or a site assignment.
 */

/**
 * The unkeyed behaviour constants of the red herrings and reactions (GDD CNB-R7 and R8 state them as numbers; TDC change
 * request 22 asks to register them as keys or confirm they live here). Never copied into another module.
 */
namespace CrowdHerringConstants
{
	/** T1: a run bout lasts 2-6 s (a late porter). */
	inline constexpr double RunBoutMinSec = 2.0;
	inline constexpr double RunBoutMaxSec = 6.0;
	/** T4: a due brace waits this long for a flinch trigger, then is an ambient startle. */
	inline constexpr double BraceDueWindowSec = 30.0;
	/** T4 flinch triggers: a runner passing within 2 m, a traversal landing within 3 m. */
	inline constexpr double FlinchRunnerRadiusCm = 200.0;
	inline constexpr double FlinchLandingRadiusCm = 300.0;
	/** T4: braces not held to the cap are released at U(1.0, 3.8) s. */
	inline constexpr double BraceReleaseMinSec = 1.0;
	inline constexpr double BraceReleaseMaxSec = 3.8;
	/** T5: the bush is the nearest one with an open side within 10 m of the current leg. */
	inline constexpr double BushSearchCm = 1000.0;
	/** T2: climbs only where the coping is at least 1.6 m. */
	inline constexpr double ClimbMinCopingCm = 160.0;
	/** T2: a due traversal looks for public coping this far away (a leg that crosses it). */
	inline constexpr double TraversalSearchCm = 2500.0;
	/** T2: the Traverse command is issued within this distance of the coping's near side. */
	inline constexpr double TraverseTriggerCm = 120.0;
	/** An accepted tell not performed within this long is dropped (counted; the chain gap is never bypassed). */
	inline constexpr double PendingTellMaxSec = 30.0;
	/** A due vault or climb waits this long for a reachable coping of its class. */
	inline constexpr double PendingTraversalMaxSec = 60.0;
	/** A due bush entry waits this long for a leg that passes a bush (bushes are sparse). */
	inline constexpr double PendingBushMaxSec = 120.0;
	/** T12: a reversal needs the leg to have been walked this long (there is something to turn back along). */
	inline constexpr double ReversalMinLegSec = 1.0;
	/** A discrete Begin whose outcome is not known after this long is taken as accepted (no reply sink on the NPC). */
	inline constexpr double BeginOutcomeTimeoutSec = 0.5;
	/** R8: a reroute walks this far away from the strike point. */
	inline constexpr double ReactionRerouteCm = 800.0;
	/**
	 * F2 (amended 2026-10-27): the mean delay from a tell's acceptance to its visible moment in the reference scene, per
	 * ECrowdTell (run, vault, climb, brace, decoy tend, bush, reversal, head hold), seconds. Derived offline from
	 * StealthGame.Crowd.TellCalibration.TellBaselineNonZero (its SHOWDELAY line) and re-derived per playtest build like
	 * the F2 rates; never adapted at runtime.
	 */
	inline constexpr double ShowDelaySec[8] = { 0.36, 12.20, 12.89, 28.87, 21.26, 7.28, 2.77, 6.66 }; // measured 2026-10-27
}

/** A public coping crossing (an LSC TraversalPoint with a nav link): near side, far side and coping height. */
struct FCrowdTraversalSite
{
	/** The Level Socket Contract point id (the Traverse hint). 0 = none. */
	uint16 PointId = 0;
	FVector Near = FVector::ZeroVector;
	FVector Far = FVector::ZeroVector;
	double CopingHeightCm = 0.0;
};

/** One court and the free stand point the NPC takes (Objectives FixtureInteractSideCount). */
struct FCrowdCourtSite
{
	int32 Court = INDEX_NONE;
	FCourtFixtureId Fixture;
	FVector StandPoint = FVector::ZeroVector;
	/** Where the tending pawn faces (the fixture). */
	FVector LookPoint = FVector::ZeroVector;
};

/** One bush and its open sides (Concealment): the entry the NPC walks to and the exit points per side. */
struct FCrowdBushSite
{
	uint16 BushId = 0;
	FVector Entry = FVector::ZeroVector;
	FVector Center = FVector::ZeroVector;
	TArray<FVector, TInlineAllocator<4>> SidePoints;
};

/** An ambient exit slot given to an NPC (story cnb-026): the socket, its cycle and the alley's mouth and absorb point. */
struct FCrowdExitSite
{
	int32 Socket = INDEX_NONE;
	int32 Cycle = INDEX_NONE;
	FVector Mouth = FVector::ZeroVector;
	FVector Inside = FVector::ZeroVector;
};

/** The action timings the herrings read from other owners' tuning (never copied into a Crowd key). */
struct FCrowdActionTimings
{
	/** Objectives ObjectiveInteractSec: a full-length decoy tend holds exactly this long. */
	double ObjectiveInteractSec = 3.0;
	/** The Prepare hold cap (Assassination DefenceDurationSec, the IIF's Prepare cap). */
	double PrepareCapSec = 4.0;
	/** The Prepare cooldown (Assassination DefenceCooldownSec, from activation). */
	double PrepareCooldownSec = 60.0;
	/** Concealment BushReenterCooldownSec (a pending owner key until the Concealment epic registers it). */
	double BushReenterCooldownSec = 6.0;
	/** SCL HandoverHoldSec: after an AI takeover the heading is held at walk this long (story cnb-028). */
	double HandoverHoldSec = 0.8;
};

/** What became of a discrete Begin the NPC sent (the NPC's reply sink is null, so the world tells it). */
enum class ECrowdBeginOutcome : uint8
{
	Pending,
	Accepted,
	Refused,
};

/**
 * The herring sites a world offers (ADR-0016 section 3). The Level Socket Contract (TraversalPoints), Objectives
 * (courts and stand points) and Concealment (bushes) implement it and the composition root registers one with
 * UCrowdDirector::SetHerringSites; a world without it disables the vault, climb, decoy and bush red herrings (the tell
 * clocks of tells with no site never run, so no accepted tell goes unperformed). Every answer is geometry only: never
 * occupancy of a bush, an assignment, a zone role or a pawn's kind.
 */
class ICrowdHerringSites
{
public:
	virtual ~ICrowdHerringSites() = default;
	/** The nearest public coping crossing within MaxCm of From of the class asked for (climb: coping >= 1.6 m). */
	virtual bool FindTraversalNear(const FVector& From, double MaxCm, bool bClimb, FCrowdTraversalSite& Out) const = 0;
	virtual int32 GetCourtCount() const = 0;
	/** A free stand point of Court (physical occupancy only); false when every stand point is occupied. */
	virtual bool GetCourtStandPoint(int32 Court, const FVector& From, FCrowdCourtSite& Out) const = 0;
	/** The nearest bush with an open side within RadiusCm of the path (From then Path). Reads no occupancy. */
	virtual bool FindBush(const FVector& From, TConstArrayView<FVector> Path, double RadiusCm, FCrowdBushSite& Out) const = 0;
	virtual bool HasTraversals() const = 0;
	virtual bool HasBushes() const = 0;
};
