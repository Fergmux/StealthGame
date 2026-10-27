#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/CrowdAvoidance.h"
#include "View/Core/StealthGait.h"

class FCrowdDraws;
struct FCrowdTuning_Server;
struct FLocomotionTuning_Public;

/** The follower's wish for this tick: a world travel direction and magnitude (the virtual input layer turns it into input). */
struct FCrowdFollowerOutput
{
	/** Unit world-space 2D direction of travel (zero when holding still). */
	FVector2D WorldDirection = FVector2D::ZeroVector;
	/** Input magnitude: the leg pace from {0.8, 0.9, 1.0}, or 0 for Move(0). */
	float Magnitude = 0.0f;
	EGait Gait = EGait::Walk;
	/** The pawn came to rest at the end of its leg this tick. */
	bool bArrived = false;
	/** The blocked ladder asks for a repath (the caller re-requests the path through the budget). */
	bool bWantsRepath = false;
	/** The leg is abandoned (two failed repaths, a failed or partial query): the brain draws a new destination. */
	bool bAbandoned = false;
	/** Move(0) was issued this tick (the start of a stop to rest). */
	bool bReleased = false;
};

/** Why the current pace was set (diagnostics and the AC-CNB-03 log). */
enum class ECrowdPaceCause : uint8
{
	None,
	LegStart,
	AfterStop,
	GroupDrift,
	DriftEnd,
};

/**
 * FCrowdFollower: turns a path into human-shaped motion (ADR-0016 section 4.2; GDD CNB-R3.1-R3.3, AC-CNB-03/04; stories
 * cnb-009 and cnb-011). Plain C++, stepped from the controller tick:
 *   - string-pulled corners (the follower funnels nothing itself: navmesh queries return string-pulled corners, E11)
 *     shifted by the pawn's lane offset (+-CrowdLaneOffsetCm, drawn once),
 *   - a pure-pursuit carrot max(CrowdCarrotMinCm, v * CrowdCarrotLeadSec) ahead along the path, recomputed at
 *     CrowdSteerPerSec with a per-pawn phase offset (the last direction is held between recomputes, like a held key),
 *   - the avoidance offset (FCrowdAvoidance) and the blocked ladder (widen, repath, abandon),
 *   - early turns: the carrot leads the corner by more than the SCL F3 arc (134 cm at 1.0), and no commanded heading
 *     change while moving exceeds ReversalAngleDeg minus a margin (so NPCs never trigger a reversal by accident),
 *   - Move(0) at the SCL F2 stop distance of the current speed before the end, through SCL's one braking path (the stop
 *     floor makes every stop last at least StopMinSec),
 *   - the pace from {0.8, 0.9, 1.0}, chosen only at a leg start or after a stop, with a walking-time share at 1.0 of
 *     CrowdFullPacePct, plus C4 group drift (match a neighbour's pace for at most CrowdGroupDriftSec per leg).
 */
class STEALTHGAMEPLAY_API FCrowdFollower
{
public:
	/** The share of 0.8 / 0.9 legs among the slower legs (equal). */
	static constexpr double SlowPaceShareOfNonFull = 0.5;
	/**
	 * Heading margin below ReversalAngleDeg the follower never commands while moving. It covers the virtual input
	 * layer's keyboard quantisation (45 degree sectors: up to 22.5 degrees off the wish) with room to spare, so a leg
	 * started while walking away from the current heading (a herring's leg, story cnb-019) never commands a reversal.
	 */
	static constexpr double ReversalMarginDeg = 40.0;

	/** Draws the pawn's lane offset (once) and sets its steering phase. */
	void Initialize(uint32 InOrdinal, double SteerPhaseSec, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws);

	/**
	 * Starts a leg on Corners (start excluded, end last) with a new LegId. The pace is drawn here (a leg boundary) unless
	 * bKeepPace (the reversal walks back at its pace). Lane offset applies to every corner but the end.
	 * Gait Run (a T1 run leg or an R8 panic run, story cnb-019) moves at full input magnitude with no group drift; when
	 * ReleaseAt is positive the leg releases Move(0) at that time (the bout ends by ordinary SCL braking, and the leg ends
	 * where the braking ends), whichever of that and the path end comes first.
	 */
	void StartLeg(uint32 LegId, const FVector& From, TConstArrayView<FVector> Corners, double Now, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws, bool bKeepPace = false,
		EGait InGait = EGait::Walk, double InReleaseAt = -1.0);

	/** Replaces the path of the current leg from From (a repath or the first async result); keeps the pace. */
	void ReplacePath(const FVector& From, TConstArrayView<FVector> Corners);

	/** Waiting for a path: hold the last Move for at most CrowdPathInvalidHoldSec, then Move(0). */
	void BeginWaitForPath(double Now);

	/** Ends the leg now with Move(0) (abandon, Halt, a behaviour that stops). */
	void Stop();

	/**
	 * One controller tick. Self: the pawn's public facts; Neighbours: the Steering query (for avoidance and drift);
	 * Locomotion: the SCL tuning (stop distances, reversal angle, walk speed).
	 */
	FCrowdFollowerOutput Tick(double Now, float Dt, const FCrowdPerceivedPawn& Self, TConstArrayView<const FCrowdPerceivedPawn*> Neighbours,
		const FCrowdTuning_Server& Tuning, const FLocomotionTuning_Public& Locomotion, FCrowdDraws& Draws);

	bool IsOnLeg() const { return bOnLeg; }
	bool IsStopping() const { return bStopping; }
	uint32 GetLegId() const { return LegId; }
	float GetPace() const { return Pace; }
	ECrowdPaceCause GetLastPaceCause() const { return LastPaceCause; }
	double GetLaneOffsetCm() const { return LaneOffsetCm; }
	TConstArrayView<FVector> GetCorners() const { return Corners; }
	/** Where the current path starts. */
	FVector GetOrigin() const { return Origin; }
	const FCrowdAvoidance& GetAvoidance() const { return Avoidance; }
	/** Repaths requested on this leg. */
	int32 GetRepathCount() const { return Repaths; }

	/** The F2 stop distance at Speed (v^2 / (2 d_eff), SCL StopDeceleration of the gait with the stop floor). */
	static double StopDistanceCm(const FLocomotionTuning_Public& Locomotion, double Speed, EGait Gait = EGait::Walk);

	/** The current leg's gait (Walk, or Run on a run leg). */
	EGait GetGait() const { return Gait; }
	/** When the current leg releases by time (negative: at the path end only). */
	double GetReleaseAt() const { return ReleaseAt; }

	/** The leg probability of pace 1.0 that gives a walking-time share of FullTimeShare (slower legs last longer). */
	static double FullPaceLegProbability(double FullTimeShare);

private:
	void ChoosePace(const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws, ECrowdPaceCause Cause);
	/** Closest point on the path (segment index and point) from Location. */
	void ProjectOntoPath(const FVector& Location, int32& OutSegment, FVector& OutPoint) const;
	/** The point Distance further along the path from (Segment, Point), and the remaining length. */
	FVector AdvanceAlongPath(int32 Segment, const FVector& Point, double Distance, double& OutRemaining, FVector& OutTangent) const;

	uint32 Ordinal = 0;
	uint32 LegId = 0;
	double SteerPhase = 0.0;
	double LaneOffsetCm = 0.0;
	TArray<FVector, TInlineAllocator<16>> Corners;
	/** The path origin (the pawn's position at the leg start), so the first segment exists. */
	FVector Origin = FVector::ZeroVector;
	/** The segment the pawn was last projected onto (monotonic along the leg). */
	int32 CurrentSegment = 0;
	double LegStartTime = 0.0;
	bool bOnLeg = false;
	bool bStopping = false;
	bool bWaitingForPath = false;
	double WaitStarted = 0.0;
	float Pace = 0.0f;
	float LegPace = 0.0f;
	EGait Gait = EGait::Walk;
	double ReleaseAt = -1.0;
	ECrowdPaceCause LastPaceCause = ECrowdPaceCause::None;
	FVector2D HeldDirection = FVector2D::ZeroVector;
	double NextSteerTime = 0.0;
	// Blocked ladder and progress monitor.
	double BlockedSec = 0.0;
	int32 Repaths = 0;
	bool bRepathPending = false;
	FVector ProgressMark = FVector::ZeroVector;
	double ProgressMarkTime = 0.0;
	// Group drift (C4).
	double DriftUntil = -1.0;
	bool bDriftUsedThisLeg = false;
	FCrowdAvoidance Avoidance;
};
