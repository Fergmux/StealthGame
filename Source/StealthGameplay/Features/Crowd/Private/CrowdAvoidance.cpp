#include "Server/CrowdAvoidance.h"

#include "Server/CrowdDraws.h"
#include "Server/StealthTuningSubstructs.h"

namespace CrowdAvoidanceLocal
{
	/** Relative lateral position below this (cm) is a tie: keep right. */
	constexpr double KeepRightToleranceCm = 10.0;
	/** An approach is forgotten when it was not predicted for this long. */
	constexpr double ForgetSec = 0.5;
	/** Below this speed (cm/s) a walker is not moving for avoidance purposes. */
	constexpr double MovingSpeedCm = 20.0;
}

double FCrowdAvoidance::PredictClosestApproach(const FVector& SelfPos, const FVector& SelfVel, const FVector& OtherPos, const FVector& OtherVel, double& OutTime)
{
	const FVector2D P(OtherPos.X - SelfPos.X, OtherPos.Y - SelfPos.Y);
	const FVector2D V(OtherVel.X - SelfVel.X, OtherVel.Y - SelfVel.Y);
	const double VV = V.SizeSquared();
	double T = VV > UE_DOUBLE_SMALL_NUMBER ? -FVector2D::DotProduct(P, V) / VV : 0.0;
	T = FMath::Max(T, 0.0);
	OutTime = T;
	return (P + V * T).Size();
}

void FCrowdAvoidance::Reset()
{
	Approaches.Reset();
}

FCrowdAvoidanceOutput FCrowdAvoidance::Step(uint32 Ordinal, const FCrowdPerceivedPawn& Self, TConstArrayView<const FCrowdPerceivedPawn*> Neighbours,
	double Now, bool bWantsToMove, bool bBlockedWide, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws)
{
	using namespace CrowdAvoidanceLocal;
	FCrowdAvoidanceOutput Out;
	if (!bWantsToMove || Self.Speed2D() < MovingSpeedCm)
	{
		return Out;
	}
	const FVector Forward = Self.VelocityCmPerSec.GetSafeNormal2D();
	double BestOffset = 0.0;
	double BestAbs = 0.0;
	for (const FCrowdPerceivedPawn* Other : Neighbours)
	{
		if (Other == nullptr || Other->Key == Self.Key)
		{
			continue;
		}
		const FVector Rel = Other->Location - Self.Location;
		if (FVector::DotProduct(Rel, Forward) < 0.0)
		{
			continue; // behind: not an approach
		}
		// UE axes: X forward, Y right. Cross < 0: the other is to the walker's left; > 0: to its right.
		const double Cross = Forward.X * Rel.Y - Forward.Y * Rel.X;
		double Side;
		if (FMath::Abs(Cross) <= HeadOnToleranceCm)
		{
			if (HeadOnSide == 0.0)
			{
				HeadOnSide = Draws.DrawChance(Ordinal, ECrowdDecision::HeadOnCoin, 0.5) ? 1.0 : -1.0;
			}
			Side = HeadOnSide;
		}
		else if (FMath::Abs(Cross) <= KeepRightToleranceCm)
		{
			Side = 1.0;
		}
		else
		{
			// Step away from the other's side: right (+) when it is on the left.
			Side = Cross < 0.0 ? 1.0 : -1.0;
		}

		if (Other->bStaticObstacle)
		{
			// SCL R16: a planted or tending pawn is a static obstacle of CrowdStaticObstacleCm plus margin; always avoided.
			const double Lateral = FMath::Abs(Cross);
			const double Clearance = Tuning.CrowdStaticObstacleCm + Tuning.CrowdAvoidOffsetCm;
			if (Rel.Size2D() < QueryRangeCm * 0.5 && Lateral < Clearance)
			{
				const double Offset = Side * (Clearance - Lateral + Tuning.CrowdStaticObstacleCm * 0.5);
				if (FMath::Abs(Offset) > BestAbs)
				{
					BestAbs = FMath::Abs(Offset);
					BestOffset = Offset;
				}
			}
			continue;
		}

		double When = 0.0;
		const double Closest = PredictClosestApproach(Self.Location, Self.VelocityCmPerSec, Other->Location, Other->VelocityCmPerSec, When);
		const bool bPredicted = Other->Speed2D() >= MovingSpeedCm && When > 0.0 && When <= HorizonSec && Closest < Tuning.CrowdAvoidTriggerCm;
		FApproach* Approach = Approaches.FindByPredicate([Other](const FApproach& A) { return A.Key == Other->Key; });
		if (bPredicted && Approach == nullptr)
		{
			// First detection: the one yield draw for this approach, kind-blind (the partner's facts are not an input).
			Approach = &Approaches.AddDefaulted_GetRef();
			Approach->Key = Other->Key;
			Approach->bYield = Draws.DrawChance(Ordinal, ECrowdDecision::Yield, Tuning.CrowdYieldPct / 100.0);
			Approach->ActiveFrom = Now + Draws.DrawUniform(Ordinal, ECrowdDecision::YieldJitter, Tuning.CrowdYieldJitterMinSec, Tuning.CrowdYieldJitterMaxSec);
			Approach->Side = Side;
			++ApproachCount;
			YieldCount += Approach->bYield ? 1 : 0;
			Out.bNewApproach = true;
			Out.bNewApproachYields = Approach->bYield;
		}
		if (Approach != nullptr && bPredicted)
		{
			Approach->LastSeen = Now;
		}
		if (Approach != nullptr && Approach->bYield && Now >= Approach->ActiveFrom)
		{
			const double Offset = Approach->Side * (bBlockedWide ? Tuning.CrowdAvoidWideOffsetCm : Tuning.CrowdAvoidOffsetCm);
			if (FMath::Abs(Offset) > BestAbs)
			{
				BestAbs = FMath::Abs(Offset);
				BestOffset = Offset;
				Out.bYielding = true;
			}
		}
	}
	// An approach ends when it has not been predicted for a moment (passed, or the partner turned away).
	Approaches.RemoveAll([Now](const FApproach& A) { return Now - FMath::Max(A.LastSeen, A.ActiveFrom) > ForgetSec; });
	if (bBlockedWide && BestOffset == 0.0)
	{
		BestOffset = Tuning.CrowdAvoidWideOffsetCm;
	}
	Out.LateralOffsetCm = BestOffset;
	return Out;
}
