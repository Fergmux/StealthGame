#include "Server/CrowdFollower.h"

#include "Server/CrowdDraws.h"
#include "Server/StealthTuningSubstructs.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/Core/WalkSlowerScale.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace CrowdFollowerLocal
{
	/** A walker below this fraction of its target speed (after its start) counts as obstructed. */
	constexpr double BlockedSpeedFrac = 0.3;
	/** The start of a leg (accelerating from rest) is never "blocked". */
	constexpr double StartGraceSec = 1.0;
	/** Probability a C4 candidate neighbour is matched (the GDD says "may match"; decision logged 2026-10-26). */
	constexpr double DriftMatchShare = 0.5;
	/** The release distance floor (a leg shorter than this from rest stops at once). */
	constexpr double ReleaseFloorCm = 5.0;
	/** The arrival speed: at or below it the stop to rest is over. */
	constexpr double RestSpeedCm = 1.0;

	FVector2D Right(const FVector2D& D)
	{
		return FVector2D(-D.Y, D.X);
	}

	FVector2D Flat(const FVector& V)
	{
		return FVector2D(V.X, V.Y);
	}
}

double FCrowdFollower::StopDistanceCm(const FLocomotionTuning_Public& Locomotion, double Speed, EGait InGait)
{
	if (Speed <= 0.0)
	{
		return 0.0;
	}
	const double Decel = FMath::Max(static_cast<double>(StealthLocomotion::StopDeceleration(Locomotion, static_cast<float>(Speed), InGait)), 1.0);
	return Speed * Speed / (2.0 * Decel);
}

double FCrowdFollower::FullPaceLegProbability(double FullTimeShare)
{
	// Slower legs of the same length take 1/pace as long: time(1.0) : time(slow) = q : (1 - q) * K.
	const double K = SlowPaceShareOfNonFull / StealthPaceSet::Slow + (1.0 - SlowPaceShareOfNonFull) / StealthPaceSet::Middle;
	const double S = FMath::Clamp(FullTimeShare, 0.0, 1.0);
	return S * K / FMath::Max(1.0 - S + S * K, UE_DOUBLE_SMALL_NUMBER);
}

void FCrowdFollower::Initialize(uint32 InOrdinal, double SteerPhaseSec, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws)
{
	Ordinal = InOrdinal;
	SteerPhase = SteerPhaseSec;
	LaneOffsetCm = Draws.DrawUniform(Ordinal, ECrowdDecision::LaneOffset, -Tuning.CrowdLaneOffsetCm, Tuning.CrowdLaneOffsetCm);
}

void FCrowdFollower::ChoosePace(const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws, ECrowdPaceCause Cause)
{
	const double FullProbability = FullPaceLegProbability(Tuning.CrowdFullPacePct / 100.0);
	const double U = Draws.DrawUnit(Ordinal, ECrowdDecision::Pace);
	if (U < FullProbability)
	{
		LegPace = StealthPaceSet::Full;
	}
	else
	{
		LegPace = U < FullProbability + (1.0 - FullProbability) * (1.0 - SlowPaceShareOfNonFull) ? StealthPaceSet::Middle : StealthPaceSet::Slow;
	}
	Pace = LegPace;
	LastPaceCause = Cause;
}

void FCrowdFollower::StartLeg(uint32 InLegId, const FVector& From, TConstArrayView<FVector> InCorners, double Now, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws, bool bKeepPace,
	EGait InGait, double InReleaseAt)
{
	LegId = InLegId;
	Gait = InGait;
	ReleaseAt = InReleaseAt;
	bOnLeg = true;
	bStopping = false;
	bWaitingForPath = false;
	LegStartTime = Now;
	BlockedSec = 0.0;
	Repaths = 0;
	bRepathPending = false;
	DriftUntil = -1.0;
	bDriftUsedThisLeg = false;
	ProgressMark = From;
	ProgressMarkTime = Now;
	NextSteerTime = Now;
	Avoidance.Reset();
	if (!bKeepPace || LegPace <= 0.0f)
	{
		ChoosePace(Tuning, Draws, ECrowdPaceCause::LegStart);
	}
	else
	{
		Pace = LegPace;
	}
	ReplacePath(From, InCorners);
}

void FCrowdFollower::ReplacePath(const FVector& From, TConstArrayView<FVector> InCorners)
{
	using namespace CrowdFollowerLocal;
	Origin = From;
	Corners.Reset();
	CurrentSegment = 0;
	bWaitingForPath = false;
	bRepathPending = false;
	FVector Previous = From;
	for (int32 Index = 0; Index < InCorners.Num(); ++Index)
	{
		FVector Corner = InCorners[Index];
		if (Index + 1 < InCorners.Num())
		{
			// Lane offset: to the right of the bisector of the incoming and outgoing segments.
			const FVector2D In = (Flat(Corner) - Flat(Previous)).GetSafeNormal();
			const FVector2D Out = (Flat(InCorners[Index + 1]) - Flat(Corner)).GetSafeNormal();
			FVector2D Normal = Right(In) + Right(Out);
			Normal = Normal.IsNearlyZero() ? Right(In) : Normal.GetSafeNormal();
			Corner += FVector(Normal * LaneOffsetCm, 0.0);
		}
		Corners.Add(Corner);
		Previous = InCorners[Index];
	}
}

void FCrowdFollower::BeginWaitForPath(double Now)
{
	bWaitingForPath = true;
	WaitStarted = Now;
}

void FCrowdFollower::Stop()
{
	if (bOnLeg)
	{
		bOnLeg = false;
		bStopping = true;
	}
	bWaitingForPath = false;
	DriftUntil = -1.0;
	Avoidance.Reset();
}

void FCrowdFollower::ProjectOntoPath(const FVector& Location, int32& OutSegment, FVector& OutPoint) const
{
	double Best = TNumericLimits<double>::Max();
	OutSegment = CurrentSegment;
	OutPoint = Corners.Num() > 0 ? Corners[0] : Origin;
	for (int32 Segment = CurrentSegment; Segment < Corners.Num(); ++Segment)
	{
		const FVector A = Segment == 0 ? Origin : Corners[Segment - 1];
		const FVector B = Corners[Segment];
		const FVector2D Ab(B.X - A.X, B.Y - A.Y);
		const double LenSq = Ab.SizeSquared();
		const double T = LenSq > UE_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(FVector2D(Location.X - A.X, Location.Y - A.Y), Ab) / LenSq, 0.0, 1.0) : 0.0;
		const FVector P = A + (B - A) * T;
		const double D = FVector::DistSquared2D(P, Location);
		if (D < Best - 1e-6)
		{
			Best = D;
			OutSegment = Segment;
			OutPoint = P;
		}
	}
}

FVector FCrowdFollower::AdvanceAlongPath(int32 Segment, const FVector& Point, double Distance, double& OutRemaining, FVector& OutTangent) const
{
	FVector Here = Point;
	double Left = Distance;
	FVector Carrot = Corners.Num() > 0 ? Corners.Last() : Origin;
	bool bPlaced = false;
	OutRemaining = 0.0;
	OutTangent = FVector::ForwardVector;
	for (int32 S = Segment; S < Corners.Num(); ++S)
	{
		const FVector End = Corners[S];
		const double Length = FVector::Dist2D(Here, End);
		const FVector Dir = (End - Here).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			OutTangent = Dir;
		}
		if (!bPlaced && Length >= Left)
		{
			Carrot = Here + Dir * Left;
			bPlaced = true;
		}
		else if (!bPlaced)
		{
			Left -= Length;
		}
		OutRemaining += Length;
		Here = End;
	}
	return Carrot;
}

FCrowdFollowerOutput FCrowdFollower::Tick(double Now, float Dt, const FCrowdPerceivedPawn& Self, TConstArrayView<const FCrowdPerceivedPawn*> Neighbours,
	const FCrowdTuning_Server& Tuning, const FLocomotionTuning_Public& Locomotion, FCrowdDraws& Draws)
{
	using namespace CrowdFollowerLocal;
	FCrowdFollowerOutput Out;
	const double Speed = Self.Speed2D();
	if (!bOnLeg)
	{
		if (bStopping && Speed <= RestSpeedCm)
		{
			bStopping = false;
			Out.bArrived = true;
		}
		return Out;
	}
	if (bWaitingForPath)
	{
		// Hold the last Move while a query is in flight (at least two frames), at most CrowdPathInvalidHoldSec.
		if (Now - WaitStarted <= Tuning.CrowdPathInvalidHoldSec && !HeldDirection.IsNearlyZero())
		{
			Out.WorldDirection = HeldDirection;
			Out.Magnitude = Gait == EGait::Run ? 1.0f : Pace;
			Out.Gait = Gait;
		}
		return Out;
	}
	if (Corners.Num() == 0)
	{
		return Out;
	}

	int32 Segment = 0;
	FVector OnPath;
	ProjectOntoPath(Self.Location, Segment, OnPath);
	CurrentSegment = Segment;
	double Remaining = 0.0;
	FVector Tangent;
	const double CarrotDistance = FMath::Max(static_cast<double>(Tuning.CrowdCarrotMinCm), Speed * Tuning.CrowdCarrotLeadSec);
	FVector Carrot = AdvanceAlongPath(Segment, OnPath, CarrotDistance, Remaining, Tangent);

	// Arrival: Move(0) at the F2 stop distance of the current speed, so the stop to rest ends at the leg's end. A run leg
	// with a release time also ends there (the bout's end is ordinary braking; the leg ends where the braking ends).
	const double StopDistance = StopDistanceCm(Locomotion, Speed, Gait);
	const bool bReleaseByTime = ReleaseAt > 0.0 && Now >= ReleaseAt;
	if (bReleaseByTime || Remaining <= FMath::Max(StopDistance, ReleaseFloorCm))
	{
		bOnLeg = false;
		bStopping = true;
		Out.bReleased = true;
		DriftUntil = -1.0;
		Avoidance.Reset();
		if (Speed <= RestSpeedCm)
		{
			bStopping = false;
			Out.bArrived = true;
		}
		return Out;
	}

	// The blocked ladder and the progress monitor (CNB-R3.3).
	const double TargetSpeed = Gait == EGait::Run ? static_cast<double>(Locomotion.RunSpeedCmPerSec) : Locomotion.WalkSpeedCmPerSec * Pace;
	const bool bPastStart = Now - LegStartTime > StartGraceSec;
	if (bPastStart && Speed < BlockedSpeedFrac * TargetSpeed)
	{
		BlockedSec += Dt;
	}
	else
	{
		BlockedSec = 0.0;
	}
	bool bBlockedRepath = BlockedSec > Tuning.CrowdBlockedRepathSec;
	if (Now - ProgressMarkTime >= Tuning.CrowdProgressWindowSec)
	{
		bBlockedRepath |= FVector::Dist2D(Self.Location, ProgressMark) < Tuning.CrowdProgressMinCm;
		ProgressMark = Self.Location;
		ProgressMarkTime = Now;
	}
	if (bBlockedRepath && !bRepathPending)
	{
		BlockedSec = 0.0;
		if (Repaths >= 2)
		{
			// Two failed repaths: abandon the leg; the brain draws a new destination.
			Stop();
			Out.bAbandoned = true;
			return Out;
		}
		++Repaths;
		bRepathPending = true;
		Out.bWantsRepath = true;
	}
	const bool bBlockedWide = BlockedSec > Tuning.CrowdBlockedWidenSec;

	// C4 group drift: a walker within range heading the same way may lend its pace for at most CrowdGroupDriftSec.
	if (DriftUntil >= 0.0 && Now >= DriftUntil)
	{
		DriftUntil = -1.0;
		Pace = LegPace;
		LastPaceCause = ECrowdPaceCause::DriftEnd;
	}
	if (Gait == EGait::Walk && !bDriftUsedThisLeg && DriftUntil < 0.0 && Speed > 0.5 * TargetSpeed && Tuning.CrowdGroupDriftSec > 0.0f)
	{
		const FVector Heading = Self.VelocityCmPerSec.GetSafeNormal2D();
		for (const FCrowdPerceivedPawn* Other : Neighbours)
		{
			if (Other == nullptr || Other->Key == Self.Key || FVector::Dist2D(Other->Location, Self.Location) > Tuning.CrowdGroupDriftRangeCm)
			{
				continue;
			}
			const double OtherSpeed = Other->Speed2D();
			const FVector OtherHeading = Other->VelocityCmPerSec.GetSafeNormal2D();
			if (OtherSpeed < 0.5 * Locomotion.WalkSpeedCmPerSec * StealthPaceSet::Slow || FVector::DotProduct(Heading, OtherHeading) < FMath::Cos(FMath::DegreesToRadians(Tuning.CrowdGroupDriftHeadingDeg)))
			{
				continue;
			}
			const float OtherPace = StealthLocomotion::QuantisePace(static_cast<float>(OtherSpeed / FMath::Max(static_cast<double>(Locomotion.WalkSpeedCmPerSec), 1.0)), 0.0f);
			bDriftUsedThisLeg = true;
			if (OtherPace > 0.0f && OtherPace != Pace && OtherPace <= StealthPaceSet::Full && Draws.DrawChance(Ordinal, ECrowdDecision::GroupDrift, DriftMatchShare))
			{
				Pace = OtherPace;
				DriftUntil = Now + Tuning.CrowdGroupDriftSec;
				LastPaceCause = ECrowdPaceCause::GroupDrift;
			}
			break;
		}
	}

	// Steering at CrowdSteerPerSec with the pawn's phase; the direction is held between recomputes.
	if (Now >= NextSteerTime)
	{
		const double Period = 1.0 / FMath::Max(static_cast<double>(Tuning.CrowdSteerPerSec), 1.0);
		const double K = FMath::FloorToDouble((Now - SteerPhase) / Period) + 1.0;
		NextSteerTime = SteerPhase + K * Period;

		const FCrowdAvoidanceOutput Avoid = Avoidance.Step(Ordinal, Self, Neighbours, Now, true, bBlockedWide, Tuning, Draws);
		const FVector2D Side = Right(Flat(Tangent).GetSafeNormal());
		Carrot += FVector(Side * Avoid.LateralOffsetCm, 0.0);
		FVector2D Direction = (Flat(Carrot) - Flat(Self.Location)).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			Direction = Flat(Tangent).GetSafeNormal();
		}
		// Never command a heading change at or above the reversal angle while moving (C7 is the only reversal).
		if (Speed > Locomotion.PivotSpeedCmPerSec)
		{
			const FVector2D Moving = Flat(Self.VelocityCmPerSec).GetSafeNormal();
			const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(Moving, Direction), -1.0, 1.0)));
			const double MaxAngle = Locomotion.ReversalAngleDeg - ReversalMarginDeg;
			if (Angle > MaxAngle)
			{
				const double Sign = FVector2D::CrossProduct(Moving, Direction) >= 0.0 ? 1.0 : -1.0;
				const double Rad = FMath::DegreesToRadians(MaxAngle) * Sign;
				Direction = FVector2D(Moving.X * FMath::Cos(Rad) - Moving.Y * FMath::Sin(Rad), Moving.X * FMath::Sin(Rad) + Moving.Y * FMath::Cos(Rad));
			}
		}
		HeldDirection = Direction;
	}
	Out.WorldDirection = HeldDirection;
	Out.Magnitude = Gait == EGait::Run ? 1.0f : Pace;
	Out.Gait = Gait;
	return Out;
}
