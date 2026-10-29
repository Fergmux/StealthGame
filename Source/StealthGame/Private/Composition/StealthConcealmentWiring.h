#pragma once

#include "CoreMinimal.h"
#include "Server/ConcealmentCore.h"
#include "Server/CrowdHerringTypes.h"
#include "View/BushRuntimeSubsystem.h"

class UStealthCompositionRoot;
class UWorld;

/**
 * The composition root's Concealment adapters (Concealment epic, stories con-002, con-009, con-023; ADR-0017; decision D49).
 *
 *   GatherBushSites      STAND-IN for the Level Socket Contract's FLayoutManifest BushSite records (D49: the graybox Manifest
 *                        carries them; LSC is not built): L_Prototype's bush actors after the seeded layout placed them. A
 *                        bush's id is the 1-based index of the world's AStealthBush actors sorted by name, its ground centre
 *                        the actor's footprint centre at the volume's bottom, every side open. Every machine runs the same
 *                        layout from the replicated RoundSeed, so every machine derives the same records.
 *   RebuildBushRuntime   builds UBushRuntimeSubsystem from the records on any machine (server and clients) and, on the
 *                        server, hands the bushes to UConcealmentService. False: V-BUSH-1 rejected the layout.
 *   FStealthCrowdHerringSites  Crowd's ICrowdHerringSites over the bush runtime (bushes only; the traversal and court herrings
 *                        stay off until their owners register sites): the nearest bush with an open side near the leg.
 *                        Geometry only, never occupancy.
 *   FStealthConcealmentFlags   the owner and team concealed flags' writers: the owner's UConcealmentOwnerViewComponent on its
 *                        controller and the Roster's SetTeammateConcealed (story con-023).
 */
namespace StealthConcealmentLayout
{
	/** The stand-in Manifest's BushSite records of World (see above). */
	void GatherBushSites(const UWorld* World, TArray<FBushSiteRecord>& OutSites);

	/** Rebuilds the bush runtime of World from the stand-in records (every machine). False when V-BUSH-1 rejects them. */
	bool RebuildBushRuntime(UWorld* World);
}

/** Crowd's bush herring sites (ADR-0016 section 3) over the bush runtime. */
class FStealthCrowdHerringSites final : public ICrowdHerringSites
{
public:
	explicit FStealthCrowdHerringSites(UWorld* InWorld) : World(InWorld) {}

	virtual bool FindTraversalNear(const FVector& From, double MaxCm, bool bClimb, FCrowdTraversalSite& Out) const override { return false; }
	virtual int32 GetCourtCount() const override { return 0; }
	virtual bool GetCourtStandPoint(int32 Court, const FVector& From, FCrowdCourtSite& Out) const override { return false; }
	virtual bool FindBush(const FVector& From, TConstArrayView<FVector> Path, double RadiusCm, FCrowdBushSite& Out) const override;
	virtual bool HasTraversals() const override { return false; }
	virtual bool HasBushes() const override;

	/** Where an NPC stands to enter by Side: just outside the planter edge (inside BushEntryReachCm). */
	static FVector EntryPoint(const FBushRuntime& Bush, uint8 Side);

private:
	TWeakObjectPtr<UWorld> World;
};

/** The owner / team concealed flag writers (IConcealmentFlagSink). */
class FStealthConcealmentFlags final : public IConcealmentFlagSink
{
public:
	explicit FStealthConcealmentFlags(UStealthCompositionRoot* InRoot) : Root(InRoot) {}

	virtual void SetOwnerConcealed(FParticipantId Participant, bool bConcealed) override;
	virtual void SetTeammateConcealed(FParticipantId Participant, bool bConcealed) override;

private:
	/** The game-instance composition root (outlives every world it wires). */
	UStealthCompositionRoot* Root = nullptr;
};
