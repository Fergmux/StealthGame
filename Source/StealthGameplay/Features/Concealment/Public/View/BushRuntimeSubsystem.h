#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "View/BushRustle.h"
#include "View/ConcealmentTypes.h"

#include "BushRuntimeSubsystem.generated.h"

class ABushVolumeSet;
class UAudioComponent;
class UMaterialParameterCollection;

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

	// ---- Rustle (ADR-0017 section 9; story con-029) ---------------------------------------------------------------------

	/**
	 * Writes the client-local rustle pulse of a hide transition of any pawn: one slot of the 8-slot ring (the oldest),
	 * timed from the pose's StartServerTime, at the bush face on Side; the MPC slot when an MPC is configured; and the one
	 * shared SFX spatialised at the face (seeked when the write comes after the onset; nothing past the pulse's life). No
	 * pawn kind, LOD, camera mode or occupancy is an input. No-op for an unknown bush or on a dedicated server. Returns the
	 * slot written, or INDEX_NONE.
	 */
	int32 PushRustlePulse(uint16 BushId, uint8 Side, double StartServerTime, EBushRustleKind Kind, double NowServerTime);

	/** The ring (observation; the material reads the MPC). */
	const FBushRustleRing& GetRustleRing() const { return RustleRing; }

	/** Every pulse written since the build, in order (bounded; observation for the parity tests). */
	const TArray<FBushRustlePulse>& GetRustleLog() const { return RustleLog; }

	/** The SFX plays started (observation; an audio-less machine starts none). */
	int32 GetRustleSoundPlays() const { return RustleSoundPlays; }

	/** The rustle's Material Parameter Collection (Art epic; unset on the graybox). Slot i writes vectors RustleA_i / RustleB_i. */
	void SetRustleCollection(UMaterialParameterCollection* Collection) { RustleCollection = Collection; }

	/** The one shared SFX (the STAND-IN default until the Art epic records one). */
	void SetRustleSound(const FSoftObjectPath& Sound) { RustleSound = Sound; }
	const FSoftObjectPath& GetRustleSound() const { return RustleSound; }

		/** Broadcast after every accepted build (presentation hooks: the rustle positions, the HUD). */
	DECLARE_MULTICAST_DELEGATE(FOnBushRuntimeBuilt);
	FOnBushRuntimeBuilt OnBuilt;

private:
	void PlayRustleSound(const FBushRustlePulse& Pulse, double SeekSec);

	TArray<FBushRuntime> Bushes;
	FBushRustleRing RustleRing;
	TArray<FBushRustlePulse> RustleLog;
	FSoftObjectPath RustleSound = BushRustle::DefaultSound();
	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> RustleCollection;
	/** The shared SFX pool (one component per ring slot, reused round-robin). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> RustlePool;
	int32 RustleSoundPlays = 0;
	TWeakObjectPtr<ABushVolumeSet> VolumeSet;
	EBushSiteReject LastReject = EBushSiteReject::None;
	bool bBuilt = false;
	int32 BuildCount = 0;
};
