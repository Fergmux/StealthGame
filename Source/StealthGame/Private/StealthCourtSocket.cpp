// Objectives Chain story oc-022: the LSC court-socket stand-in that replaced the prototype objective actor. See
// StealthCourtSocket.h.

#include "View/StealthCourtSocket.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

AStealthCourtSocket::AStealthCourtSocket()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Fixture = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	RootComponent = Fixture;
	Fixture->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.2f));
	Fixture->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Fixture->SetStaticMesh(CylinderMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Fixture->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AStealthCourtSocket::GetCourts(const UWorld* World, TArray<const AStealthCourtSocket*>& OutCourts)
{
	OutCourts.Reset();
	if (World == nullptr)
	{
		return;
	}
	for (TActorIterator<AStealthCourtSocket> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			OutCourts.Add(*It);
		}
	}
	OutCourts.Sort([](const AStealthCourtSocket& A, const AStealthCourtSocket& B) { return A.GetFName().LexicalLess(B.GetFName()); });
}

const AStealthCourtSocket* AStealthCourtSocket::FindCourt(const UWorld* World, FCourtFixtureId Court)
{
	TArray<const AStealthCourtSocket*> Courts;
	GetCourts(World, Courts);
	const int32 Index = static_cast<int32>(Court.Value) - 1;
	return Courts.IsValidIndex(Index) ? Courts[Index] : nullptr;
}

FCourtFixtureId AStealthCourtSocket::FindCourtInReach(const UWorld* World, const FVector& Location, double RangeCm)
{
	TArray<const AStealthCourtSocket*> Courts;
	GetCourts(World, Courts);
	FCourtFixtureId Best;
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Courts.Num(); ++Index)
	{
		const double Distance = FVector::Dist2D(Courts[Index]->GetActorLocation(), Location);
		if (Distance <= RangeCm && Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = FCourtFixtureId(static_cast<uint16>(Index + 1));
		}
	}
	return Best;
}
