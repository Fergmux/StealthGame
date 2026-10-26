#include "Server/CrowdSpatialHash.h"

FIntPoint FCrowdSpatialHash::CellOf(const FVector& Location) const
{
	return FIntPoint(FMath::FloorToInt32(Location.X / CellCm), FMath::FloorToInt32(Location.Y / CellCm));
}

void FCrowdSpatialHash::Build(TConstArrayView<FCrowdPerceivedPawn> InPawns)
{
	++BuildCount;
	Pawns.Reset();
	Pawns.Append(InPawns.GetData(), InPawns.Num());
	Cells.Reset();
	ByKey.Reset();
	for (int32 Index = 0; Index < Pawns.Num(); ++Index)
	{
		Cells.Add(CellOf(Pawns[Index].Location), Index);
		ByKey.Add(Pawns[Index].Key, Index);
	}
}

void FCrowdSpatialHash::Query(const FVector& Center, double Radius, ECrowdQuery Mode, uint64 ExcludeKey, TArray<const FCrowdPerceivedPawn*>& Out) const
{
	Out.Reset();
	const FIntPoint Min = CellOf(Center - FVector(Radius, Radius, 0.0));
	const FIntPoint Max = CellOf(Center + FVector(Radius, Radius, 0.0));
	const double RadiusSq = Radius * Radius;
	TArray<int32, TInlineAllocator<16>> Found;
	for (int32 X = Min.X; X <= Max.X; ++X)
	{
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			for (auto It = Cells.CreateConstKeyIterator(FIntPoint(X, Y)); It; ++It)
			{
				const FCrowdPerceivedPawn& Pawn = Pawns[It.Value()];
				if (Pawn.Key == ExcludeKey)
				{
					continue;
				}
				if (Mode == ECrowdQuery::Perception ? !Pawn.bPerceivable : !Pawn.bObstacle)
				{
					continue;
				}
				if (FVector::DistSquared2D(Pawn.Location, Center) <= RadiusSq)
				{
					Found.Add(It.Value());
				}
			}
		}
	}
	Found.Sort([this, &Center](int32 A, int32 B)
	{
		const double DA = FVector::DistSquared2D(Pawns[A].Location, Center);
		const double DB = FVector::DistSquared2D(Pawns[B].Location, Center);
		return DA != DB ? DA < DB : Pawns[A].Key < Pawns[B].Key;
	});
	for (const int32 Index : Found)
	{
		Out.Add(&Pawns[Index]);
	}
}

const FCrowdPerceivedPawn* FCrowdSpatialHash::Find(uint64 Key) const
{
	const int32* Index = ByKey.Find(Key);
	return Index != nullptr ? &Pawns[*Index] : nullptr;
}

int32 FCrowdRayBudget::Process(int32 Limit, FTrace Trace, FDeliver Deliver)
{
	int32 Done = 0;
	while (Done < Limit && Queue.Num() > 0)
	{
		// One pass: the oldest ray of each requester, in queue order.
		TSet<uint32, DefaultKeyFuncs<uint32>, TInlineSetAllocator<16>> Served;
		bool bProgress = false;
		for (int32 Index = 0; Index < Queue.Num() && Done < Limit;)
		{
			if (Served.Contains(Queue[Index].Requester))
			{
				++Index;
				continue;
			}
			const FCrowdRayRequest Ray = Queue[Index];
			Queue.RemoveAt(Index, EAllowShrinking::No);
			Served.Add(Ray.Requester);
			Deliver(Ray, Trace(Ray.From, Ray.To));
			++Done;
			bProgress = true;
		}
		if (!bProgress)
		{
			break;
		}
	}
	return Done;
}
