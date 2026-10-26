#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Templates/Function.h"

/**
 * One pawn as crowd perception sees it: public facts only (ADR-0016 section 5; GDD CNB-R2). There is no field for a
 * player, a binding, a team or a participant state, so no query can branch on kind.
 */
struct FCrowdPerceivedPawn
{
	/** An opaque per-pawn key (the pawn's object key in the game, an index in a sim). Never an identity. */
	uint64 Key = 0;
	/** The pawn's spawn ordinal when it has one (every factory pawn has one); tests use it to pair runs. */
	uint32 Ordinal = MAX_uint32;
	FVector Location = FVector::ZeroVector;
	FVector VelocityCmPerSec = FVector::ZeroVector;
	/** Body facing (actor yaw), degrees. */
	float FacingYawDeg = 0.0f;
	/** Eye height above the location (for line of sight). */
	float EyeHeightCm = 160.0f;
	/** ICrowdPerceptionFilter::IsPerceivable (false while Concealed). */
	bool bPerceivable = true;
	/** ICrowdPerceptionFilter::IsSteeringObstacle (false while entering/exiting/emerging/concealed). */
	bool bObstacle = true;
	/** A planted (brace) or tending pawn: a static obstacle of CrowdStaticObstacleCm plus margin (SCL R16). */
	bool bStaticObstacle = false;
	/** Public locomotion facts the gaze catalogue reads (IsRunning, a traversal landing this frame). */
	bool bRunning = false;
	bool bTraversalLanding = false;

	/** Horizontal speed. */
	double Speed2D() const { return VelocityCmPerSec.Size2D(); }
};

/** What a query is for: the two filter answers select different pawns. */
enum class ECrowdQuery : uint8
{
	/** Glance targets, conversation partners, reaction sources: only perceivable pawns. */
	Perception,
	/** Avoidance obstacles: only pawns that are steering obstacles. */
	Steering,
};

/**
 * FCrowdSpatialHash: the Director's uniform grid over the frame's public pawn facts (ADR-0016 section 4.2 and 5; GDD
 * CNB-R3.3; story cnb-008). Built once per frame (CrowdHashCellCm, 2 m cells) from every pawn, player-driven and
 * AI-driven alike; every neighbour query of the crowd (avoidance, glance, conversation, reaction) goes through it, so
 * they share one filter path (AC-CNB-36). Results are sorted by distance then key, so they are deterministic and do not
 * depend on insertion order. Plain C++.
 */
class STEALTHGAMEPLAY_API FCrowdSpatialHash
{
public:
	explicit FCrowdSpatialHash(double InCellCm = 200.0) : CellCm(FMath::Max(InCellCm, 10.0)) {}

	/** Rebuilds the grid from this frame's pawns (the previous frame's contents are dropped). */
	void Build(TConstArrayView<FCrowdPerceivedPawn> Pawns);

	/**
	 * Pawns within Radius (2D) of Center that pass Query's filter, excluding ExcludeKey, nearest first (ties by key).
	 * Out is reset. Allocation-free once Out has capacity.
	 */
	void Query(const FVector& Center, double Radius, ECrowdQuery Mode, uint64 ExcludeKey, TArray<const FCrowdPerceivedPawn*>& Out) const;

	/** The pawn with Key, or null (this frame's facts). */
	const FCrowdPerceivedPawn* Find(uint64 Key) const;

	int32 Num() const { return Pawns.Num(); }
	TConstArrayView<FCrowdPerceivedPawn> GetPawns() const { return Pawns; }
	/** How many times Build ran (the once-per-frame guard of the Director reads it). */
	uint64 GetBuildCount() const { return BuildCount; }

private:
	FIntPoint CellOf(const FVector& Location) const;

	double CellCm;
	TArray<FCrowdPerceivedPawn> Pawns;
	TMultiMap<FIntPoint, int32> Cells;
	TMap<uint64, int32> ByKey;
	uint64 BuildCount = 0;
};

/** One line-of-sight request (ADR-0016 section 5). */
struct FCrowdRayRequest
{
	/** Who asked (an ordinal); round-robin is per requester. */
	uint32 Requester = 0;
	/** What the ray is for (the requester's own tag; returned unchanged). */
	uint32 Tag = 0;
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	/** The time of the stimulus that caused the request: a deferred ray reports it unchanged. */
	double StimulusTime = 0.0;
};

/**
 * FCrowdRayBudget: at most CrowdRaysPerFrameLimit (8) line-of-sight rays per frame, round-robin over requesters, FIFO
 * within a requester; a deferred ray keeps its stimulus time, so a reaction delay measured from the stimulus is
 * unaffected (story cnb-008). The trace itself is injected (LineTraceSingleByChannel on the crowd sight channel in the
 * game, a geometric test in specs).
 */
class STEALTHGAMEPLAY_API FCrowdRayBudget
{
public:
	using FTrace = TFunctionRef<bool(const FVector& From, const FVector& To)>;
	using FDeliver = TFunctionRef<void(const FCrowdRayRequest& Request, bool bVisible)>;

	void Request(const FCrowdRayRequest& Ray) { Queue.Add(Ray); }

	/** Runs up to Limit rays this frame (one per requester per pass, oldest first) and delivers each result. */
	int32 Process(int32 Limit, FTrace Trace, FDeliver Deliver);

	int32 NumPending() const { return Queue.Num(); }
	void Reset() { Queue.Reset(); }

private:
	TArray<FCrowdRayRequest> Queue;
};
