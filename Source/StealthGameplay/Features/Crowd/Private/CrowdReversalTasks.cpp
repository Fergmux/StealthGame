// Story cnb-019 (T12 "forgot something" reversals; GDD CNB-R5 C7, R7, R16 T12). A reversal is a stop-pivot-start: the
// stop releases Move(0) through SCL's one braking path (so it lasts at least StopMinSec), the pivot is SCL's turn at
// rest, and the start walks back along the last leg toward its origin at the leg's pace. It is the only behaviour that
// turns the heading by more than ReversalAngleDeg, and it never does so while moving (the follower caps every commanded
// heading change while moving below ReversalAngleDeg; the reversal turns from rest).

#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"
#include "View/StealthTuningPublicSubstructs.h"

void FCrowdAgent::StartReversal(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	using namespace CrowdHerringConstants;
	// Only a walker some way into its leg can turn back along it; otherwise the reversal waits for the next leg.
	const FLocomotionTuning_Public& Loco = World.GetLocomotionTuning();
	if (Activity != ECrowdActivity::Routing || !Follower.IsOnLeg() || Follower.GetGait() != EGait::Walk
		|| Self.Speed2D() < 0.5 * Loco.WalkSpeedCmPerSec * Follower.GetPace() || FVector::Dist2D(Self.Location, Follower.GetOrigin()) < Loco.WalkSpeedCmPerSec * ReversalMinLegSec)
	{
		return;
	}
	if (!TryPerformTell(ECrowdTell::Reversal, Now, World))
	{
		return;
	}
	ReversalTarget = Follower.GetOrigin();
	Follower.Stop();
	Mode = EHerringMode::ReversalStop;
	if (Observer != nullptr)
	{
		Observer->OnReversal(Ordinal, Now);
	}
}
