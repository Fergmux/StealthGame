// Story oc-006 (production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md): the stand-point
// claim table (ADR-0020 sections 5.1 and 5.3). See ObjectivesClaims.h.

#include "ObjectivesClaims.h"

#include "Server/StealthFaultSink.h"

namespace ObjectivesClaimsLocal
{
	const FName FaultSource(TEXT("Objectives"));
}

int32 FObjectivesClaimTable::Claim(FCourtFixtureId Court, const APawn* Pawn, const FVector& PawnLocation, TConstArrayView<FVector> Faces, int32 SideCount, uint32 ControlEpoch)
{
	if (Pawn == nullptr || !Court.IsSet())
	{
		return INDEX_NONE;
	}
	const int32 Sides = FMath::Clamp(SideCount, 1, MaxFaces);
	FFixture& Fixture = Fixtures.FindOrAdd(Court);
	const int32 Held = FindFace(Court, Pawn);
	if (Held != INDEX_NONE)
	{
		return Held;
	}
	int32 Best = INDEX_NONE;
	double BestDistSq = TNumericLimits<double>::Max();
	for (int32 Face = 0; Face < Sides; ++Face)
	{
		const FClaim& Claim = Fixture.Faces[Face];
		if (Claim.bHeld && Claim.Pawn.IsValid())
		{
			continue;
		}
		// Without geometry for this face every face is equally near: the lowest free index wins.
		const double DistSq = Faces.IsValidIndex(Face) ? FVector::DistSquared2D(Faces[Face], PawnLocation) : 0.0;
		if (DistSq < BestDistSq) // strict: an equal distance keeps the lower face index
		{
			BestDistSq = DistSq;
			Best = Face;
		}
	}
	if (Best == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	FClaim& Claim = Fixture.Faces[Best];
	Claim.Pawn = Pawn;
	Claim.ControlEpoch = ControlEpoch;
	Claim.bHeld = true;
	return Best;
}

bool FObjectivesClaimTable::Release(FCourtFixtureId Court, const APawn* Pawn, uint32* OutControlEpoch)
{
	FFixture* Fixture = Fixtures.Find(Court);
	if (Fixture == nullptr || Pawn == nullptr)
	{
		return false;
	}
	for (FClaim& Claim : Fixture->Faces)
	{
		if (Claim.bHeld && Claim.Pawn.Get() == Pawn)
		{
			if (OutControlEpoch != nullptr)
			{
				*OutControlEpoch = Claim.ControlEpoch;
			}
			Claim = FClaim();
			return true;
		}
	}
	return false;
}

int32 FObjectivesClaimTable::GetClaimCount(FCourtFixtureId Court) const
{
	const FFixture* Fixture = Fixtures.Find(Court);
	int32 Count = 0;
	if (Fixture != nullptr)
	{
		for (const FClaim& Claim : Fixture->Faces)
		{
			// A claim whose pawn was destroyed without an end call is stale: it no longer occupies the face.
			Count += Claim.bHeld && Claim.Pawn.IsValid() ? 1 : 0;
		}
	}
	return Count;
}

const FObjectivesClaimTable::FClaim* FObjectivesClaimTable::FindClaim(FCourtFixtureId Court, const APawn* Pawn) const
{
	const int32 Face = FindFace(Court, Pawn);
	return Face != INDEX_NONE ? &Fixtures.FindChecked(Court).Faces[Face] : nullptr;
}

int32 FObjectivesClaimTable::FindFace(FCourtFixtureId Court, const APawn* Pawn) const
{
	const FFixture* Fixture = Fixtures.Find(Court);
	if (Fixture == nullptr || Pawn == nullptr)
	{
		return INDEX_NONE;
	}
	for (int32 Face = 0; Face < MaxFaces; ++Face)
	{
		if (Fixture->Faces[Face].bHeld && Fixture->Faces[Face].Pawn.Get() == Pawn)
		{
			return Face;
		}
	}
	return INDEX_NONE;
}

void FObjectivesClaimTable::GetClaimedCourts(TArray<FCourtFixtureId>& OutCourts) const
{
	OutCourts.Reset();
	for (const TPair<FCourtFixtureId, FFixture>& Pair : Fixtures)
	{
		if (GetClaimCount(Pair.Key) > 0)
		{
			OutCourts.Add(Pair.Key);
		}
	}
}

#if !UE_BUILD_SHIPPING
int32 FObjectivesClaimTable::CheckConsistency(const TMap<FCourtFixtureId, int32>& InteractingSlots, IStealthFaultSink* Faults)
{
	TSet<FCourtFixtureId> Courts;
	for (const TPair<FCourtFixtureId, int32>& Pair : InteractingSlots)
	{
		Courts.Add(Pair.Key);
	}
	TArray<FCourtFixtureId> Claimed;
	GetClaimedCourts(Claimed);
	Courts.Append(Claimed);
	int32 Mismatches = 0;
	for (const FCourtFixtureId& Court : Courts)
	{
		const int32* Slots = InteractingSlots.Find(Court);
		if (GetClaimCount(Court) == (Slots != nullptr ? *Slots : 0))
		{
			ReportedMismatches.Remove(Court);
			continue;
		}
		++Mismatches;
		bool bAlready = false;
		ReportedMismatches.Add(Court, &bAlready);
		if (!bAlready && Faults != nullptr)
		{
			Faults->Report(EStealthFault::InvariantViolation, ObjectivesClaimsLocal::FaultSource, static_cast<int64>(Court.Value));
		}
	}
	return Mismatches;
}
#endif
