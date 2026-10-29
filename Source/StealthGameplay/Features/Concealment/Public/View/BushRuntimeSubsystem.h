#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "View/ConcealmentTypes.h"

#include "BushRuntimeSubsystem.generated.h"

class ABushVolumeSet;

/**
 * One BushSite of the layout Manifest (ADR-0014 BushSite, ADR-0017 section 1; decision D49: the graybox Manifest carries
 * BushSite records). Public geometry, identical on every machine: the bush id, the footprint centre at ground level and
 * the open sides. STAND-IN source until the Level Socket Contract builds FLayoutManifest: the composition root derives the
 * records from L_Prototype's bush actors after the seeded layout placed them (every machine runs the same placement).
 */
struct FBushSiteRecord
{
	uint16 BushId = 0;
	FVector GroundCentre = FVector::ZeroVector;
	/** Bit k = side k open (ConcealmentTypes.h side convention). Empty is a V-BUSH-1 reject. */
	uint8 EntryAxisMask = ConcealmentGeometry::AllSidesMask;
};

/** Why a Manifest's bush sites were rejected (V-BUSH-1 and the record shape). */
enum class EBushSiteReject : uint8
{
	None,
	/** V-BUSH-1: a BushSite with no open side. */
	EmptyEntryAxis,
	/** A BushSite with id 0 or an id used twice. */
	BadBushId,
};

/**
 * UBushRuntimeSubsystem (ADR-0017 section 1; GDD CON-R1; story con-002): every machine (server and each client) builds the
 * same FBushRuntime per bush from the Manifest's BushSite records, and spawns exactly one locally owned, non-replicated
 * ABushVolumeSet holding one box per bush on the BushVolume object channel (engine fact E5: a bReplicates = false actor
 * spawned independently on every machine is valid local geometry from replicated data). Concealment spawns zero
 * replicated actors. Built once per layout; no per-frame work. A layout with a V-BUSH-1 violation is rejected whole:
 * nothing is built and BuildFromSites returns false (the composition root then aborts the layout, so no round starts).
 *
 * Usage:
 *   UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(World);
 *   Bushes->BuildFromSites(Records);
 *   if (const FBushRuntime* Bush = Bushes->Find(3)) { ... }
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API UBushRuntimeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Game and PIE worlds (server and clients); never editor preview worlds. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** The subsystem of World, or null. */
	static UBushRuntimeSubsystem* Get(const UWorld* World);

	/** V-BUSH-1 and the record shape over a whole site list (the first failing record decides). Pure. */
	static EBushSiteReject ValidateSites(TConstArrayView<FBushSiteRecord> Sites);

	/**
	 * Builds the runtime from the Manifest's sites, replacing any previous layout's (and its volume set). Rejected (false,
	 * nothing built, the previous runtime cleared) when ValidateSites fails. Spawns the volume set only in a world.
	 */
	bool BuildFromSites(TConstArrayView<FBushSiteRecord> Sites);

	/** Drops the runtime and destroys the volume set. */
	void Clear();

	/** The bush with Id, or null. */
	const FBushRuntime* Find(uint16 Id) const;

	/** Every bush, in id order. */
	const TArray<FBushRuntime>& GetBushes() const { return Bushes; }

	/** True once a layout was accepted. */
	bool IsBuilt() const { return bBuilt; }

	/** The last rejection (None after an accepted build). */
	EBushSiteReject GetLastReject() const { return LastReject; }

	/**
	 * The bush whose open-side entry reach holds Location (CON-R3 public geometry): horizontal distance to the footprint
	 * <= ReachCm, on an open side; the nearest such bush, or null. Clients use it to name the BushId of a Hide(Enter).
	 */
	const FBushRuntime* FindEnterableBush(const FVector& Location, double ReachCm) const;

	/** The bush whose footprint holds Location (horizontal), or null (an owner derives its BushId from its position). */
	const FBushRuntime* FindBushAt(const FVector& Location) const;

	/** The local volume set (null before a build or without a world). */
	ABushVolumeSet* GetVolumeSet() const { return VolumeSet.Get(); }

	/** Layout builds accepted since the subsystem started (observation). */
	int32 GetBuildCount() const { return BuildCount; }

	/** Broadcast after every accepted build (presentation hooks: the rustle positions, the HUD). */
	DECLARE_MULTICAST_DELEGATE(FOnBushRuntimeBuilt);
	FOnBushRuntimeBuilt OnBuilt;

private:
	TArray<FBushRuntime> Bushes;
	TWeakObjectPtr<ABushVolumeSet> VolumeSet;
	EBushSiteReject LastReject = EBushSiteReject::None;
	bool bBuilt = false;
	int32 BuildCount = 0;
};
