#include "StealthObjective.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor ObjectiveIdleColor(0.55f, 0.47f, 0.35f);   // stone
	const FLinearColor ObjectiveWorkColor(0.85f, 0.65f, 0.25f);   // worked: warm gold pulse
	const FLinearColor ObjectiveDoneColor(0.25f, 0.22f, 0.20f);   // spent
}

AStealthObjective::AStealthObjective()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	RootComponent = Marker;
	Marker->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.2f));
	Marker->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Marker->SetStaticMesh(CylinderMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Marker->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AStealthObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStealthObjective, bCompleted);
	DOREPLIFETIME(AStealthObjective, InteractorCount);
}

void AStealthObjective::BeginPlay()
{
	Super::BeginPlay();
	UpdateVisuals();
}

bool AStealthObjective::IsInRange(const AStealthCharacter* Character) const
{
	return Character && FVector::Dist2D(Character->GetActorLocation(), GetActorLocation()) <= UStealthSettings::Get()->ObjectiveInteractRange;
}

void AStealthObjective::SetInteractor(AStealthCharacter* Character, bool bInteracting)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	if (bInteracting && !bCompleted)
	{
		Progress.FindOrAdd(Character) = 0.f;
	}
	else
	{
		Progress.Remove(Character);
	}
	InteractorCount = Progress.Num();
	UpdateVisuals();
}

void AStealthObjective::ResetObjective()
{
	if (HasAuthority())
	{
		bCompleted = false;
		Progress.Reset();
		InteractorCount = 0;
		UpdateVisuals();
	}
}

void AStealthObjective::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (InteractorCount > 0 && GetNetMode() != NM_DedicatedServer)
	{
		PulseTime += DeltaSeconds;
		UpdateVisuals();
	}

	if (!HasAuthority() || Progress.Num() == 0)
	{
		return;
	}

	AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	const float Required = UStealthSettings::Get()->ObjectiveInteractTime;
	AStealthCharacter* Completer = nullptr;

	for (auto It = Progress.CreateIterator(); It; ++It)
	{
		AStealthCharacter* Character = It->Key.Get();
		// Interaction breaks if the character leaves, runs, dies or stops being active.
		if (!Character || !Character->IsActiveAgent() || Character->IsRunning() || !IsInRange(Character))
		{
			It.RemoveCurrent();
			continue;
		}
		It->Value += DeltaSeconds;
		if (It->Value >= Required && !Completer)
		{
			Completer = Character;
		}
	}

	const int32 NewCount = Progress.Num();
	if (NewCount != InteractorCount)
	{
		InteractorCount = NewCount;
		UpdateVisuals();
	}

	if (Completer && GameMode)
	{
		bCompleted = true;
		Progress.Reset();
		InteractorCount = 0;
		UpdateVisuals();
		GameMode->OnObjectiveCompleted(this, Completer);
	}
}

void AStealthObjective::OnRep_Visuals()
{
	UpdateVisuals();
}

void AStealthObjective::UpdateVisuals()
{
	if (!Marker)
	{
		return;
	}
	UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Marker->GetMaterial(0));
	if (!Dynamic)
	{
		Dynamic = Marker->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (!Dynamic)
	{
		return;
	}
	FLinearColor Color = ObjectiveIdleColor;
	if (bCompleted)
	{
		Color = ObjectiveDoneColor;
	}
	else if (InteractorCount > 0)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(PulseTime * 6.f);
		Color = FMath::Lerp(ObjectiveIdleColor, ObjectiveWorkColor, Pulse);
	}
	Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
}
