#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Templates/Function.h"

/** One path request of a driven pawn (ADR-0016 section 4.1; story cnb-007). */
struct FCrowdPathRequest
{
	uint32 Ordinal = 0;
	/** The requester's leg id: a result is accepted only for the leg that asked. */
	uint32 LegId = 0;
	/** The requester's drive-token epoch: a result is accepted only while the pawn's epoch is unchanged. */
	uint32 ControlEpoch = 0;
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
};

/** A path result as delivered (at least two frames after the start for a navmesh query, E7). */
struct FCrowdPathResult
{
	uint32 Ordinal = 0;
	uint32 LegId = 0;
	uint32 ControlEpoch = 0;
	bool bValid = false;
	bool bPartial = false;
	/** String-pulled corners, start excluded. */
	TArray<FVector> Corners;
};

/**
 * Where paths come from (ADR-0016 section 4.1), behind an interface so the cost cut's coarse waypoint A* (story cnb-033)
 * can swap in. Pre-baked CrowdRoute edge paths answer synchronously with no query; everything else starts an async
 * query whose result the source hands back through the Director's delivery path.
 */
class ICrowdPathSource
{
public:
	virtual ~ICrowdPathSource() = default;

	/** True with OutCorners filled when From -> To follows a CrowdRoute edge (no FindPathAsync call). */
	virtual bool TryPrebakedEdge(const FVector& From, const FVector& To, TArray<FVector>& OutCorners) = 0;

	/** Starts one async query; the result arrives later through the Director (never in this call). */
	virtual void StartQuery(const FCrowdPathRequest& Request) = 0;
};

/**
 * FCrowdPathBudget: the Director's path-start budget (story cnb-007, GDD CNB-R15, F5). At most Limit async starts per
 * frame (CrowdPathStartsPerFrameLimit, 2), FIFO with round-robin deferral: a requester is served once per frame, a
 * hitch frame still honours the cap, and a deferred request is never dropped. A newer request of the same ordinal
 * replaces its pending one in place (the old leg is gone; its result would be refused anyway). Plain C++.
 */
class STEALTHGAMEPLAY_API FCrowdPathBudget
{
public:
	/** Queues a request (no query before OnLayoutReady: the Director refuses to start while the layout is not ready). */
	void Enqueue(const FCrowdPathRequest& Request);

	/** Starts up to Limit queued requests through Start this frame; returns the number started. */
	int32 StartThisFrame(int32 Limit, TFunctionRef<void(const FCrowdPathRequest&)> Start);

	int32 NumPending() const { return Pending.Num(); }
	/** Every request ever started (the AC "never drops" counter pairs it with Enqueue's count). */
	int64 GetStartedCount() const { return Started; }
	int64 GetEnqueuedCount() const { return Enqueued; }
	/** Requests replaced in place by a newer one of the same ordinal (not drops: the old leg no longer exists). */
	int64 GetReplacedCount() const { return Replaced; }
	void Reset() { Pending.Reset(); }

	/**
	 * The acceptance gate of a delivered result (ADR-0016 section 4.1): the requester's current leg id and the pawn's
	 * current control epoch must both match the result's.
	 */
	static bool Accept(const FCrowdPathResult& Result, uint32 CurrentLegId, uint32 CurrentControlEpoch)
	{
		return Result.LegId == CurrentLegId && Result.ControlEpoch == CurrentControlEpoch;
	}

private:
	TArray<FCrowdPathRequest> Pending;
	int64 Started = 0;
	int64 Enqueued = 0;
	int64 Replaced = 0;
};
