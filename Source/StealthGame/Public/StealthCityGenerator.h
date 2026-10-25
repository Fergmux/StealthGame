#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthCityGenerator.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** What a city grid cell became for one seed. */
UENUM()
enum class EStealthCellKind : uint8
{
	Open,
	Reserved,
	Building,
	Temple,
	Garden,
	LowWall
};

/**
 * Builds the round's city from a seed, identically on every machine (PROTOTYPE, see ADR backlog).
 * The layout is a grid of cells separated by streets, so every open cell stays reachable. Each cell
 * becomes a building module, a temple, a bush garden, a low traversal wall or open space. Geometry is
 * local instanced meshes (never replicated); the server and every client run Build with the same seed
 * (AStealthGameState::RoundSeed), so collision, bushes and objective sites agree everywhere.
 * Objectives and bushes already placed in the level are moved onto generated sites.
 */
UCLASS()
class STEALTHGAME_API AStealthCityGenerator : public AActor
{
	GENERATED_BODY()

public:
	AStealthCityGenerator();

	/** Rebuilds the layout for Seed. Deterministic: same seed, same level -> same result. */
	void Build(int32 Seed);

	/** Seed of the current layout, or INDEX_NONE if nothing was built. */
	int32 GetBuiltSeed() const { return BuiltSeed; }

	/** Server: logs whether every objective is reachable on the nav mesh from both team spawns. */
	bool ValidateReachability() const;

	/** Grid origin (south-west corner), cm. */
	UPROPERTY(EditAnywhere, Category = "City")
	FVector2D GridOrigin = FVector2D(-4500.f, -4500.f);

	UPROPERTY(EditAnywhere, Category = "City", meta = (ClampMin = "1"))
	int32 CellsX = 6;

	UPROPERTY(EditAnywhere, Category = "City", meta = (ClampMin = "1"))
	int32 CellsY = 6;

	/** Cell size including the street around a block, cm. */
	UPROPERTY(EditAnywhere, Category = "City", meta = (ClampMin = "600"))
	float CellSize = 1500.f;

	/** Width of the street left free on each side of a block, cm. */
	UPROPERTY(EditAnywhere, Category = "City", meta = (ClampMin = "200"))
	float StreetWidth = 350.f;

	/** Cells whose centre lies west of this X stay clear (team spawn zone), cm. */
	UPROPERTY(EditAnywhere, Category = "City")
	float SpawnZoneMaxX = -2300.f;

	/** Radius around the world origin kept as an open plaza for the crowd, cm. */
	UPROPERTY(EditAnywhere, Category = "City", meta = (ClampMin = "0"))
	float PlazaRadius = 1300.f;

	/** Probabilities for non-reserved cells (remainder = open). */
	UPROPERTY(EditAnywhere, Category = "City", meta = (ClampMin = "0", ClampMax = "1"))
	float BuildingChance = 0.5f;

	UPROPERTY(EditAnywhere, Category = "City", meta = (ClampMin = "0", ClampMax = "1"))
	float GardenChance = 0.2f;

	UPROPERTY(EditAnywhere, Category = "City", meta = (ClampMin = "0", ClampMax = "1"))
	float LowWallChance = 0.15f;

	/** Optional kit meshes. When set, that module's boxes become invisible collision and this mesh is drawn, fitted to the module footprint. */
	UPROPERTY(EditAnywhere, Category = "City|Kit")
	TSoftObjectPtr<UStaticMesh> HouseMesh;

	UPROPERTY(EditAnywhere, Category = "City|Kit")
	TSoftObjectPtr<UStaticMesh> TempleMesh;

	UPROPERTY(EditAnywhere, Category = "City|Kit")
	TSoftObjectPtr<UStaticMesh> StallMesh;

protected:
	virtual void BeginPlay() override;

	enum class EModule : uint8 { House, Hall, Stall };

	/** Instanced visual layer for a kit mesh (no collision), created on first use. */
	UInstancedStaticMeshComponent* GetKitLayer(UStaticMesh* Mesh);

	/** Places Mesh centred on Center, uniformly scaled to fit Footprint (cm), base on the ground. Returns the placed size. */
	FVector AddKitMesh(UStaticMesh* Mesh, const FVector& Center, const FVector2D& Footprint, float Yaw);

	/** Collision-only boxes behind kit meshes (hidden in game, still block and shape the nav mesh). */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> HiddenCollision;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>> KitLayers;

	void ClearLayout();
	bool CellBlockedByLevel(const FVector& Center, const FVector& Extent) const;
	void AddBox(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size, float Yaw);
	void BuildModule(EModule Module, const FVector& Center, float Yaw, FRandomStream& Stream);
	void BuildTemple(const FVector& Center);
	void PlaceObjectivesAndBushes(const TArray<FVector>& GardenSpots, const TArray<FVector>& OpenSpots, const FVector& TempleFront, FRandomStream& Stream);

	/** Building walls (limewash). */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Walls;

	/** Roofs, platforms and temple tiers (stone). */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Stone;

	/** Low walls and stalls (adobe) — waist-high pieces are vault/traversal opportunities. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Adobe;

	int32 BuiltSeed = INDEX_NONE;
	TArray<EStealthCellKind> Cells;
};
