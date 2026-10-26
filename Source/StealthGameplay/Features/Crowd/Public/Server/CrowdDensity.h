#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

class FCrowdDraws;
struct FCrowdTuning_Server;

/** Area types of the density model (GDD CNB-R13, F1; a court inherits its host area's type). Values index arrays. */
enum class ECrowdAreaType : uint8
{
	Causeway,
	Market,
	Temple,
	Residential,
	Count,
};

/** One CrowdRoute node as the density model sees it: geometry and area type only (no site role, no zone role). */
struct FCrowdRouteNode
{
	FVector Location = FVector::ZeroVector;
	ECrowdAreaType Area = ECrowdAreaType::Causeway;
	/** Node catchment area a_n, m^2. */
	double CatchmentM2 = 25.0;
	/** Node frontage L_n, cm (capacity clamp(floor(L_n / 100), 1, 4)). */
	double FrontageCm = 200.0;
};

/**
 * FCrowdDensityModel: F1 (GDD CNB-R13; story cnb-017). Inputs are only {N, the area-type map, geometry, the crowd's own
 * observed density from public positions}: never FSiteAssignment, zone roles or player positions, so permuting the site
 * assignment cannot change any output (AC-CNB-05).
 *   rho_t  = N * w_t / sum_t' (w_t' * A_t')                         target density per area type
 *   W_n    = w_t(n) * a_n * clamp((rho_t / max(rho_obs,t, 0.25 rho_t))^0.5, 0.5, 2)   destination weight
 *   cap_n  = clamp(floor(L_n / 100 cm), 1, 4)                         node capacity (pawns of any kind)
 * Weights are computed at round start and on a coarse refresh, never per frame. Plain C++.
 */
class STEALTHGAMEPLAY_API FCrowdDensityModel
{
public:
	/** Sets the layout (nodes; A_t is the sum of catchments per type) and computes the targets for NpcCount. */
	void Initialize(TConstArrayView<FCrowdRouteNode> InNodes, int32 NpcCount, const FCrowdTuning_Server& Tuning);

	/** The area weight w_t (CrowdAreaWeight<Type>Mult). */
	static double AreaWeight(ECrowdAreaType Type, const FCrowdTuning_Server& Tuning);

	/** Target density rho_t per m^2. */
	double GetTargetDensity(ECrowdAreaType Type) const { return Target[static_cast<int32>(Type)]; }
	/** Walkable area A_t, m^2. */
	double GetArea(ECrowdAreaType Type) const { return Area[static_cast<int32>(Type)]; }
	/** Expected NPC count of a type at the target (rho_t * A_t). */
	double GetTargetCount(ECrowdAreaType Type) const { return GetTargetDensity(Type) * GetArea(Type); }

	/** Node capacity cap_n. */
	static int32 Capacity(const FCrowdRouteNode& Node);

	/**
	 * Recomputes the destination weights from the observed crowd counts per type (the coarse refresh). ObservedCount:
	 * crowd-driven pawns per area type, counted from public positions by the caller.
	 */
	void Refresh(const int32 ObservedCount[static_cast<int32>(ECrowdAreaType::Count)]);

	/** The current destination weights W_n (one per node). */
	TConstArrayView<double> GetDestinationWeights() const { return Weights; }

	/**
	 * Picks a destination node for Ordinal (ECrowdDecision::Destination over integer-scaled weights), excluding nodes
	 * whose occupancy (any kind) is at capacity and the node the pawn stands on. Returns INDEX_NONE when none is free.
	 */
	int32 PickDestination(uint32 Ordinal, int32 CurrentNode, TConstArrayView<int32> Occupancy, FCrowdDraws& Draws) const;

	/**
	 * Start placement (CNB-R1, R13; ADR-0016 section 9): Count start nodes drawn from the NpcRoutes stream of the round
	 * (sub-seed GetServerSubSeed(NpcRoutes, LayoutAttempt)), each type receiving its F1 share, respecting node capacity.
	 * Occupancy (optional, any kind: players already placed) is counted against capacity. Deterministic per sub-seed.
	 */
	TArray<int32> PlaceStarts(int32 Count, uint64 NpcRoutesSubSeed, TConstArrayView<int32> Occupancy) const;

	/** The node nearest Location (the observed type of a pawn), INDEX_NONE when there are no nodes. */
	int32 NearestNode(const FVector& Location) const;

	TConstArrayView<FCrowdRouteNode> GetNodes() const { return Nodes; }

private:
	TArray<FCrowdRouteNode> Nodes;
	TArray<double> Weights;
	double Target[static_cast<int32>(ECrowdAreaType::Count)] = {};
	double Area[static_cast<int32>(ECrowdAreaType::Count)] = {};
	double AreaWeights[static_cast<int32>(ECrowdAreaType::Count)] = {};
};

/**
 * The site-assignment blindness sentinel (AC-CNB-05; story cnb-017). The Objectives site-assignment query (when it
 * exists) calls NoteSiteAssignmentQuery(); inside a crowd scope (any Director or controller tick holds an FCrowdScope)
 * that is a server assert. Tests swap the handler to observe it.
 */
namespace StealthCrowdBlindness
{
	/** RAII marker: crowd code is running on this thread. */
	struct STEALTHGAMEPLAY_API FCrowdScope
	{
		FCrowdScope();
		~FCrowdScope();
	};

	/** True while a crowd scope is open on the game thread. */
	STEALTHGAMEPLAY_API bool IsInCrowdScope();

	/** Called by the site-assignment query: fires the assert handler when crowd code is asking. */
	STEALTHGAMEPLAY_API void NoteSiteAssignmentQuery();

	/** The assert handler (default: ensureAlwaysMsgf). Returns the previous one. */
	using FHandler = void (*)();
	STEALTHGAMEPLAY_API FHandler SetAssertHandler(FHandler Handler);

	/** How many crowd-scope site-assignment queries were seen (diagnostics; never logged with any identity). */
	STEALTHGAMEPLAY_API int32 GetViolationCount();
}
