// Concealment epic (stories con-002, con-009, con-023): the composition root's Concealment adapters. See
// StealthConcealmentWiring.h.

#include "StealthConcealmentWiring.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Server/ConcealmentService.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthCompositionRoot.h"
#include "StealthBush.h"
#include "View/ConcealmentOwnerViewComponent.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace StealthConcealmentLayout
{
	namespace
	{
		/** The ground under a bush: a downward WorldStatic trace (the same geometry on every machine), else the volume bottom. */
		double GroundZOf(const UWorld& World, const AStealthBush& Bush, const FVector2D& Centre)
		{
			const FBox Bounds = Bush.GetComponentsBoundingBox(/*bNonColliding*/ true);
			const double Fallback = Bounds.IsValid ? Bounds.Min.Z : Bush.GetActorLocation().Z;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(StealthBushGround), false, &Bush);
			const FVector Start(Centre.X, Centre.Y, Fallback + 150.0);
			const FVector End(Centre.X, Centre.Y, Fallback - 500.0);
			return World.LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params) ? Hit.ImpactPoint.Z : Fallback;
		}
	}

	void GatherBushSites(const UWorld* World, TArray<FBushSiteRecord>& OutSites)
	{
		OutSites.Reset();
		if (World == nullptr)
		{
			return;
		}
		TArray<const AStealthBush*> Bushes;
		for (TActorIterator<AStealthBush> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Bushes.Add(*It);
			}
		}
		Bushes.Sort([](const AStealthBush& A, const AStealthBush& B) { return A.GetFName().LexicalLess(B.GetFName()); });
		for (int32 Index = 0; Index < Bushes.Num(); ++Index)
		{
			const FBox2D Footprint = Bushes[Index]->GetFootprint2D();
			const FVector2D Centre = Footprint.bIsValid ? Footprint.GetCenter() : FVector2D(Bushes[Index]->GetActorLocation());
			FBushSiteRecord Site;
			Site.BushId = static_cast<uint16>(Index + 1);
			Site.GroundCentre = FVector(Centre.X, Centre.Y, GroundZOf(*World, *Bushes[Index], Centre));
			Site.EntryAxisMask = ConcealmentGeometry::AllSidesMask;
			OutSites.Add(Site);
		}
	}

	bool RebuildBushRuntime(UWorld* World)
	{
		UBushRuntimeSubsystem* Runtime = UBushRuntimeSubsystem::Get(World);
		if (Runtime == nullptr)
		{
			return true; // no runtime in this world type (an editor preview): nothing to build
		}
		TArray<FBushSiteRecord> Sites;
		GatherBushSites(World, Sites);
		const bool bAccepted = Runtime->BuildFromSites(Sites);
		if (UConcealmentService* Service = UConcealmentService::Get(World))
		{
			Service->SetBushes(Runtime->GetBushes());
		}
		return bAccepted;
	}
}

FVector FStealthCrowdHerringSites::EntryPoint(const FBushRuntime& Bush, uint8 Side)
{
	// 35 cm beyond the planter edge: inside the 60 cm entry reach for an NPC that stops within Crowd's arrival radius.
	return Bush.Anchor + ConcealmentGeometry::SideNormal(Side) * (ConcealmentGeometry::FootprintSizeCm * 0.5 + 35.0);
}

bool FStealthCrowdHerringSites::HasBushes() const
{
	const UBushRuntimeSubsystem* Runtime = UBushRuntimeSubsystem::Get(World.Get());
	return Runtime != nullptr && Runtime->GetBushes().Num() > 0;
}

bool FStealthCrowdHerringSites::FindBush(const FVector& From, TConstArrayView<FVector> Path, double RadiusCm, FCrowdBushSite& Out) const
{
	const UBushRuntimeSubsystem* Runtime = UBushRuntimeSubsystem::Get(World.Get());
	if (Runtime == nullptr)
	{
		return false;
	}
	// The nearest bush to the leg (From, then the path corners) within RadiusCm; geometry only, never occupancy.
	const FBushRuntime* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (const FBushRuntime& Bush : Runtime->GetBushes())
	{
		double Distance = FVector::Dist2D(From, Bush.Anchor);
		for (const FVector& Corner : Path)
		{
			Distance = FMath::Min(Distance, FVector::Dist2D(Corner, Bush.Anchor));
		}
		if (Distance <= RadiusCm && Distance < BestDistance && Bush.EntryAxisMask != 0)
		{
			Best = &Bush;
			BestDistance = Distance;
		}
	}
	if (Best == nullptr)
	{
		return false;
	}
	const uint8 Side = ConcealmentGeometry::NearestOpenSide(Best->EntryAxisMask, ConcealmentGeometry::YawFromAnchor(*Best, From));
	Out = FCrowdBushSite();
	Out.BushId = Best->Id;
	Out.Center = Best->Anchor;
	Out.Entry = EntryPoint(*Best, Side);
	for (uint8 Open = 0; Open < 4; ++Open)
	{
		if (Best->IsSideOpen(Open))
		{
			Out.SidePoints.Add(EntryPoint(*Best, Open));
		}
	}
	return true;
}

void FStealthConcealmentFlags::SetOwnerConcealed(FParticipantId Participant, bool bConcealed)
{
	const UStealthCompositionRoot* Target = Root;
	APlayerController* Controller = Target != nullptr ? Target->GetController(Participant) : nullptr;
	if (UConcealmentOwnerViewComponent* View = Controller != nullptr ? Controller->FindComponentByClass<UConcealmentOwnerViewComponent>() : nullptr)
	{
		View->SetConcealed(bConcealed);
	}
}

void FStealthConcealmentFlags::SetTeammateConcealed(FParticipantId Participant, bool bConcealed)
{
	const UStealthCompositionRoot* Target = Root;
	if (URosterStore* Roster = Target != nullptr ? Target->GetRosterStore() : nullptr)
	{
		Roster->SetTeammateConcealed(Participant, bConcealed);
	}
}
