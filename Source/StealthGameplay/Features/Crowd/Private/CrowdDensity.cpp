#include "Server/CrowdDensity.h"

#include "Server/CrowdDraws.h"
#include "Server/Random/StealthSecretStream.h"
#include "Server/StealthTuningSubstructs.h"
#include "View/Random/StealthRangedDraw.h"

namespace CrowdDensityLocal
{
	/** Integer scale of the destination weights for DrawWeighted. */
	constexpr double WeightScale = 1000.0;
	/** The stream tag of the crowd's start placement (NpcRoutes domain). */
	constexpr uint64 StartPlacementTag = StealthRandom::Tag("CNB_ROUT");

	int32 GCrowdScopeDepth = 0;
	int32 GViolations = 0;
	void DefaultHandler()
	{
		ensureAlwaysMsgf(false, TEXT("Crowd code queried the site assignment (identity blindness, CNB-R2, AC-CNB-05)"));
	}
	StealthCrowdBlindness::FHandler GHandler = &DefaultHandler;
}

double FCrowdDensityModel::AreaWeight(ECrowdAreaType Type, const FCrowdTuning_Server& Tuning)
{
	switch (Type)
	{
	case ECrowdAreaType::Causeway: return Tuning.CrowdAreaWeightCausewayMult;
	case ECrowdAreaType::Market: return Tuning.CrowdAreaWeightMarketMult;
	case ECrowdAreaType::Temple: return Tuning.CrowdAreaWeightTempleMult;
	default: return Tuning.CrowdAreaWeightResidentialMult;
	}
}

int32 FCrowdDensityModel::Capacity(const FCrowdRouteNode& Node)
{
	return FMath::Clamp(FMath::FloorToInt32(Node.FrontageCm / 100.0), 1, 4);
}

void FCrowdDensityModel::Initialize(TConstArrayView<FCrowdRouteNode> InNodes, int32 NpcCount, const FCrowdTuning_Server& Tuning)
{
	Nodes = TArray<FCrowdRouteNode>(InNodes.GetData(), InNodes.Num());
	for (int32 Type = 0; Type < static_cast<int32>(ECrowdAreaType::Count); ++Type)
	{
		Area[Type] = 0.0;
		AreaWeights[Type] = AreaWeight(static_cast<ECrowdAreaType>(Type), Tuning);
	}
	for (const FCrowdRouteNode& Node : Nodes)
	{
		Area[static_cast<int32>(Node.Area)] += Node.CatchmentM2;
	}
	double Denominator = 0.0;
	for (int32 Type = 0; Type < static_cast<int32>(ECrowdAreaType::Count); ++Type)
	{
		Denominator += AreaWeights[Type] * Area[Type];
	}
	for (int32 Type = 0; Type < static_cast<int32>(ECrowdAreaType::Count); ++Type)
	{
		Target[Type] = Denominator > 0.0 ? static_cast<double>(NpcCount) * AreaWeights[Type] / Denominator : 0.0;
	}
	// Before any observation every district is at target: the weights are w_t * a_n.
	int32 AtTarget[static_cast<int32>(ECrowdAreaType::Count)];
	for (int32 Type = 0; Type < static_cast<int32>(ECrowdAreaType::Count); ++Type)
	{
		AtTarget[Type] = FMath::RoundToInt32(GetTargetCount(static_cast<ECrowdAreaType>(Type)));
	}
	Refresh(AtTarget);
}

void FCrowdDensityModel::Refresh(const int32 ObservedCount[static_cast<int32>(ECrowdAreaType::Count)])
{
	Weights.SetNumUninitialized(Nodes.Num());
	double Relax[static_cast<int32>(ECrowdAreaType::Count)];
	for (int32 Type = 0; Type < static_cast<int32>(ECrowdAreaType::Count); ++Type)
	{
		const double RhoT = Target[Type];
		const double RhoObs = Area[Type] > 0.0 ? static_cast<double>(ObservedCount[Type]) / Area[Type] : 0.0;
		Relax[Type] = RhoT > 0.0 ? FMath::Clamp(FMath::Sqrt(RhoT / FMath::Max(RhoObs, 0.25 * RhoT)), 0.5, 2.0) : 0.0;
	}
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const int32 Type = static_cast<int32>(Nodes[Index].Area);
		Weights[Index] = AreaWeights[Type] * Nodes[Index].CatchmentM2 * Relax[Type];
	}
}

int32 FCrowdDensityModel::PickDestination(uint32 Ordinal, int32 CurrentNode, TConstArrayView<int32> Occupancy, FCrowdDraws& Draws) const
{
	using namespace CrowdDensityLocal;
	TArray<uint32, TInlineAllocator<64>> Cumulative;
	Cumulative.SetNumUninitialized(Nodes.Num());
	uint32 Total = 0;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const bool bFull = Occupancy.IsValidIndex(Index) && Occupancy[Index] >= Capacity(Nodes[Index]);
		if (Index != CurrentNode && !bFull)
		{
			Total += static_cast<uint32>(FMath::Max(FMath::RoundToInt64(Weights[Index] * WeightScale), int64(0)));
		}
		Cumulative[Index] = Total;
	}
	return Total > 0 ? Draws.DrawWeighted(Ordinal, ECrowdDecision::Destination, Cumulative) : INDEX_NONE;
}

TArray<int32> FCrowdDensityModel::PlaceStarts(int32 Count, uint64 NpcRoutesSubSeed, TConstArrayView<int32> Occupancy) const
{
	using namespace CrowdDensityLocal;
	TArray<int32> Out;
	if (Nodes.Num() == 0 || Count <= 0)
	{
		return Out;
	}
	TArray<int32> Used;
	Used.SetNumZeroed(Nodes.Num());
	for (int32 Index = 0; Index < Nodes.Num() && Index < Occupancy.Num(); ++Index)
	{
		Used[Index] = Occupancy[Index];
	}
	// One secret stream for the whole placement event (a single draw site: ADR-0006 counter-as-Attempt shape not needed).
	FSecretStream Stream(NpcRoutesSubSeed, StartPlacementTag);
	for (int32 Placed = 0; Placed < Count; ++Placed)
	{
		// Each type's start share follows its F1 target: the start weight of a node is w_t * a_n (rho_t * a_n up to a constant).
		uint64 Total = 0;
		TArray<uint64, TInlineAllocator<64>> Cumulative;
		Cumulative.SetNumUninitialized(Nodes.Num());
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Used[Index] < Capacity(Nodes[Index]))
			{
				Total += static_cast<uint64>(FMath::Max(FMath::RoundToInt64(AreaWeights[static_cast<int32>(Nodes[Index].Area)] * Nodes[Index].CatchmentM2 * WeightScale), int64(1)));
			}
			Cumulative[Index] = Total;
		}
		if (Total == 0 || Total > MAX_uint32)
		{
			break; // every node full (capacity counts pawns of any kind), or an out-of-range layout
		}
		const uint32 Pick = StealthRandom::UniformBelow(Stream, static_cast<uint32>(Total));
		int32 Chosen = Nodes.Num() - 1;
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Pick < Cumulative[Index])
			{
				Chosen = Index;
				break;
			}
		}
		++Used[Chosen];
		Out.Add(Chosen);
	}
	return Out;
}

int32 FCrowdDensityModel::NearestNode(const FVector& Location) const
{
	int32 Best = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const double Distance = FVector::DistSquared2D(Nodes[Index].Location, Location);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Index;
		}
	}
	return Best;
}

// ---- Blindness sentinel --------------------------------------------------------------------------------------------

StealthCrowdBlindness::FCrowdScope::FCrowdScope()
{
	++CrowdDensityLocal::GCrowdScopeDepth;
}

StealthCrowdBlindness::FCrowdScope::~FCrowdScope()
{
	--CrowdDensityLocal::GCrowdScopeDepth;
}

bool StealthCrowdBlindness::IsInCrowdScope()
{
	return CrowdDensityLocal::GCrowdScopeDepth > 0;
}

void StealthCrowdBlindness::NoteSiteAssignmentQuery()
{
	if (IsInCrowdScope())
	{
		++CrowdDensityLocal::GViolations;
		if (CrowdDensityLocal::GHandler != nullptr)
		{
			CrowdDensityLocal::GHandler();
		}
	}
}

StealthCrowdBlindness::FHandler StealthCrowdBlindness::SetAssertHandler(FHandler Handler)
{
	const FHandler Previous = CrowdDensityLocal::GHandler;
	CrowdDensityLocal::GHandler = Handler;
	return Previous;
}

int32 StealthCrowdBlindness::GetViolationCount()
{
	return CrowdDensityLocal::GViolations;
}
