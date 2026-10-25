#include "StealthCityGenerator.h"
#include "StealthBush.h"
#include "StealthGame.h"
#include "StealthObjective.h"
#include "StealthTypes.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// "Held Breath" palette placeholders until the modular kit exists.
	const FLinearColor LimewashColor(0.78f, 0.74f, 0.66f);
	const FLinearColor StoneColor(0.46f, 0.43f, 0.39f);
	const FLinearColor AdobeColor(0.55f, 0.38f, 0.27f);

	/** Level actors sorted by name, so every machine visits them in the same order. */
	template <typename T>
	TArray<T*> SortedLevelActors(UWorld* World)
	{
		TArray<T*> Result;
		for (TActorIterator<T> It(World); It; ++It)
		{
			Result.Add(*It);
		}
		Result.Sort([](const T& A, const T& B) { return A.GetName() < B.GetName(); });
		return Result;
	}

	UInstancedStaticMeshComponent* MakeLayer(AActor* Owner, USceneComponent* Root, const TCHAR* Name, UStaticMesh* Cube, UMaterialInterface* Material)
	{
		UInstancedStaticMeshComponent* Layer = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Layer->SetupAttachment(Root);
		Layer->SetStaticMesh(Cube);
		Layer->SetMaterial(0, Material);
		Layer->SetCollisionProfileName(TEXT("BlockAll"));
		Layer->SetCanEverAffectNavigation(true);
		Layer->SetMobility(EComponentMobility::Movable);
		return Layer;
	}
}

AStealthCityGenerator::AStealthCityGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bNetLoadOnClient = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UStaticMesh* Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	UMaterialInterface* Material = ShapeMaterial.Succeeded() ? ShapeMaterial.Object : nullptr;

	Walls = MakeLayer(this, Root, TEXT("Walls"), Cube, Material);
	Stone = MakeLayer(this, Root, TEXT("Stone"), Cube, Material);
	Adobe = MakeLayer(this, Root, TEXT("Adobe"), Cube, Material);
	HiddenCollision = MakeLayer(this, Root, TEXT("HiddenCollision"), Cube, Material);
	HiddenCollision->SetHiddenInGame(true);
}

UInstancedStaticMeshComponent* AStealthCityGenerator::GetKitLayer(UStaticMesh* Mesh)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* Existing = KitLayers.Find(Mesh))
	{
		return *Existing;
	}
	UInstancedStaticMeshComponent* Layer = NewObject<UInstancedStaticMeshComponent>(this);
	Layer->SetStaticMesh(Mesh);
	Layer->SetMobility(EComponentMobility::Movable);
	Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Layer->SetCanEverAffectNavigation(false);
	Layer->SetupAttachment(RootComponent);
	Layer->RegisterComponent();
	KitLayers.Add(Mesh, Layer);
	return Layer;
}

FVector AStealthCityGenerator::AddKitMesh(UStaticMesh* Mesh, const FVector& Center, const FVector2D& Footprint, float Yaw)
{
	const FBox Bounds = Mesh->GetBoundingBox();
	const FVector Size = Bounds.GetSize();
	const float Scale = FMath::Min(Footprint.X / FMath::Max(Size.X, 1.f), Footprint.Y / FMath::Max(Size.Y, 1.f));
	const FRotator Rotation(0.f, Yaw, 0.f);
	const FVector BoundsOffset = Rotation.RotateVector(FVector(-Bounds.GetCenter().X, -Bounds.GetCenter().Y, 0.f) * Scale);
	const FVector Location = Center + BoundsOffset + FVector(0.f, 0.f, -Bounds.Min.Z * Scale);
	GetKitLayer(Mesh)->AddInstance(FTransform(Rotation, Location, FVector(Scale)), true);
	return Size * Scale;
}

void AStealthCityGenerator::BeginPlay()
{
	Super::BeginPlay();

	const TPair<UInstancedStaticMeshComponent*, FLinearColor> LayerColors[] = { { Walls, LimewashColor }, { Stone, StoneColor }, { Adobe, AdobeColor } };
	for (const auto& Layer : LayerColors)
	{
		if (UMaterialInstanceDynamic* Dynamic = Layer.Key->CreateAndSetMaterialInstanceDynamic(0))
		{
			Dynamic->SetVectorParameterValue(TEXT("Color"), Layer.Value);
		}
	}
}

void AStealthCityGenerator::ClearLayout()
{
	Walls->ClearInstances();
	Stone->ClearInstances();
	Adobe->ClearInstances();
	HiddenCollision->ClearInstances();
	for (const auto& Layer : KitLayers)
	{
		Layer.Value->ClearInstances();
	}
	Cells.Reset();
}

bool AStealthCityGenerator::CellBlockedByLevel(const FVector& Center, const FVector& Extent) const
{
	// Raised above the floor so only real level geometry (courses, terrain bumps) counts.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StealthCityCell), false, this);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	const FVector BoxCenter = Center + FVector(0.f, 0.f, Extent.Z + 30.f);
	return GetWorld()->OverlapAnyTestByObjectType(BoxCenter, FQuat::Identity, Objects, FCollisionShape::MakeBox(Extent), Params);
}

void AStealthCityGenerator::AddBox(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size, float Yaw)
{
	// Engine cube: 100 cm, pivot at its centre. Center.Z is the base of the box.
	const FTransform Transform(FRotator(0.f, Yaw, 0.f), Center + FVector(0.f, 0.f, Size.Z * 0.5f), Size / 100.f);
	Target->AddInstance(Transform, true);
}

void AStealthCityGenerator::BuildModule(EModule Module, const FVector& Center, float Yaw, FRandomStream& Stream)
{
	// The stream is consumed identically with or without kit meshes, so layouts stay seed-stable.
	switch (Module)
	{
	case EModule::House:
	case EModule::Hall:
	{
		const bool bHall = Module == EModule::Hall;
		const float Height = bHall ? Stream.FRandRange(380.f, 450.f) : Stream.FRandRange(350.f, 500.f);
		const FVector2D Footprint = bHall ? FVector2D(800.f, 450.f) : FVector2D(700.f, 700.f);
		if (UStaticMesh* Kit = HouseMesh.LoadSynchronous())
		{
			const FVector Placed = AddKitMesh(Kit, Center, Footprint, Yaw);
			AddBox(HiddenCollision, Center, Placed, Yaw);
			break;
		}
		AddBox(Walls, Center, FVector(Footprint.X, Footprint.Y, Height), Yaw);
		AddBox(Stone, Center + FVector(0.f, 0.f, Height), FVector(Footprint.X + 60.f, Footprint.Y + 60.f, 30.f), Yaw);
		break;
	}
	case EModule::Stall:
	default:
	{
		// Two waist-high market counters with a street between them: vaultable, and crowd cover.
		const FVector Offset = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(0.f, 220.f, 0.f));
		UStaticMesh* Kit = StallMesh.LoadSynchronous();
		for (const FVector& CounterCenter : { Center + Offset, Center - Offset })
		{
			if (Kit)
			{
				AddKitMesh(Kit, CounterCenter, FVector2D(340.f, 200.f), Yaw);
			}
			AddBox(Kit ? HiddenCollision.Get() : Adobe.Get(), CounterCenter, FVector(320.f, 160.f, 100.f), Yaw);
		}
		break;
	}
	}
}

void AStealthCityGenerator::BuildTemple(const FVector& Center)
{
	// Stepped platform with mantle-height tiers, crowned by a shrine. With a kit mesh the tiers stay as collision.
	UStaticMesh* Kit = TempleMesh.LoadSynchronous();
	if (Kit)
	{
		AddKitMesh(Kit, Center, FVector2D(820.f, 820.f), 0.f);
	}
	UInstancedStaticMeshComponent* TierLayer = Kit ? HiddenCollision.Get() : Stone.Get();
	UInstancedStaticMeshComponent* ShrineLayer = Kit ? HiddenCollision.Get() : Walls.Get();
	AddBox(TierLayer, Center, FVector(800.f, 800.f, 150.f), 0.f);
	AddBox(TierLayer, Center + FVector(0.f, 0.f, 150.f), FVector(600.f, 600.f, 150.f), 0.f);
	AddBox(TierLayer, Center + FVector(0.f, 0.f, 300.f), FVector(400.f, 400.f, 150.f), 0.f);
	AddBox(ShrineLayer, Center + FVector(0.f, 0.f, 450.f), FVector(240.f, 240.f, 240.f), 0.f);
}

void AStealthCityGenerator::Build(int32 Seed)
{
	ClearLayout();
	FRandomStream Stream(Seed);

	const float BlockSize = CellSize - 2.f * StreetWidth;
	const FVector BlockExtent(BlockSize * 0.5f, BlockSize * 0.5f, 200.f);
	const int32 NumCells = CellsX * CellsY;
	Cells.Init(EStealthCellKind::Open, NumCells);

	auto CellCenter = [this](int32 X, int32 Y)
	{
		return FVector(GridOrigin.X + (X + 0.5f) * CellSize, GridOrigin.Y + (Y + 0.5f) * CellSize, 0.f);
	};

	// Pass 1 (no randomness): reserved cells.
	TArray<int32> TempleCandidates;
	for (int32 Y = 0; Y < CellsY; ++Y)
	{
		for (int32 X = 0; X < CellsX; ++X)
		{
			const FVector Center = CellCenter(X, Y);
			const int32 Index = Y * CellsX + X;
			if (Center.X < SpawnZoneMaxX || Center.Size2D() < PlazaRadius || CellBlockedByLevel(Center, BlockExtent))
			{
				Cells[Index] = EStealthCellKind::Reserved;
			}
			else if (X >= CellsX - 2)
			{
				TempleCandidates.Add(Index);
			}
		}
	}

	// Pass 2: temple, then every other free cell.
	const int32 TempleIndex = TempleCandidates.Num() > 0 ? TempleCandidates[Stream.RandRange(0, TempleCandidates.Num() - 1)] : INDEX_NONE;
	TArray<FVector> GardenSpots;
	TArray<FVector> OpenSpots;
	FVector TempleFront = FVector::ZeroVector;
	const float YawChoices[] = { 0.f, 90.f, 180.f, 270.f };

	for (int32 Y = 0; Y < CellsY; ++Y)
	{
		for (int32 X = 0; X < CellsX; ++X)
		{
			const int32 Index = Y * CellsX + X;
			const FVector Center = CellCenter(X, Y);
			if (Cells[Index] == EStealthCellKind::Reserved)
			{
				if (Center.X >= SpawnZoneMaxX && Center.Size2D() < PlazaRadius + CellSize)
				{
					OpenSpots.Add(Center);
				}
				continue;
			}
			if (Index == TempleIndex)
			{
				Cells[Index] = EStealthCellKind::Temple;
				BuildTemple(Center);
				TempleFront = Center - FVector(BlockSize * 0.5f + 180.f, 0.f, 0.f);
				continue;
			}

			const float Roll = Stream.FRand();
			const float Yaw = YawChoices[Stream.RandRange(0, 3)];
			if (Roll < BuildingChance)
			{
				Cells[Index] = EStealthCellKind::Building;
				const float ModuleRoll = Stream.FRand();
				const EModule Module = ModuleRoll < 0.6f ? EModule::House : (ModuleRoll < 0.85f ? EModule::Hall : EModule::Stall);
				BuildModule(Module, Center, Yaw, Stream);
				// A street spot beside the building (objective "door").
				const FVector Side = FRotator(0.f, YawChoices[Stream.RandRange(0, 3)], 0.f).RotateVector(FVector(BlockSize * 0.5f + 160.f, 0.f, 0.f));
				OpenSpots.Add(Center + Side);
			}
			else if (Roll < BuildingChance + GardenChance)
			{
				Cells[Index] = EStealthCellKind::Garden;
				const FVector Offset = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(220.f, 0.f, 0.f));
				GardenSpots.Add(Center + Offset);
				GardenSpots.Add(Center - Offset);
				OpenSpots.Add(Center + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(0.f, 300.f, 0.f)));
			}
			else if (Roll < BuildingChance + GardenChance + LowWallChance)
			{
				Cells[Index] = EStealthCellKind::LowWall;
				const FVector Offset = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(0.f, 200.f, 0.f));
				AddBox(Adobe, Center + Offset, FVector(600.f, 40.f, 110.f), Yaw);
				AddBox(Adobe, Center - Offset, FVector(600.f, 40.f, 110.f), Yaw);
				OpenSpots.Add(Center);
			}
			else
			{
				Cells[Index] = EStealthCellKind::Open;
				OpenSpots.Add(Center);
			}
		}
	}

	if (TempleIndex == INDEX_NONE)
	{
		TempleFront = OpenSpots.Num() > 0 ? OpenSpots.Last() : FVector(2000.f, 0.f, 0.f);
	}

	PlaceObjectivesAndBushes(GardenSpots, OpenSpots, TempleFront, Stream);
	BuiltSeed = Seed;
	UE_LOG(LogStealth, Log, TEXT("City built for seed %d: %d walls, %d stone, %d adobe pieces, %d garden spots, %d open spots."),
		Seed, Walls->GetInstanceCount(), Stone->GetInstanceCount(), Adobe->GetInstanceCount(), GardenSpots.Num(), OpenSpots.Num());
}

void AStealthCityGenerator::PlaceObjectivesAndBushes(const TArray<FVector>& GardenSpots, const TArray<FVector>& OpenSpots, const FVector& TempleFront, FRandomStream& Stream)
{
	TArray<FVector> Pool = OpenSpots;
	auto TakeSpot = [&Pool, &Stream](TFunctionRef<bool(const FVector&)> Filter) -> TOptional<FVector>
	{
		TArray<int32> Matches;
		for (int32 i = 0; i < Pool.Num(); ++i)
		{
			if (Filter(Pool[i]))
			{
				Matches.Add(i);
			}
		}
		if (Matches.Num() == 0)
		{
			return {};
		}
		const int32 Chosen = Matches[Stream.RandRange(0, Matches.Num() - 1)];
		const FVector Spot = Pool[Chosen];
		Pool.RemoveAt(Chosen);
		return Spot;
	};

	for (AStealthObjective* Objective : SortedLevelActors<AStealthObjective>(GetWorld()))
	{
		TOptional<FVector> Spot;
		if (Objective->Stage == STEALTH_FINAL_STAGE)
		{
			Spot = TempleFront;
		}
		else
		{
			// Each team works its own half of the city.
			const bool bSouth = Objective->OwningTeam == 0;
			Spot = TakeSpot([bSouth](const FVector& P) { return bSouth ? P.Y < -300.f : P.Y > 300.f; });
			if (!Spot.IsSet())
			{
				Spot = TakeSpot([](const FVector&) { return true; });
			}
		}
		if (Spot.IsSet())
		{
			Objective->SetActorLocation(FVector(Spot->X, Spot->Y, Objective->GetActorLocation().Z));
		}
	}

	TArray<FVector> BushSpots = GardenSpots;
	for (AStealthBush* Bush : SortedLevelActors<AStealthBush>(GetWorld()))
	{
		FVector Spot;
		if (BushSpots.Num() > 0)
		{
			Spot = BushSpots[0];
			BushSpots.RemoveAt(0);
		}
		else if (TOptional<FVector> Open = TakeSpot([](const FVector&) { return true; }))
		{
			Spot = *Open + FVector(250.f, 0.f, 0.f);
		}
		else
		{
			continue;
		}
		Bush->SetActorLocation(FVector(Spot.X, Spot.Y, Bush->GetActorLocation().Z));
	}
}

bool AStealthCityGenerator::ValidateReachability() const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		return false;
	}
	TArray<APlayerStart*> TeamStarts;
	for (int32 Team = 0; Team < STEALTH_NUM_TEAMS; ++Team)
	{
		const FName Tag(*FString::Printf(TEXT("Team%d"), Team));
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == Tag)
			{
				TeamStarts.Add(*It);
				break;
			}
		}
	}

	// Starts float above the floor and objectives carve the nav mesh, so both ends are projected first.
	const FVector QueryExtent(300.f, 300.f, 500.f);
	bool bAllReachable = true;
	for (TActorIterator<AStealthObjective> It(GetWorld()); It; ++It)
	{
		FNavLocation Goal;
		if (!NavSystem->ProjectPointToNavigation(It->GetActorLocation(), Goal, QueryExtent))
		{
			bAllReachable = false;
			UE_LOG(LogStealth, Warning, TEXT("Seed %d: objective '%s' is off the nav mesh."), BuiltSeed, *It->DisplayName.ToString());
			continue;
		}
		for (const APlayerStart* Start : TeamStarts)
		{
			FNavLocation From;
			if (!NavSystem->ProjectPointToNavigation(Start->GetActorLocation(), From, QueryExtent))
			{
				bAllReachable = false;
				UE_LOG(LogStealth, Warning, TEXT("Seed %d: spawn %s is off the nav mesh."), BuiltSeed, *Start->PlayerStartTag.ToString());
				continue;
			}
			UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From.Location, Goal.Location);
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				bAllReachable = false;
				UE_LOG(LogStealth, Warning, TEXT("Seed %d: objective '%s' unreachable from %s."), BuiltSeed, *It->DisplayName.ToString(), *Start->PlayerStartTag.ToString());
			}
		}
	}
	UE_LOG(LogStealth, Log, TEXT("Seed %d reachability: %s."), BuiltSeed, bAllReachable ? TEXT("all objectives reachable from both team spawns") : TEXT("FAILED"));
	return bAllReachable;
}
