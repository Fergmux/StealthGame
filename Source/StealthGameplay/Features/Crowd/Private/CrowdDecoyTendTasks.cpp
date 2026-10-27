// Story cnb-020 (T3 decoy tending red herring; GDD CNB-R7 decoy bullet, R16 T3, AC-CNB-14; ADR-0020). A due decoy tend
// draws a court uniformly over ALL courts (live, decoy, used or not, currently tended), never weighted by anything the
// crowd cannot see; the NPC takes a free stand point and draws again only when every stand point of the drawn court is
// physically occupied. At the stand point it dry-runs Interact, then sends Interact(Begin) through the sink; a Begin the
// fixture refused (NotNow: a fifth Begin at a full fixture) abandons that court and a different court is drawn, as
// after any rejection (CNB-R3). CrowdDecoyCompletePct of holds last exactly ObjectiveInteractSec (one value for every
// court, read from the Objectives owner); the rest release at U(CrowdDecoyEarlyReleaseMinSec, MaxSec).

#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"
#include "View/StealthTuningPublicSubstructs.h"

bool FCrowdAgent::ChooseCourt(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, int32 ExcludedCourt)
{
	const int32 Courts = World.GetCourtCount();
	if (ExcludedCourt == INDEX_NONE && MustDeferTell(ECrowdTell::DecoyTend, Now, World))
	{
		return false; // waits until no different tell was shown within the gap
	}
	if (Courts <= 0 || (ExcludedCourt != INDEX_NONE && Courts < 2))
	{
		Mode = EHerringMode::None;
		return false;
	}
	FCrowdDraws& Draws = World.GetDraws();
	// Uniform over every court; a court with every stand point taken (physical space only) is drawn past.
	for (int32 Attempt = 0; Attempt < 4 * Courts; ++Attempt)
	{
		const int32 Court = static_cast<int32>(Draws.DrawBelow(Ordinal, ECrowdDecision::DecoyCourt, static_cast<uint32>(Courts)));
		if (Court == ExcludedCourt)
		{
			continue;
		}
		FCrowdCourtSite Site;
		if (!World.GetCourtStandPoint(Court, Self.Location, Site))
		{
			continue;
		}
		DecoySite = Site;
		if (Observer != nullptr)
		{
			Observer->OnDecoyCourtChosen(Ordinal, Now, Court, ExcludedCourt);
		}
		LeaveRestForHerring(Now);
		BeginLegTo(Now, Self, World, Site.StandPoint, false);
		Mode = EHerringMode::DecoyWalk;
		return true;
	}
	Mode = EHerringMode::None;
	return false;
}

void FCrowdAgent::ArriveAtCourt(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	if (World.GetTellSampler().IsInShownGap(Ordinal, ECrowdTell::DecoyTend, Now, Tuning))
	{
		RequeueInProgress(ECrowdTell::DecoyTend);
		Mode = EHerringMode::None;
		BeginPause(Now, Self, World);
		return;
	}
	if (!Output.DryRun(EActionType::Interact, &DecoySite.Fixture))
	{
		// Refused by the dry run (the fixture is full): abandon it and draw a different court.
		if (Observer != nullptr)
		{
			Observer->OnDecoyRefused(Ordinal, Now, DecoySite.Court);
		}
		if (++DecoyAttempts > 3 || !ChooseCourt(Now, Self, World, DecoySite.Court))
		{
			DecoyAttempts = 0;
			World.GetTellSampler().NoteDropped(Ordinal, ECrowdTell::DecoyTend);
			Mode = EHerringMode::None;
			BeginPause(Now, Self, World);
		}
		return;
	}
	Output.InteractBegin(DecoySite.Fixture);
	DecoyBeginAt = Now;
	Mode = EHerringMode::DecoyBeginCheck;
	SetActivity(ECrowdActivity::Tending, World);
}

void FCrowdAgent::TickDecoy(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	using namespace CrowdHerringConstants;
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	if (Mode == EHerringMode::DecoyBeginCheck)
	{
		ECrowdBeginOutcome Outcome = World.GetBeginOutcome(Ordinal, EActionType::Interact, DecoyBeginAt, Now);
		if (Outcome == ECrowdBeginOutcome::Pending && Now - DecoyBeginAt >= BeginOutcomeTimeoutSec)
		{
			Outcome = ECrowdBeginOutcome::Accepted;
		}
		if (Outcome == ECrowdBeginOutcome::Pending)
		{
			return;
		}
		if (Outcome == ECrowdBeginOutcome::Refused)
		{
			// Reject(NotNow): the fixture filled between the dry run and the Begin. A different court.
			if (Observer != nullptr)
			{
				Observer->OnDecoyRefused(Ordinal, Now, DecoySite.Court);
			}
			SetActivity(ECrowdActivity::Routing, World);
			if (++DecoyAttempts > 3 || !ChooseCourt(Now, Self, World, DecoySite.Court))
			{
				DecoyAttempts = 0;
				World.GetTellSampler().NoteDropped(Ordinal, ECrowdTell::DecoyTend);
				Mode = EHerringMode::None;
				BeginPause(Now, Self, World);
			}
			return;
		}
		DecoyAttempts = 0;
		TryPerformTell(ECrowdTell::DecoyTend, Now, World);
		FCrowdDraws& Draws = World.GetDraws();
		const bool bComplete = Draws.DrawChance(Ordinal, ECrowdDecision::DecoyRelease, Tuning.CrowdDecoyCompletePct / 100.0);
		const double Hold = bComplete
			? World.GetActionTimings().ObjectiveInteractSec
			: Draws.DrawUniform(Ordinal, ECrowdDecision::DecoyEarlyRelease, Tuning.CrowdDecoyEarlyReleaseMinSec, Tuning.CrowdDecoyEarlyReleaseMaxSec);
		DecoyHoldEnd = DecoyBeginAt + Hold;
		Mode = EHerringMode::DecoyHold;
		if (Observer != nullptr)
		{
			Observer->OnDecoyHold(Ordinal, Now, DecoySite.Court, Hold, bComplete);
		}
		return;
	}
	if (Mode == EHerringMode::DecoyHold && Now >= DecoyHoldEnd - 1e-9)
	{
		Output.InteractEnd();
		Mode = EHerringMode::None;
		// Walk away from the court (an ordinary leg drawn by the density model).
		BeginLeg(Now, Self, World, -1.0);
	}
}
