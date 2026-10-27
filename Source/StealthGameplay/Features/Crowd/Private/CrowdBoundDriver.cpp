// Story cnb-028 (participant-bound pawn driving; GDD CNB-R10 items 1-3 and 5, AC-CNB-20; ADR-0016 section 7). On
// Round's takeover (DriveBoundPawn, then the factory's hand-over) the restricted driver first holds
// Move(current heading, Walk) at the pace nearest the current speed for HandoverHoldSec (SCL R4's stop-and-blend: the
// pawn never brakes faster than the SCL run braking, CrowdStopDecel = RunSpeedCmPerSec / RunStopSec), or Move(0) when
// the pawn is slower than the SCL pivot speed or the held heading would leave the navmesh within the hold distance;
// then it runs the restricted catalogue (C1, C2, C5, C6, G1-G3, G5, G7) through FCrowdMotionSink, which carries only
// Move and Look. A concealed bound pawn stays concealed: the restricted output has no Hide.

#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/Core/WalkSlowerScale.h"
#include "View/StealthTuningPublicSubstructs.h"

void FCrowdAgent::StartHandoverHold(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	const FLocomotionTuning_Public& Loco = World.GetLocomotionTuning();
	const double HoldSec = World.GetActionTimings().HandoverHoldSec;
	const double Speed = Self.Speed2D();
	const FVector Heading = Self.VelocityCmPerSec.GetSafeNormal2D().IsNearlyZero() ? FRotator(0.0, Self.FacingYawDeg, 0.0).Vector() : Self.VelocityCmPerSec.GetSafeNormal2D();
	HoldDirection = FVector2D(Heading.X, Heading.Y);
	// The pace nearest the current speed, at walk (a running pawn slows to its walk by SCL's own braking).
	HoldMagnitude = StealthLocomotion::QuantisePace(static_cast<float>(FMath::Min(Speed / FMath::Max(static_cast<double>(Loco.WalkSpeedCmPerSec), 1.0), 1.0)), 0.0f);
	HoldMagnitude = FMath::Clamp(HoldMagnitude, StealthPaceSet::Slow, StealthPaceSet::Full);
	const double HoldDistance = FMath::Max(Speed, static_cast<double>(Loco.WalkSpeedCmPerSec)) * HoldSec;
	if (Speed < Loco.PivotSpeedCmPerSec || !World.IsOnNavigation(Self.Location + Heading * HoldDistance))
	{
		HoldMagnitude = 0.0f;
	}
	HoldUntil = Now + HoldSec;
	SetActivity(ECrowdActivity::Routing, World);
}

bool FCrowdAgent::TickHandoverHold(double Now, float Dt, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	if (HoldUntil < 0.0)
	{
		return false;
	}
	if (Now < HoldUntil)
	{
		FCrowdFollowerOutput Motion;
		Motion.WorldDirection = HoldMagnitude > 0.0f ? HoldDirection : FVector2D::ZeroVector;
		Motion.Magnitude = HoldMagnitude;
		Motion.Gait = EGait::Walk;
		EmitInput(Now, Dt, Motion, FCrowdGazeIntent(), World, Output);
		return true;
	}
	// The hold is over: the restricted catalogue starts with an ordinary leg from where the pawn is.
	HoldUntil = -1.0;
	BeginLeg(Now, Self, World, -1.0);
	return false;
}
