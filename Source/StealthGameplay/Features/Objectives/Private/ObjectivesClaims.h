#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "View/CourtFixtureId.h"

class APawn;
class IStealthFaultSink;

/**
 * The stand-point claim table (ADR-0020 sections 5.1 and 5.3; GDD OC-7, OC-8; story oc-006). "Physically occupied" is
 * implemented as claims by ACTIVE Interact holds: OnInteractExecuted claims one of the fixture's FixtureInteractSideCount
 * faces (the free face nearest the pawn's position in that slot, ties to the lower face index) and records the pawn's
 * control epoch with it (the completion gate's (c)); OnInteractEnded releases only that pawn's own claim. A body merely
 * standing at a face holds no claim, so nobody can lock a fixture by parking at it. Server-only, kind-blind (no pawn kind,
 * team or role is read) and keyed by fixture; every operation is O(1) over at most four faces.
 *
 * Usage:
 *   const int32 Face = Claims.Claim(Court, Pawn, Pawn->GetActorLocation(), Faces, SideCount, ControlEpoch);
 *   const int32 Count = Claims.GetClaimCount(Court);   // the predicate's "full fixture" input
 *   Claims.Release(Court, Pawn, &RecordedEpoch);
 */
class FObjectivesClaimTable
{
public:
	/** Faces a fixture can have (FixtureInteractSideCount's hard maximum). */
	static constexpr int32 MaxFaces = 4;

	/** One face's claim. */
	struct FClaim
	{
		TWeakObjectPtr<const APawn> Pawn;
		uint32 ControlEpoch = 0;
		bool bHeld = false;
	};

	/**
	 * Claims the free face nearest PawnLocation among the first SideCount faces (ties to the lower index) for Pawn, recording
	 * ControlEpoch. Faces are the fixture's face stand points (fewer or none: the lowest free index is taken). A pawn that
	 * already holds a claim at Court keeps it (and its recorded epoch). Returns the face index, or INDEX_NONE when every face
	 * is taken (no claim, nothing changed).
	 */
	int32 Claim(FCourtFixtureId Court, const APawn* Pawn, const FVector& PawnLocation, TConstArrayView<FVector> Faces, int32 SideCount, uint32 ControlEpoch);

	/** Releases Pawn's own claim at Court. True when it held one; OutControlEpoch (optional) receives the recorded epoch. */
	bool Release(FCourtFixtureId Court, const APawn* Pawn, uint32* OutControlEpoch = nullptr);

	/** Active-hold claims at Court (public occupancy: what every observer can see). */
	int32 GetClaimCount(FCourtFixtureId Court) const;

	/** Pawn's claim at Court, or null. */
	const FClaim* FindClaim(FCourtFixtureId Court, const APawn* Pawn) const;

	/** The face index Pawn holds at Court, or INDEX_NONE. */
	int32 FindFace(FCourtFixtureId Court, const APawn* Pawn) const;

	/** Courts with at least one claim. */
	void GetClaimedCourts(TArray<FCourtFixtureId>& OutCourts) const;

	/** Drops every claim (round reset, Deinitialize). */
	void Reset() { Fixtures.Reset(); }

#if !UE_BUILD_SHIPPING
	/**
	 * The drain-end consistency check (ADR-0020 5.1; non-Shipping only): for every court in InteractingSlots or with a claim,
	 * the claim count must equal the number of IIF Interacting slots targeting it. Each mismatching court reports one
	 * InvariantViolation to Faults (Detail: the court's id) the first time it mismatches, never again while it stays
	 * mismatched. Returns the number of mismatching courts.
	 */
	int32 CheckConsistency(const TMap<FCourtFixtureId, int32>& InteractingSlots, IStealthFaultSink* Faults);
#endif

private:
	struct FFixture
	{
		FClaim Faces[MaxFaces];
	};

	TMap<FCourtFixtureId, FFixture> Fixtures;
#if !UE_BUILD_SHIPPING
	TSet<FCourtFixtureId> ReportedMismatches;
#endif
};
