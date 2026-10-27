// Story cnb-019 (T2 traversal red herrings; GDD CNB-R7 traversal bullet, R16 T2, F2). A due vault or climb takes a leg
// that crosses public coping (an LSC TraversalPoint with a nav link): the leg walks to the coping's near side, over it
// and on to a landing beyond. Near the coping the agent asks the SCL dry run (CanBegin with the CrowdDryRunReachCm /
// FacingDeg margins) and sends Traverse through the sink with the point as the hint; the crossing itself is the shared
// SCL traversal state. A refused dry run abandons the behaviour and the point is not retried for CrowdRetryBlockSec.
// Climbs only happen where the coping is at least 1.6 m (the site query filters by class).

#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace CrowdTraverseLocal
{
	/** The landing walk continues this far beyond the coping's far side (cm). */
	constexpr double LandingRunOutCm = 300.0;
	/** The approach lines up this far before the coping's near side (cm). */
	constexpr double RunUpCm = 400.0;
	/** A traversal that has not started this long after Traverse was sent is over (the SCL resolver refused it). */
	constexpr double TraversalStartTimeoutSec = 0.5;
}

bool FCrowdAgent::StartTraversal(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, bool bClimb)
{
	using namespace CrowdHerringConstants;
	const ECrowdTell Tell = bClimb ? ECrowdTell::Climb : ECrowdTell::Vault;
	FCrowdTraversalSite Site;
	if (!World.FindTraversalNear(Ordinal, Self.Location, TraversalSearchCm, bClimb, Site))
	{
		return false; // no public coping of that class nearby yet: the tell waits (and is dropped after its wait)
	}
	if (const double* Until = BlockedTraversals.Find(Site.PointId); Until != nullptr && Now < *Until)
	{
		return false;
	}
	if (MustDeferTell(Tell, Now, World))
	{
		return false; // waits until no different tell was shown within the gap
	}
	InProgressAcceptedAt = PendingSince[static_cast<int32>(Tell)];
	PendingSince[static_cast<int32>(Tell)] = -1.0;
	TraversalSite = Site;
	bTraversalClimb = bClimb;
	bTraversalSent = false;
	const FVector Over = (Site.Far - Site.Near).GetSafeNormal2D();
	// The leg: to a run-up point before the coping, straight over it, and on to the landing run-out, so the pawn meets
	// the coping square (the dry run's facing margin) as a human lining up a vault would.
	TraversalTail.Reset();
	TraversalTail.Add(Site.Near);
	TraversalTail.Add(Site.Far);
	TraversalTail.Add(Site.Far + Over * CrowdTraverseLocal::LandingRunOutCm);
	LeaveRestForHerring(Now);
	BeginLegTo(Now, Self, World, Site.Near - Over * CrowdTraverseLocal::RunUpCm, true, EGait::Walk, -1.0, TraversalTail);
	Mode = EHerringMode::TraverseApproach;
	return true;
}

void FCrowdAgent::TickTraversal(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	using namespace CrowdHerringConstants;
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	const ECrowdTell Tell = bTraversalClimb ? ECrowdTell::Climb : ECrowdTell::Vault;
	if (Mode == EHerringMode::TraverseApproach)
	{
		if (FVector::Dist2D(Self.Location, TraversalSite.Near) > TraverseTriggerCm)
		{
			return;
		}
		// The dry run, then the command (the visible moment of the tell).
		if (!World.CanBeginTraversal(Ordinal, Self, TraversalSite) || World.GetTellSampler().IsInShownGap(Ordinal, Tell, Now, Tuning))
		{
			BlockedTraversals.Add(TraversalSite.PointId, Now + Tuning.CrowdRetryBlockSec);
			if (Observer != nullptr)
			{
				Observer->OnTraversalRefused(Ordinal, Now, TraversalSite.PointId);
			}
			// The tell waits for another coping (the refused point is blocked for CrowdRetryBlockSec).
			RequeueInProgress(Tell);
			Mode = EHerringMode::None;
			Follower.Stop();
			return;
		}
		FTraversalHint Hint;
		Hint.TraversalPointHint = TraversalSite.PointId;
		Hint.ClientTimestamp = static_cast<float>(Now);
		Output.Traverse(Hint);
		TryPerformTell(Tell, Now, World);
		bTraversalSent = true;
		TraversalStarted = Now;
		Mode = EHerringMode::Traversing;
		if (Observer != nullptr)
		{
			Observer->OnTraverse(Ordinal, Now, TraversalSite, bTraversalClimb);
		}
		return;
	}
	// Traversing: the shared SCL state moves the pawn; the leg resumes past the coping when it ends.
	const bool bInTraversal = World.IsTraversing(Ordinal);
	if (!bInTraversal && Now - TraversalStarted > CrowdTraverseLocal::TraversalStartTimeoutSec)
	{
		Mode = EHerringMode::None;
		if (Follower.IsOnLeg())
		{
			// Walk on from where the landing put the pawn to the landing run-out.
			Follower.ReplacePath(Self.Location, MakeArrayView(&TraversalTail.Last(), 1));
		}
	}
}
