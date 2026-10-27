// Story cnb-026 (the agent half of an ambient exit; GDD CNB-R9.1, LSC R5): an NPC given an exit slot takes "leave by the
// alley" as its next leg, walks into the alley and is absorbed once the Level Socket Contract's clear check passes
// (nobody sees it go). If it is watched for CrowdExitGiveUpSec inside the alley it turns and walks back out, an ordinary
// alley dawdle; the cycle is then abandoned (no vacancy, no entry).

#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"

bool FCrowdAgent::CanTakeExitSlot() const
{
	return CanStartHerring() && HoldUntil < 0.0;
}

void FCrowdAgent::StartAmbientExit(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, const FCrowdExitSite& Site)
{
	ExitSite = Site;
	ExitWatchedSec = 0.0;
	bAwaitingExitSlot = false;
	LeaveRestForHerring(Now);
	BeginLegTo(Now, Self, World, Site.Inside, false, EGait::Walk, -1.0, TConstArrayView<FVector>());
	Mode = EHerringMode::ExitWalk;
	SetActivity(ECrowdActivity::Exiting, World);
}

void FCrowdAgent::TickAmbientExit(double Now, float Dt, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	const bool bInAlley = FVector::Dist2D(Self.Location, ExitSite.Inside) <= FVector::Dist2D(ExitSite.Mouth, ExitSite.Inside) + 50.0;
	if (bInAlley && World.IsExitWatched(Ordinal, ExitSite, Now))
	{
		ExitWatchedSec += Dt;
	}
	if (ExitWatchedSec >= Tuning.CrowdExitGiveUpSec)
	{
		// Watched too long: walk back out (the slot's cycle is abandoned).
		World.OnAmbientExitAbandoned(Ordinal, ExitSite);
		Mode = EHerringMode::None;
		BeginLeg(Now, Self, World, -1.0);
		return;
	}
	if (Mode == EHerringMode::ExitWait && World.TryAbsorb(Ordinal, ExitSite, Now))
	{
		// Absorbed: the world removes the pawn (its card becomes vacant); nothing more is issued.
		Mode = EHerringMode::None;
		bAbsorbed = true;
		Follower.Stop();
	}
}
