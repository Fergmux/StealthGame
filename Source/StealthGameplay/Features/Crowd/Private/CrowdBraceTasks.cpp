// Story cnb-021 (T4 brace and startle red herring; GDD CNB-R7 brace bullet, R16 T4, F2). The T4 clock decides WHEN a
// brace is due; a due brace is performed at the next flinch trigger within 30 s (a runner passing within 2 m, a
// traversal landing within 3 m) or else as an ambient startle when the window ends. A trigger alone never causes a
// brace: only due NPCs scan, so "sprint past and watch for a flinch" finds about 1 in 100 at defaults. The brace is a
// real Prepare(Begin/End) through the sink after the dry run, with the same cooldown and planted stance as a player's;
// CrowdBraceCapSharePct of braces are held to the Prepare cap (read from the cap's owner, never copied), the rest are
// released at U(1.0, 3.8) s. The R8 startle (story cnb-024) uses the same PerformBrace and counts in the same budget.

#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"
#include "View/StealthTuningPublicSubstructs.h"

bool FCrowdAgent::FlinchTriggered(const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World) const
{
	using namespace CrowdHerringConstants;
	// The Director's hash (cost inside Crowd.Perception, scanned by due NPCs only): any perceivable pawn, any kind.
	TArray<const FCrowdPerceivedPawn*> Near;
	World.GetHash().Query(Self.Location, FlinchLandingRadiusCm, ECrowdQuery::Perception, Self.Key, Near);
	for (const FCrowdPerceivedPawn* Other : Near)
	{
		const double Distance = FVector::Dist2D(Other->Location, Self.Location);
		if ((Other->bRunning && Distance <= FlinchRunnerRadiusCm) || (Other->bTraversalLanding && Distance <= FlinchLandingRadiusCm))
		{
			return true;
		}
	}
	return false;
}

bool FCrowdAgent::BraceFitsStretch(double Now, double HoldSec, ICrowdAgentWorld& World) const
{
	// A brace from rest extends the visible stationary stretch: it must end inside CrowdMaxIdleSec (AC-CNB-13).
	return Activity != ECrowdActivity::Pausing || World.GetStationaryAge(Ordinal, Now) + HoldSec < World.GetCrowdTuning().CrowdMaxIdleSec - WatchdogLeadSec;
}

void FCrowdAgent::PerformBrace(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output, bool bTriggered)
{
	using namespace CrowdHerringConstants;
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	const FCrowdActionTimings Timings = World.GetActionTimings();
	if (Now - LastBraceTime < Timings.PrepareCooldownSec)
	{
		// The shared cooldown from activation: the brace cannot happen (the IIF would refuse it too).
		DropPending(ECrowdTell::Brace, Now, World);
		return;
	}
	FCrowdDraws& Draws = World.GetDraws();
	if (BraceHoldPlanned < 0.0)
	{
		// Drawn once per due brace (a brace that waits for the walk keeps its draws).
		bBraceCapPlanned = Draws.DrawChance(Ordinal, ECrowdDecision::BraceCap, Tuning.CrowdBraceCapSharePct / 100.0);
		BraceHoldPlanned = bBraceCapPlanned ? Timings.PrepareCapSec : Draws.DrawUniform(Ordinal, ECrowdDecision::BraceRelease, BraceReleaseMinSec, BraceReleaseMaxSec);
	}
	const bool bCapped = bBraceCapPlanned;
	const double Hold = BraceHoldPlanned;
	if (!BraceFitsStretch(Now, Hold, World))
	{
		// From a long rest the brace would make the stand too long: it waits for the walk.
		return;
	}
	BraceHoldPlanned = -1.0;
	if (!Output.DryRun(EActionType::Prepare, nullptr))
	{
		DropPending(ECrowdTell::Brace, Now, World);
		return;
	}
	if (!TryPerformTell(ECrowdTell::Brace, Now, World, !bBraceIsStartle))
	{
		return;
	}
	bBraceIsStartle = false;
	BraceDueUntil = -1.0;
	Output.PrepareBegin();
	LastBraceTime = Now;
	BraceEnd = Now + Hold;
	BraceResume = Activity;
	bBraceResumeLeg = Activity == ECrowdActivity::Routing && Follower.IsOnLeg() && Follower.GetCorners().Num() > 0;
	BraceResumeGoal = bBraceResumeLeg ? Follower.GetCorners().Last() : Self.Location;
	// The planted stance: the pawn stops through SCL's braking path (the brace pose itself is the shared IIF/SCL pose).
	Follower.Stop();
	Mode = EHerringMode::BraceHold;
	SetActivity(ECrowdActivity::Bracing, World);
	if (Observer != nullptr)
	{
		Observer->OnBrace(Ordinal, Now, bTriggered, Hold, bCapped);
	}
}

void FCrowdAgent::EndBrace(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	Output.PrepareEnd();
	Mode = EHerringMode::None;
	if (BraceResume == ECrowdActivity::Pausing && Now < PauseEnd)
	{
		// Back to the rest it interrupted.
		SetActivity(ECrowdActivity::Pausing, World);
		DueList.Schedule(ECrowdDueKind::PauseEnd, PauseEnd);
		return;
	}
	if (BraceResume == ECrowdActivity::Pausing)
	{
		EndPause(Now);
	}
	if (bBraceResumeLeg)
	{
		// Walk on to where it was going.
		BeginLegTo(Now, Self, World, BraceResumeGoal, true);
		return;
	}
	BeginLeg(Now, Self, World, -1.0);
}
