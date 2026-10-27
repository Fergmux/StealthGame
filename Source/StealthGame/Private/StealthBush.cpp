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
	Volume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Volume->SetGenerateOverlapEvents(true);
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

	if (HasAuthority())
	{
		Volume->OnComponentBeginOverlap.AddDynamic(this, &AStealthBush::OnVolumeBeginOverlap);
		Volume->OnComponentEndOverlap.AddDynamic(this, &AStealthBush::OnVolumeEndOverlap);

		// Characters already inside when the round starts.
		TArray<AActor*> Inside;
		Volume->GetOverlappingActors(Inside, AStealthCharacter::StaticClass());
		for (AActor* Actor : Inside)
		{
			CastChecked<AStealthCharacter>(Actor)->SetInBush(this, true);
		}
	}
}

void AStealthBush::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		TArray<AActor*> Inside;
		Volume->GetOverlappingActors(Inside, AStealthCharacter::StaticClass());
		for (AActor* Actor : Inside)
		{
			CastChecked<AStealthCharacter>(Actor)->SetInBush(this, false);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AStealthBush::OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AStealthCharacter* Character = Cast<AStealthCharacter>(OtherActor))
	{
		Character->SetInBush(this, true);
	}
}

void AStealthBush::OnVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AStealthCharacter* Character = Cast<AStealthCharacter>(OtherActor))
	{
		Character->SetInBush(this, false);
	}
}
