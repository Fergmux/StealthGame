#include "StealthBush.h"
#include "StealthCharacter.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AStealthBush::AStealthBush()
{
	PrimaryActorTick.bCanEverTick = false;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetBoxExtent(FVector(120.f, 120.f, 100.f));
	// Concealment epic (ADR-0017): the bush is now only the visual planter and the stand-in Manifest's BushSite; the
	// production hide volume is the local ABushVolumeSet's box. The prototype's overlap-driven concealment is off.
	Volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Volume->SetGenerateOverlapEvents(false);
	RootComponent = Volume;

	Foliage = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Foliage"));
	Foliage->SetupAttachment(Volume);
	Foliage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Foliage->SetRelativeScale3D(FVector(2.6f, 2.6f, 1.9f));
	Foliage->SetRelativeLocation(FVector(0.f, 0.f, -10.f));
	Foliage->CastShadow = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Foliage->SetStaticMesh(SphereMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Foliage->SetMaterial(0, ShapeMaterial.Object);
	}
}

FBox2D AStealthBush::GetFootprint2D() const
{
	if (Volume == nullptr)
	{
		return FBox2D(ForceInit);
	}
	const FBox Bounds = Volume->Bounds.GetBox();
	return FBox2D(FVector2D(Bounds.Min), FVector2D(Bounds.Max));
}

void AStealthBush::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyFoliageColor();
}

void AStealthBush::ApplyFoliageColor()
{
	if (UMaterialInstanceDynamic* Dynamic = Foliage->CreateAndSetMaterialInstanceDynamic(0))
	{
		Dynamic->SetVectorParameterValue(TEXT("Color"), FoliageColor);
	}
}

void AStealthBush::BeginPlay()
{
	Super::BeginPlay();
	ApplyFoliageColor();

	// Concealment epic: hiding is the production Hide(Enter) command (UConcealmentService); no overlap conceals anyone.
}

void AStealthBush::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

